#pragma once
#include <windows.h>
#include <string>
#include <cstdio>

// Runs cdb.exe to attach to a process, change a thread's RIP, and detach.
// cdbPath  : Full path to portable cdb.exe
// pid      : Target process ID (decimal)
// tid      : Target thread ID (system TID, will be converted to hex for ~~[])
// newRip   : New instruction pointer value
// timeoutMs: Optional timeout (0 = wait indefinitely)
bool ChangeThreadRipViaCdb(
    const std::wstring& cdbPath,
    DWORD pid,
    DWORD tid,
    ULONG64 newRip,
    DWORD timeoutMs = 30000)
{
    // Build the -c script. TID must be hex without 0x prefix.
    wchar_t script[512];
    swprintf_s(script, L"~~[%x]s; r rip=0x%llX; qd",
        tid, (unsigned long long)newRip);

    // Build the full command line.
    wchar_t cmdLine[1024];
    swprintf_s(cmdLine, L"\"%s\" -p %u -c \"%s\"",
        cdbPath.c_str(), pid, script);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // no console window flash

    PROCESS_INFORMATION pi = {};

    // CREATE_NO_WINDOW prevents a console from appearing.
    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine,
        nullptr, nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    if (!ok) {
        wprintf(L"[!] CreateProcess failed. Error = %lu\n", GetLastError());
        return false;
    }

    // Wait for cdb.exe to finish (with optional timeout).
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs ? timeoutMs : INFINITE);

    if (waitResult == WAIT_TIMEOUT) {
        wprintf(L"[!] cdb.exe timed out. Terminating.\n");
        TerminateProcess(pi.hProcess, 1);
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    wprintf(L"[*] cdb.exe exited with code %lu\n", exitCode);
    return (waitResult == WAIT_OBJECT_0);
}