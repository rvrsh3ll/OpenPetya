// OpenPetya.cpp

#include <iostream>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tchar.h>

#pragma comment(lib, "setupapi.dll")

#define SECTOR_SIZE 512
#define MBR_BOOT_CODE_SIZE 446;
#define MBR_FULL_SIZE 512
#define STAGE2_START_SECTOR 1
#define BACKUP_MBR_SECTOR 63;

struct stDriveInfo
{
    int nIndex;
    UINT64 uSizeBytes;
    std::wstring szModel;
    std::wstring szPath;
};

ULONG fnHexdump(const uint8_t* abBuffer, size_t nLength, size_t nOffset = 0)
{
    ULONG nResult = 0;
    for (ULONG i = 0; i < nLength; i += 16)
    {
        printf("%08X |", i);

        nResult += 16;
        for (ULONG j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                nResult += printf(" %02X", abBuffer[i + j]);
            }
            else
            {
                nResult += printf(" 00");
            }
        }

        nResult += printf(" | ");
        for (ULONG j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                UCHAR k = abBuffer[i + j];
                UCHAR c = k < 32 || k > 127 ? '.' : k;
                nResult += printf("%c", c);
            }
            else
            {
                nResult += printf(" ");
            }
        }

        nResult += printf("\n");
    }

    return nResult;
}

class clsDiskHandle
{
public:
    HANDLE m_hFile = INVALID_HANDLE_VALUE;

    clsDiskHandle() = default;

    ~clsDiskHandle()
    {
        if (INVALID_HANDLE_VALUE != m_hFile)
            CloseHandle(m_hFile);
    }

    bool fnbOpen(const std::wstring& szPath, bool bWriteAccess)
    {
        DWORD dwAccess = bWriteAccess ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
        m_hFile = CreateFileW(
            szPath.c_str(),
            bWriteAccess,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
            nullptr
        );

        if (INVALID_HANDLE_VALUE == m_hFile)
        {
            DWORD hError = GetLastError();
            std::wcerr << L"Failed to open " << szPath << L" (error " << hError << L")\n";

            if (ERROR_ACCESS_DENIED == hError)
                std::cerr << " Please Run as Administrator\n";

            return false;
        }

        return true;
    }

    bool fnbReadSectors(LONGLONG nLBA, DWORD nCount, std::vector<uint8_t>& abBuffer)
    {
        LARGE_INTEGER nOffset;
        nOffset.QuadPart = nLBA * SECTOR_SIZE;

        if (!SetFilePointerEx(m_hFile, nOffset, nullptr, FILE_BEGIN))
        {
            std::cerr << "Seek failed (error: " << GetLastError() << ")\n";
            return false;
        }

        abBuffer.resize((size_t)(nCount * SECTOR_SIZE));
        DWORD nRead = 0;
        if (!ReadFile(m_hFile, abBuffer.data(), (DWORD)abBuffer.size(), &nRead, nullptr) || nRead != abBuffer.size())
        {
            std::wcerr << "Read failed (error: " << GetLastError() << ")\n";
            return false;
        }

        return true;
    }

    bool fnbWriteSectors(LONGLONG nLBA, const std::vector<uint8_t>& abBuffer)
    {

    }
};

std::vector<stDriveInfo> fnListDrives()
{
    std::vector<stDriveInfo> lsDrives;

    for (int i = 0; i < 16; i++)
    {
        std::wstring szPath = TEXT("\\\\.\\PhysicalDrive") + std::to_wstring(i);
        HANDLE hFile = CreateFileW(
            szPath.c_str(),
            0, // query only, no read/write
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (INVALID_HANDLE_VALUE == hFile)
            continue;

        stDriveInfo info;
        info.nIndex = i;
        info.szPath = szPath;

        // Get disk size
        DISK_GEOMETRY_EX geo = {};
        DWORD ret = 0;
        if (DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geo, sizeof(geo), &ret, nullptr))
        {
            info.uSizeBytes = (UINT64)geo.DiskSize.QuadPart;
        }

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        uint8_t abDescriptor[512] = {};
        if (DeviceIoControl(hFile, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), abDescriptor, sizeof(abDescriptor), &ret, nullptr))
        {
            auto *descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(abDescriptor);
            if (descriptor->ProductIdOffset)
            {
                const char *szModel = reinterpret_cast<const char*>(abDescriptor) + descriptor->ProductIdOffset;
                int nLength = MultiByteToWideChar(CP_ACP, 0, szModel, -1, nullptr, 0);
                if (nLength > 0)
                {
                    std::wstring s(nLength, 0);
                    MultiByteToWideChar(CP_ACP, 0, szModel, -1, s.data(), nLength);
                    while (!s.empty() && (s.back() == L' ' || s.back() == L'\0'))
                        s.pop_back();

                    info.szModel = s;
                }
            }
        }

        if (info.szModel.empty())
            info.szModel = L"(Unknown model)";

        lsDrives.push_back(info);

        CloseHandle(hFile);
    }

    return lsDrives;
}

void fnPrintDrives(const std::vector<stDriveInfo>& lsDrives)
{
    std::wcout << "\nAvailable physical drives:\n";
    std::wcout << "\t# Size\t\tModel\n";
    std::wcout << "\t------\t\t--------------------\n";
    for (auto& drive : lsDrives)
    {
        double gb = (double)drive.uSizeBytes / (1024 * 1024 * 1024);
        std::wcout << L"\t[" << drive.nIndex << L"]";
        std::cout << std::fixed << std::setprecision(1) << std::setw(6) << gb << " GB\t\t";
        std::wcout << drive.szModel << L"\n";
    }
}

bool fnbReadFile(const std::string& szPath, std::vector<uint8_t>& abBuffer)
{
    std::ifstream fs(szPath, std::ios::binary | std::ios::ate);
    if (!fs)
    {
        std::cerr << "\tCannot open file: " << szPath << "\n";
        return false;
    }

    size_t nSize = (size_t)fs.tellg();
    fs.seekg(0);
    abBuffer.resize(nSize);

    if (!fs.read(reinterpret_cast<char*>(abBuffer.data()), nSize))
    {
        std::cerr << "\tRead error: " << szPath << "\n";
        return false;
    }

    return true;
}

void fnPadToSector(std::vector<uint8_t>& abBuffer)
{

}

bool fnbValidateMBR(const std::vector<uint8_t>& abMBR)
{

}



void fnPrintUsage(const TCHAR* szProg)
{
    std::wcout  << "\nUsage: " << szProg << " [options]\n\n"
                << "Options:\n"
                << "\t--list\n"
                << "\t--backup-mbr <file>\n"

                << "\n";
}

int _tmain(int argc, TCHAR *argv[])
{
    if (argc < 2)
    {
        fnPrintUsage(argv[0]);
        return 1;
    }

    return 0;
}