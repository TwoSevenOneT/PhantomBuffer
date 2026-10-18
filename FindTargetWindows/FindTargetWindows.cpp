#include <windows.h>
#include <iostream>
#include <string>
#include "ProcessHelper.h"

bool IsStandardEditControl(const wchar_t* className) {
    if (className == nullptr) return false;
    return (_wcsicmp(className, L"Edit") == 0);
}

// Helper to get the process executable name from an HWND
std::wstring GetProcessNameAndPid(HWND hwnd, DWORD& outPid) {
    outPid = 0;
    GetWindowThreadProcessId(hwnd, &outPid);

    if (outPid == 0) return L"Unknown Process";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, outPid);
    if (hProcess) {
        wchar_t buffer[MAX_PATH] = { 0 };
        DWORD size = _countof(buffer);

        if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
            std::wstring fullPath(buffer);
            size_t lastSlash = fullPath.find_last_of(L"\\/");
            CloseHandle(hProcess);
            if (lastSlash != std::wstring::npos) {
                return fullPath.substr(lastSlash + 1);
            }
            return fullPath;
        }
        CloseHandle(hProcess);
    }
    return L"System/Protected Process";
}

// Function that handles verifying, printing, and optionally modifying the Edit control
void InspectAndModifyControl(HWND hwnd, HWND topLevelParent) {
    wchar_t className[256] = { 0 };

    if (GetClassNameW(hwnd, className, _countof(className))) {
        // Filter strictly for standard "Edit" controls
        if (IsStandardEditControl(className)) {
            DWORD pid = 0;
            std::wstring processName = GetProcessNameAndPid(topLevelParent, pid);

            wchar_t parentTitle[256] = L"Untitled Window";
            GetWindowTextW(topLevelParent, parentTitle, _countof(parentTitle));

            wchar_t controlText[512] = L"";
            GetWindowTextW(hwnd, controlText, _countof(controlText));

            std::wcout << L"Process: " << processName << L" (PID: " << pid << L")\n"
                << L"Window Title: \"" << parentTitle << L"\"\n"
                << L"  -> Standard Edit HWND: " << hwnd
                << L" | Current Text: \"" << controlText << L"\"\n";

            std::wcout << L"--------------------------------------------------" << std::endl;
        }
    }
}

// Callback for EnumChildWindows (looks inside top-level windows)
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    HWND topLevelParent = (HWND)lParam;
    InspectAndModifyControl(hwnd, topLevelParent);
    return TRUE;
}

// Callback for EnumWindows (loops through all top-level windows)
BOOL CALLBACK EnumTopLevelWindowsProc(HWND hwnd, LPARAM lParam) {
    // 1. Check if the top-level window itself is a naked Edit control
    InspectAndModifyControl(hwnd, hwnd);

    // 2. Drill down into child elements
    EnumChildWindows(hwnd, EnumChildProc, (LPARAM)hwnd);

    return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
    //Running as normal application
    std::wcout << L"\nPhantomBuffer: Send buffers without Writing\n"
        << L"\nGitHub:  https://github.com/TwoSevenOneT/PhantomBuffer\n"
        << L"\nTwo Seven One Three: https://x.com/TwoSevenOneT\n"
        << L"\n==========================================================\n\n";


    if (argc < 2)
    {
        std::wcerr << L"For Enum:\n";
        std::wcerr << L"Usage: PhantomBuffer\\" << argv[0] << L" -list\n";
        std::wcerr << L"For spawn new:\n";
        std::wcerr << L"Usage: PhantomBuffer\\" << argv[0] << L" <FilePath>\n";
        return 1;
    }


    std::wstring input = argv[1];
    if (input == L"-list") {
        std::wcout << L"Scanning system strictly for visible, standard [Edit] controls...\n" << std::endl;
        EnumWindows(EnumTopLevelWindowsProc, 0);
        std::wcout << L"\nScan complete." << std::endl;
    }
    else {
        StartTarget(input);
    }

    return 0;
}
