#pragma once
#include <windows.h>
#include <iostream>
#include <string>

BOOL SendMsg(HWND targetHwnd, BYTE* rawData) {

    if (!IsWindow(targetHwnd)) {
        std::cerr << "Error: HWND is not an active window handle.\n";
        return 1;
    }

    //unsigned char rawData[38] = {
    //    0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x62, 0x62,
    //    0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x63, 0x63, 0x63, 0x63, 0x63,
    //    0xAA, 0xAA, 0xAA, 0xAB, 0x00, 0x0A, 0x0D, 0x1A, 0xCD, 0xDD, 0xDD, 0xDE,
    //    0xEE, 0xEE
    //};


    // WM_SETTEXT ignores wParam (must be 0)
    WPARAM wp = 0;

    // lParam holds the raw pointer addressing our 2-byte wide text structure
    LPARAM lp = (LPARAM)rawData;

    std::cout << "Sending WM_SETTEXT via SendMessageW...\n";

    // 4. Fire the message
    LRESULT result = SendMessageW(targetHwnd, WM_SETTEXT, wp, lp);

    if (result == TRUE) {
        std::cout << "Success: Target control text updated.\n";
    }
    else {
        std::cerr << "Failure: The target window rejected the message.\n";
        return FALSE;
    }

    return TRUE;
}