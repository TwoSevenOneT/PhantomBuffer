#pragma once
#include <windows.h>
#include <iostream>
#include <vector>

BOOL StartTarget(std::wstring commandStr) {

    LPWCH pCurrentEnv = GetEnvironmentStringsW();
    if (!pCurrentEnv) return false;

    // 2. Copy existing environment variables into a vector
    std::vector<wchar_t> newEnv;
    LPWCH pVar = pCurrentEnv;
    while (*pVar) {
        size_t len = wcslen(pVar) + 1;
        newEnv.insert(newEnv.end(), pVar, pVar + len);
        pVar += len;
    }
    FreeEnvironmentStringsW(pCurrentEnv); // Free the original block

    // 3. Append our override variable: "__COMPAT_LAYER=RunAsInvoker\0"
    const wchar_t* compatLayer = L"__COMPAT_LAYER=RunAsInvoker";
    size_t compatLen = wcslen(compatLayer) + 1;
    newEnv.insert(newEnv.end(), compatLayer, compatLayer + compatLen);

    // 4. Append the final null terminator to mark the end of the block
    newEnv.push_back(L'\0');

    // Convert to a vector to ensure a mutable, null-terminated wchar_t array
    std::vector<wchar_t> cmdLineBuffer(commandStr.begin(), commandStr.end());
    cmdLineBuffer.push_back(L'\0'); // Explicitly append null-terminator

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 4. Configure the STARTUPINFO structure to hide the window
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wcout << L"Launching hidden process: " << commandStr << std::endl;

    // 5. Call CreateProcessW using the mutable buffer pointer
    BOOL success = CreateProcessW(
        NULL,                   // No module name (use command line)
        cmdLineBuffer.data(),   // Pointer to mutable wide string buffer
        NULL,                   // Process handle not inheritable
        NULL,                   // Thread handle not inheritable
        FALSE,                  // Set handle inheritance to FALSE
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,       // Process creation flags (for console applications)
        newEnv.data(),                   // Use parent's environment block
        NULL,                   // Use parent's starting directory 
        &si,                    // Pointer to STARTUPINFO structure
        &pi                     // Pointer to PROCESS_INFORMATION structure
    );

    if (!success) {
        std::cerr << "CreateProcess failed. Error code: " << GetLastError() << std::endl;
        return FALSE;
    }
    std::wcout << L"Process created successfully! ID: " << pi.dwProcessId << std::endl;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return TRUE;
}
