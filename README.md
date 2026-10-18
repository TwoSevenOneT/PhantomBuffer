# PhantomBuffer

**PhantomBuffer** EDR evasion uses stealth injection via SendMessage with WM_SETTEXT to effective bypass EDR (Endpoint Detection and Response).

Unlike traditional edr bypass techniques, **it does not use the VirtualAllocEx and WriteProcessMemory APIs**.

### Command Line Syntax

**PhantomBuffer.exe `<PID`> `<HWND: 0xXXXXXXXX`> `<SC_FilePath`> `<CDB.exe Path`>**

**PID**: target PID
**HWND: 0xXXXXXXXX**: target Windows Handle
**SC_FilePath**: base64 payload/shellcode. Sample payload: 8 bytes 0xCC. **Bad char: 0x00**
**<CDB.exe Path**: path to cdb.exe

To find a suitable target or spawn a target process yourself to bypass edr, use **“FindTargetWindows.exe.”** 

On Windows 11, use `C:\Windows\regedit.exe`; on Windows 10, use `Notepad.exe`.


## Detailed article

[PhantomBuffer: Effective EDR Evasion Through Stealth Injection](https://www.zerosalarium.com/2026/10/PhantomBuffer-Effective-EDR-Evasion-Stealth-Injection.html)


## Demo Video

Youtube: [https://youtu.be/2UmMFXyZ2OU](https://youtu.be/2UmMFXyZ2OU)


## 🐦 Enjoying my work? Support the journey by following me on X

[![Twitter Follow](https://img.shields.io/twitter/follow/TwoSevenOneT?style=for-the-badge&logo=x&color=000)](https://x.com/TwoSevenOneT)

## Author:

[Two Seven One Three](https://x.com/TwoSevenOneT)
