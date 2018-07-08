#include "stdafx.h"
#include "Game.hpp"
#include "Sys.hpp"
#include "VGA.hpp"
#include "bmp.hpp"
#include "Input.hpp"
#include "Error.hpp"
#include "Psx.hpp"
#include "DynamicArray.hpp"
#include "Sound.hpp"
#include "Function.hpp"
#include "ResourceManager.hpp"
#include "PsxDisplay.hpp"
#include "Map.hpp"
#include "ScreenManager.hpp"
#include "Animation.hpp"
#include "stdlib.hpp"
#include "PauseMenu.hpp"
#include <timeapi.h>
#include "GameSpeak.hpp"
#include "PathData.hpp"
#include "DDCheat.hpp"
#include "Quicksave.hpp"
#include <atomic>
#include <fstream>

void Game_ForceLink() { }

using TExitGameCallBack = std::add_pointer<void CC()>::type;

ALIVE_VAR(1, 0xBBFB00, TExitGameCallBack, sGame_OnExitCallback_BBFB00, nullptr);

ALIVE_VAR(1, 0x5C1B84, unsigned int, sGnFrame_5C1B84, 0);

// Timer
ALIVE_VAR(1, 0xBBB9D4, DWORD, sTimer_period_BBB9D4, 0);

// I/O
ALIVE_VAR(1, 0xBD2A5C, BOOL, sIOSyncReads_BD2A5C, FALSE);
ALIVE_VAR(1, 0xBBC558, DWORD, sIoThreadId_BBC558, 0);
ALIVE_VAR(1, 0xBBC55C, HANDLE, sIoThreadHandle_BBC55C, nullptr);


EXPORT bool CC Is_Cd_Rom_Drive_495470(CHAR driveLetter)
{
    CHAR RootPathName[4] = {};
    strcpy(RootPathName, "C:\\");
    RootPathName[0] = driveLetter;
    return ::GetDriveTypeA(RootPathName) == DRIVE_CDROM;
}


EXPORT int CC Game_End_Frame_4950F0(float fps)
{
    NOT_IMPLEMENTED();
}

EXPORT void CC sub_496720()
{
    NOT_IMPLEMENTED();
}

EXPORT void IO_Init_494230()
{
    NOT_IMPLEMENTED();
}

struct IO_Handle
{
    int field_0_flags;
    int field_4;
    FILE *field_8_hFile;
    int field_C_last_api_result;
    std::atomic<bool> field_10_bDone; // Note: OG bug - appears to be no thread sync on this
    int field_14;
    int field_18;
};
ALIVE_ASSERT_SIZEOF(IO_Handle, 0x1C);

EXPORT IO_Handle* CC IO_Open_4F2320(const char* fileName, int modeFlag)
{
    IO_Handle* pHandle = reinterpret_cast<IO_Handle*>(malloc_4F4E60(sizeof(IO_Handle)));
    if (!pHandle)
    {
        return nullptr;
    }

    memset(pHandle, 0, sizeof(IO_Handle));

    const char* mode = nullptr;
    if ((modeFlag & 3) == 3)
    {
        mode = "rwb";
    }
    else if (modeFlag & 1)
    {
        mode = "rb";
    }
    else
    {
        mode = "wb";
        if (!(modeFlag & 2))
        {
            // Somehow it can also be passed as string?? I don't think this case ever happens
            mode = (const char *)modeFlag;
        }
    }

    pHandle->field_8_hFile = fopen(fileName, mode);
    if (pHandle->field_8_hFile)
    {
        pHandle->field_0_flags = modeFlag;
        return pHandle;
    }
    else
    {
        mem_free_4F4EA0(pHandle);
        return nullptr;
    }
}

EXPORT void CC IO_WaitForComplete_4F2510(IO_Handle* hFile)
{
    if (hFile && hFile->field_10_bDone)
    {
        do
        {
            Sleep(0);
        }
        while (hFile->field_10_bDone);
    }
}

EXPORT int CC IO_Seek_4F2490(IO_Handle* hFile, int offset, int origin)
{
    if (!hFile)
    {
        return 0;
    }

    if (origin != 1 && origin != 2)
    {
        origin = 0;
    }
    return fseek(hFile->field_8_hFile, offset, origin);
}

EXPORT void CC IO_fclose_4F24E0(IO_Handle* hFile)
{
    if (hFile)
    {
        IO_WaitForComplete_4F2510(hFile);
        fclose(hFile->field_8_hFile);
        mem_free_4F4EA0(hFile);
    }
}

ALIVE_VAR(1, 0xBBC4BC, std::atomic<IO_Handle*>, sIOHandle_BBC4BC, {});
ALIVE_VAR(1, 0xBBC4C4, std::atomic<void*>, sIO_ReadBuffer_BBC4C4, {});
ALIVE_VAR(1, 0xBBC4C0, std::atomic<int>, sIO_Thread_Operation_BBC4C0, {});
ALIVE_VAR(1, 0xBBC4C8, std::atomic<size_t>, sIO_BytesToRead_BBC4C8, {});

EXPORT DWORD WINAPI FS_IOThread_4F25A0(LPVOID lpThreadParameter)
{
    while (1)
    {
        while (1)
        {
            // Wait for a control message
            MSG msg = {};
            do
            {
                while (GetMessageA(&msg, 0, 0x400u, 0x400u) == -1)
                {

                }

            } while (msg.wParam != 4444 || msg.lParam != 5555 || sIO_Thread_Operation_BBC4C0 != 3);

            if (sIOHandle_BBC4BC.load())
            {
                break;
            }

            sIO_Thread_Operation_BBC4C0 = 0;
        }

        if (fread(sIO_ReadBuffer_BBC4C4, 1u, sIO_BytesToRead_BBC4C8, sIOHandle_BBC4BC.load()->field_8_hFile) == sIO_BytesToRead_BBC4C8)
        {
            sIOHandle_BBC4BC.load()->field_C_last_api_result = 0;
        }
        else
        {
            sIOHandle_BBC4BC.load()->field_C_last_api_result = -1;
        }

        sIOHandle_BBC4BC.load()->field_10_bDone = false;
        sIOHandle_BBC4BC = nullptr;
        sIO_Thread_Operation_BBC4C0 = 0;
    }
}

EXPORT signed int CC IO_Issue_ASync_Read_4F2430(IO_Handle *hFile, int always3, void* readBuffer, size_t bytesToRead, int /*notUsed1*/, int /*notUsed2*/, int /*notUsed3*/)
{
    if (sIOHandle_BBC4BC.load())
    {
        return -1;
    }

    hFile->field_10_bDone = true;

    // TODO: Should be made atomic for thread safety.
    sIOHandle_BBC4BC = hFile;
    sIO_ReadBuffer_BBC4C4 = readBuffer;
    sIO_Thread_Operation_BBC4C0 = always3;
    sIO_BytesToRead_BBC4C8 = bytesToRead;
    //sIO_NotUsed1_dword_BBC4CC = notUsed1;
    //sIO_NotUsed2_dword_BBC4D0 = notUsed2;
    //sIO_NotUsed3_dword_BBC4D4 = notUsed3;
    return 0;
}

EXPORT int CC IO_Read_4F23A0(IO_Handle* hFile, void* pBuffer, size_t bytesCount)
{
    if (!hFile || !hFile->field_8_hFile)
    {
        return -1;
    }

    if (hFile->field_0_flags & 4) // ASync flag
    {
        IO_WaitForComplete_4F2510(hFile);
        IO_Issue_ASync_Read_4F2430(hFile, 3, pBuffer, bytesCount, 0, 0, 0);
        ::PostThreadMessageA(sIoThreadId_BBC558, 0x400u, 0x115Cu, 5555); // TODO: Add constants
        return 0;
    }
    else
    {
        if (fread(pBuffer, 1u, bytesCount, hFile->field_8_hFile) == bytesCount)
        {
            hFile->field_C_last_api_result = 0;
        }
        else
        {
            hFile->field_C_last_api_result = -1;
        }

        return hFile->field_C_last_api_result;
    }
}

ALIVE_ARY(1, 0x5CA488, char, 30, sCdRomDrives_5CA488, {});
ALIVE_VAR(1, 0x5CA4D4, int, dword_5CA4D4, 0);
ALIVE_VAR(1, 0x55EF90, int, dword_55EF90, 1);
ALIVE_VAR(1, 0x55EF88, bool, byte_55EF88, true);
ALIVE_VAR(1, 0x5CA4D0, bool, sCommandLine_ShowFps_5CA4D0, false);
ALIVE_VAR(1, 0x5CA4B5, bool, sCommandLine_DDCheatEnabled_5CA4B5, false);
ALIVE_VAR(1, 0x5CA4D2, bool, byte_5CA4D2, false);
ALIVE_VAR(1, 0x5CA4E0, int, dword_5CA4E0, 0);

EXPORT void CC sub_4ED960(int a1)
{
    NOT_IMPLEMENTED();
}

EXPORT void CC Main_ParseCommandLineArguments_494EA0(const char* /*pCmdLineNotUsed*/, const char* pCommandLine)
{
    //nullsub_2(); // Note: Pruned
    IO_Init_494230();

    // Default the CD drive to C:
    char strDrive[3] = {};
    strcpy(strDrive, "C:");

    // Collect up all CD rom drives
    int pos = 0;
    for (char drive = 'D'; drive < 'Z'; drive++)
    {
        if (Is_Cd_Rom_Drive_495470(drive))
        {
            sCdRomDrives_5CA488[pos++] = drive;
        }
    }

    // Use the first found CD ROM drive as the game location
    if (sCdRomDrives_5CA488[0])
    {
        strDrive[0] = sCdRomDrives_5CA488[0];
    }

    PSX_EMU_Set_Cd_Emulation_Paths_4FAA70(".", strDrive, strDrive);
    Sys_WindowClass_Register_4EE22F("ABE_WINCLASS", "Oddworld Abe's Exoddus", 32, 64, 640, 480);
    Sys_Set_Hwnd_4F2C50(Sys_GetWindowHandle_4EE180());

    dword_5CA4D4 = 0;
    dword_55EF90 = 1;
    byte_55EF88 = true;

    if (pCommandLine)
    {
        if (strstr(pCommandLine, "-ddfps"))
        {
            sCommandLine_ShowFps_5CA4D0 = true;
        }

        if (strstr(pCommandLine, "-ddnoskip"))
        {
            sCommandLine_NoFrameSkip_5CA4D1 = true;
        }

        if (strstr(pCommandLine, "-ddfast"))
        {
            byte_5CA4D2 = true;
            dword_5CA4D4 = 1;
            byte_55EF88 = false;
            dword_5CA4E0 = 2;
        }

        if (strstr(pCommandLine, "-ddfastest"))
        {
            dword_5CA4E0 = 1;
        }

        if (strstr(pCommandLine, "-ddcheat"))
        {
            sCommandLine_DDCheatEnabled_5CA4B5 = true;
        }
    }

    if (dword_5CA4E0 == 1)
    {
        sub_4ED960(1);
        PSX_EMU_Set_screen_mode_4F9420(1);
    }
    else if (dword_5CA4E0 == 2)
    {
        sub_4ED960(0);
        PSX_EMU_Set_screen_mode_4F9420(0);
    }
    else
    {
        sub_4ED960(2);
        PSX_EMU_Set_screen_mode_4F9420(2);
    }

    Init_VGA_AndPsxVram_494690();
    PSX_EMU_Init_4F9CD0(false);
    PSX_EMU_VideoAlloc_4F9D70();
    PSX_EMU_SetCallBack_4F9430(1, Game_End_Frame_4950F0);
    //Main_Set_HWND_4F9410(Sys_GetWindowHandle_4EE180()); // Note: Set but never read
    sub_496720();
}

EXPORT LRESULT CC Sys_WindowMessageHandler_494A40(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    NOT_IMPLEMENTED();
    return 0;
}

EXPORT int CC CreateTimer_4EDEC0(UINT /*uDelay*/, void* /*callBack*/)
{
    NOT_IMPLEMENTED();
    return 0;
}

class FG1 : public BaseGameObject
{
public:
    // TODO
};

ALIVE_VAR(1, 0x5C1A24, DynamicArrayT<Animation>*, gObjList_animations_5C1A24, nullptr);
ALIVE_VAR(1, 0x5C1124, DynamicArrayT<BaseGameObject>*, gObjList_drawables_5C1124, nullptr);
ALIVE_VAR(1, 0x5D1E28, DynamicArrayT<FG1>*, gFG1List_5D1E28, nullptr);

EXPORT void CC Init_Sound_DynamicArrays_And_Others_43BDB0()
{
    NOT_IMPLEMENTED();
}

EXPORT void __stdcall sub_45F000(int )
{
    NOT_IMPLEMENTED();
}

EXPORT void CC sub_494580()
{
    NOT_IMPLEMENTED();
}

ALIVE_VAR(1, 0x5C2F6C, DWORD, dword_5C2F6C, 0);
ALIVE_VAR(1, 0x5C1BA0, WORD, word_5C1BA0, 0);
ALIVE_VAR(1, 0x5C2F70, DWORD, dword_5C2F70, 0);

struct LvlFileRecord
{
    char field_0_file_name[12];
    int field_C_start_sector;
    int field_10_num_sectors;
    int field_14_file_size;
};

struct LvlHeader_Sub
{
    int field_0_num_files;
    int field_4_unknown1;
    int field_8_unknown2;
    int field_C_unknown3;
    LvlFileRecord field_10_file_recs[1]; // TODO: Strictly UB on >= 1 access
};

struct LvlHeader
{
    int field_0_first_file_offset;
    int field_4_ref_count;
    int field_8_magic;
    int field_C_id;
    LvlHeader_Sub field_10_sub;
};

EXPORT int CC File_pc_open_4FA2C0(const char* fileName, int mode)
{
    NOT_IMPLEMENTED();
    return 0;
}

EXPORT int CC File_seek_4FA490(int hFile, int distance, int method)
{
    NOT_IMPLEMENTED();
    return 0;
}

EXPORT int CC File_close_4FA530(int hFile)
{
    NOT_IMPLEMENTED();
    return 0;
}

EXPORT void CC PSX_CD_Normalize_FileName_4FAD90(char* pNormalized, const char* pFileName)
{
    NOT_IMPLEMENTED();
}

ALIVE_ARY(1, 0xC14620, char, 128, sCdEmu_Path1_C14620, {});
ALIVE_ARY(1, 0xC144C0, char, 128, sCdEmu_Path2_C144C0, {});
ALIVE_ARY(1, 0xC145A0, char, 128, sCdEmu_Path3_C145A0, {});

ALIVE_VAR(1, 0xBD1CC4, IO_Handle*, sCdFileHandle_BD1CC4, nullptr);
ALIVE_VAR(1, 0xBD1894, int, sCdReadPos_BD1894, 0);

EXPORT int CC PSX_CD_OpenFile_4FAE80(const char* pFileName, int bTryAllPaths)
{
    static char sLastOpenedFileName_BD1898[1024] = {};

    char pNormalizedName[256] = {};
    char fullFilePath[1024] = {};

    if (_strcmpi(sLastOpenedFileName_BD1898, pFileName) != 0)
    {
        PSX_CD_Normalize_FileName_4FAD90(pNormalizedName, (*pFileName == '\\') ? pFileName + 1 : pFileName);
        //dword_578D5C = -1; // Note: Never read
        sCdReadPos_BD1894 = 0;
        
        int openMode = 1;
        if (!sIOSyncReads_BD2A5C)
        {
            openMode = 5;
        }

        strcpy(fullFilePath, sCdEmu_Path1_C14620);
        strcat(fullFilePath, pNormalizedName);
        if (bTryAllPaths)
        {
            // Close any existing open file
            if (sCdFileHandle_BD1CC4)
            {
                IO_fclose_4F24E0(sCdFileHandle_BD1CC4);
                sLastOpenedFileName_BD1898[0] = 0;
            }

            // Try to open from path 1
            sCdFileHandle_BD1CC4 = IO_Open_4F2320(fullFilePath, openMode);
            if (!sCdFileHandle_BD1CC4)
            {
                // Failed, try path 2
                strcpy(fullFilePath, sCdEmu_Path2_C144C0);
                strcat(fullFilePath, pNormalizedName);
                sCdFileHandle_BD1CC4 = IO_Open_4F2320(fullFilePath, openMode);
                if (!sCdFileHandle_BD1CC4)
                {
                    // Failed try path 3 - each CD drive in the system
                    strcpy(fullFilePath, sCdEmu_Path3_C145A0);
                    strcat(fullFilePath, pNormalizedName);

                    // Oops, we don't have any CD-ROM drives
                    if (!sCdRomDrives_5CA488[0])
                    {
                        return 0;
                    }

                    char* pCdRomDrivesIter = sCdRomDrives_5CA488;
                    while (*pCdRomDrivesIter)
                    {
                        fullFilePath[0] = *pCdRomDrivesIter;
                        if (*pCdRomDrivesIter != sCdEmu_Path2_C144C0[0])
                        {
                            sCdFileHandle_BD1CC4 = IO_Open_4F2320(fullFilePath, openMode);
                            if (sCdFileHandle_BD1CC4)
                            {
                                // Update the default CD-ROM to try
                                sCdEmu_Path2_C144C0[0] = fullFilePath[0];
                                strcpy(sLastOpenedFileName_BD1898, pFileName);
                                return 1;
                            }
                        }
                        pCdRomDrivesIter++;
                    }
                    return 0;
                }
            }
        }
        else
        {
            // Open the file
            IO_Handle* hFile = IO_Open_4F2320(fullFilePath, openMode);
            if (!hFile)
            {
                return 0;
            }

            // Close the old file and set the current file as the new one
            if (sCdFileHandle_BD1CC4)
            {
                IO_fclose_4F24E0(sCdFileHandle_BD1CC4);
                sLastOpenedFileName_BD1898[0] = 0;
            }
            sCdFileHandle_BD1CC4 = hFile;
        }
        strcpy(sLastOpenedFileName_BD1898, pFileName);
    }
    return 1;
}

struct CdlLOC
{
    unsigned __int8 field_0_minute;
    unsigned __int8 field_1_second;
    unsigned __int8 field_2_sector;
    char field_3_track;
};
ALIVE_ASSERT_SIZEOF(CdlLOC, 0x4);

EXPORT CdlLOC* CC PSX_Pos_To_CdLoc_4FADD0(int pos, CdlLOC* pLoc)
{
    pLoc->field_3_track = 0;
    pLoc->field_0_minute = static_cast<BYTE>(pos / 75 / 60 + 2);
    pLoc->field_1_second = pos / 75 % 60;
    pLoc->field_2_sector = static_cast<BYTE>(pos + 108 * (pos / 75 / 60) - 75 * (pos / 75 % 60));
    return pLoc;
}

EXPORT int CC PSX_CdLoc_To_Pos_4FAE40(const CdlLOC* pLoc)
{
    int min = pLoc->field_0_minute;
    if (min < 2)
    {
        min = 2;
    }
    return pLoc->field_2_sector + 75 * (pLoc->field_1_second + 20 * (3 * min - 6));
}

EXPORT int CC PSX_CD_File_Seek_4FB1E0(char mode, const CdlLOC* pLoc)
{
    if (mode != 2)
    {
        return 0;
    }
    sCdReadPos_BD1894 = PSX_CdLoc_To_Pos_4FAE40(pLoc);
    return 1;
}

EXPORT int CC PSX_CD_File_Read_4FB210(int numSectors, void* pBuffer)
{
    IO_Seek_4F2490(sCdFileHandle_BD1CC4, sCdReadPos_BD1894 << 11, 0);
    IO_Read_4F23A0(sCdFileHandle_BD1CC4, pBuffer, numSectors << 11);
    sCdReadPos_BD1894 += numSectors;
    return 1;
}

EXPORT int CC PSX_CD_FileIOWait_4FB260(int bASync)
{
    if (!sCdFileHandle_BD1CC4)
    {
        return -1;
    }

    if (!bASync)
    {
        IO_WaitForComplete_4F2510(sCdFileHandle_BD1CC4);
    }
    return sCdFileHandle_BD1CC4->field_10_bDone != 0;
}

class LvlArchive
{
public:
    EXPORT int Open_Archive_432E80(const char* fileName);
    EXPORT LvlFileRecord* Find_File_Record_433160(const char* pFileName);
    EXPORT int Read_File_433070(const char* pFileName, void* pBuffer);
    EXPORT int Read_File_4330A0(LvlFileRecord* hFile, void* pBuffer);
    EXPORT int Free_433130();
private:
    ResourceManager::Handle<LvlHeader_Sub*> field_0_0x2800_res;
    DWORD field_4[41]; // TODO: Maybe is just 1 DWORD
};
ALIVE_ASSERT_SIZEOF(LvlArchive, 0xA8);

int LvlArchive::Read_File_433070(const char* pFileName, void* pBuffer)
{
    return Read_File_4330A0(Find_File_Record_433160(pFileName), pBuffer);
}

int LvlArchive::Read_File_4330A0(LvlFileRecord* hFile, void* pBuffer)
{
    if (!hFile || !pBuffer)
    {
        return 0;
    }

    CdlLOC cdLoc = {};
    PSX_Pos_To_CdLoc_4FADD0(field_4[0] + hFile->field_C_start_sector, &cdLoc);
    PSX_CD_File_Seek_4FB1E0(2, &cdLoc);

    int bOK = PSX_CD_File_Read_4FB210(hFile->field_10_num_sectors, pBuffer);
    if (PSX_CD_FileIOWait_4FB260(0) == -1)
    {
        bOK = 0;
    }
    return bOK;
}

int LvlArchive::Free_433130()
{
    // Strangely the emulated CD file isn't closed, but the next CD open file will close it anyway..
    if (field_0_0x2800_res.Valid())
    {
        ResourceManager::FreeResource_49C330(field_0_0x2800_res);
        field_0_0x2800_res.Clear();
    }
    return 0;
}

const static int kSectorSize = 2048;

int LvlArchive::Open_Archive_432E80(const char* fileName)
{
    // Allocate space for LVL archive header
    field_0_0x2800_res = ResourceManager::Allocate_New_Block_49BFB0_T<LvlHeader_Sub*>(kSectorSize * 5, 0);

    // Open the LVL file
    int hFile = PSX_CD_OpenFile_4FAE80(fileName, 1);
    if (!hFile)
    {
        return 0;
    }

    // Not sure why any of this is required, doesn't really do anything.
    CdlLOC cdLoc = {};
    PSX_Pos_To_CdLoc_4FADD0(0, &cdLoc);
    field_4[0] = PSX_CdLoc_To_Pos_4FAE40(&cdLoc);
    PSX_CD_File_Seek_4FB1E0(2, &cdLoc);

    // Read the header
    ResourceManager::Header* pResHeader = field_0_0x2800_res.GetHeader();
    int bOk = PSX_CD_File_Read_4FB210(5, pResHeader);
    if (PSX_CD_FileIOWait_4FB260(0) == -1)
    {
        bOk = 0;
    }

    // Set ref count to 1 so ResourceManager won't kill it
    pResHeader->field_4_ref_count = 1;
    return bOk;
}

ALIVE_VAR(1, 0x5CA4B0, BOOL, sbEnable_PCOpen_5CA4B0, FALSE);
ALIVE_VAR(1, 0x5BC218, int, sWrappingFileIdx_5BC218, 0);
ALIVE_VAR(1, 0x551D28, int, sTotalOpenedFilesCount_551D28, 3); // Starts at 3.. for some reason
ALIVE_ARY(1, 0x5BC220, LvlFileRecord, 32, sOpenFileNames_5BC220, {});

LvlFileRecord* LvlArchive::Find_File_Record_433160(const char* pFileName)
{
    const unsigned int fileNameLen = strlen(pFileName) + 1;

    const bool notEnoughSpaceForFileExt = (static_cast<signed int>(fileNameLen) - 1) < 4;
    if (notEnoughSpaceForFileExt || _strcmpi(&pFileName[fileNameLen - 5], ".STR") != 0) // Check its not a STR file
    {
        if (sbEnable_PCOpen_5CA4B0)
        {
            const int hFile = File_pc_open_4FA2C0(pFileName, 0);
            if (hFile >= 0)
            {
                const int idx = sWrappingFileIdx_5BC218++ & 31;
                strcpy(sOpenFileNames_5BC220[idx].field_0_file_name, pFileName);
                sOpenFileNames_5BC220[idx].field_C_start_sector = 0;
                sOpenFileNames_5BC220[idx].field_14_file_size = File_seek_4FA490(hFile, 0, 2);
                sOpenFileNames_5BC220[idx].field_10_num_sectors = (unsigned int)(sOpenFileNames_5BC220[idx].field_14_file_size + kSectorSize - 1) >> 11;
                File_close_4FA530(hFile);
                return &sOpenFileNames_5BC220[idx];
            }
            return nullptr;
        }
    }
    else
    {
        ++sTotalOpenedFilesCount_551D28;
    }
    
    if (!field_0_0x2800_res->field_0_num_files)
    {
        return nullptr;
    }

    int fileRecordIndex = 0;
    while (strncmp(field_0_0x2800_res->field_10_file_recs[fileRecordIndex].field_0_file_name, pFileName, _countof(LvlFileRecord::field_0_file_name)))
    {
        fileRecordIndex++;
        if (fileRecordIndex >= field_0_0x2800_res->field_0_num_files)
        {
            return nullptr;
        }
    }
    return &field_0_0x2800_res->field_10_file_recs[fileRecordIndex];
}

ALIVE_VAR(1, 0x5BC520, LvlArchive, sLvlArchive_5BC520, {});


EXPORT void CC DDCheat_Allocate_415320()
{
    alive_new<DDCheat>(); // DDCheat_ctor_4153C0
}

EXPORT void CC Game_Loop_467230();

class CheatController : public BaseGameObject
{
public:
    virtual void VDestructor(signed int flags) override;
    EXPORT void dtor_421C10(signed int flags);

    CheatController();
    EXPORT void ctor_421BD0();
 
    __int16 field_20;
    __int16 field_22;
    int field_24;
    int field_28;
    int field_2C;
    int field_30;
    int field_34;
    int field_38;
    int field_3C;
};
ALIVE_ASSERT_SIZEOF(CheatController, 0x40);

ALIVE_VAR(1, 0x5BC120, CheatController*, pCheatController_5BC120, nullptr);

void CheatController::VDestructor(signed int flags)
{
    dtor_421C10(flags);
}

void CheatController::dtor_421C10(signed int flags)
{
    NOT_IMPLEMENTED();
}


CheatController::CheatController()
{
    ctor_421BD0();
}

void CheatController::ctor_421BD0()
{
    NOT_IMPLEMENTED();
}

EXPORT void CC Game_Init_LoadingIcon_482CD0()
{
    NOT_IMPLEMENTED();
}

EXPORT void CC Game_Free_LoadingIcon_482D40()
{
    NOT_IMPLEMENTED();
}

class MusicController
{
public:
    static EXPORT void CC Shutdown_47FD20();
};

ALIVE_VAR(1, 0x5C3020, MusicController*, pMusicController_5C3020, nullptr);

void CC MusicController::Shutdown_47FD20()
{
    NOT_IMPLEMENTED();
}

ALIVE_VAR(1, 0x5C1B78, DynamicArray*, ObjList_5C1B78, nullptr);
ALIVE_VAR(1, 0x5BD4D8, DynamicArray*, ObjList_5BD4D8, nullptr);
ALIVE_VAR(1, 0x5C1B7C, DynamicArray*, gBaseAliveGameObjects_5C1B7C, nullptr);
ALIVE_VAR(1, 0x5C1B80, DynamicArray*, sShadow_dArray_5C1B80, nullptr);

class Collisions
{
public:
    EXPORT void dtor_4189F0();
private:
    void* field_0_pArray;
    int field_4;
    int field_8;
    int field_C;
};
ALIVE_ASSERT_SIZEOF(Collisions, 0x10);

void Collisions::dtor_4189F0()
{
    Mem_Free_495560(field_0_pArray);
}

ALIVE_VAR(1, 0x5C1128, Collisions*, sCollisions_DArray_5C1128, nullptr);



EXPORT void CC Game_Run_466D40()
{
    // Begin start up
    sub_494580();
    dword_5C2F6C = 6000;
    word_5C1BA0 = 0;
    dword_5C2F70 = 34;
    sub_494580();

    PSX_ResetCallBack_4FAA20();
    gPsxDisplay_5C1130.ctor_41DC30();
    PSX_CdInit_4FB2C0();
    PSX_CdSetDebug_4FB330(0);
    sub_45F000(1); // starts card/pads on psx ver

    gBaseGameObject_list_BB47C4 = alive_new<DynamicArrayT<BaseGameObject>>(50);
    gObjList_drawables_5C1124 = alive_new<DynamicArrayT<BaseGameObject>>(30);
    gFG1List_5D1E28 = alive_new<DynamicArrayT<FG1>>(4);
    gObjList_animations_5C1A24 = alive_new<DynamicArrayT<Animation>>(30);
    pResourceManager_5C1BB0 = alive_new<ResourceManager>();

    Init_Sound_DynamicArrays_And_Others_43BDB0();

    Camera camera; // ctor_480DD0

    sLvlArchive_5BC520.Open_Archive_432E80(CdLvlName(0));
    ResourceManager::LoadResourceFile_49C170("STP01C25.CAM", &camera);

    camera.field_C_pCamRes = ResourceManager::GetLoadedResource_49C2A0(ResourceManager::Resource_Bits, 125, 1u, 0);
    gMap_5C3030.field_28_5C3058 = 0;
    gMap_5C3030.field_24_5C3054 = 0;

    pScreenManager_5BB5F4 = alive_new<ScreenManager>((int)camera.field_C_pCamRes, (int)&gMap_5C3030.field_24_5C3054); // ctor_40E3E0
    pScreenManager_5BB5F4->sub_cam_vlc_40EF60((unsigned __int16 **)camera.field_C_pCamRes);
    pScreenManager_5BB5F4->MoveImage_40EB70();

    sLvlArchive_5BC520.Free_433130();

    camera.dtor_480E00();

    Input_Init_491BC0();
#ifdef DEVELOPER_MODE
    gMap_5C3030.sub_4803F0(0, 1, 1, 0, 0, 0);
#else
    gMap_5C3030.sub_4803F0(0, 1, 25, 0, 0, 0);
#endif
    DDCheat_Allocate_415320();
    pEventSystem_5BC11C = alive_new<GameSpeak>(); // ctor_421820
    pCheatController_5BC120 = alive_new<CheatController>(); // ctor_421BD0
    
    Game_Init_LoadingIcon_482CD0();

#ifdef DEVELOPER_MODE
    // LOAD DEBUG SAVE //
    // If debug.sav exists, load it before game start.
    // Makes debugging in game stuff a lot faster.
    // NOTE: Hold left shift during boot to skip this.
    std::ifstream debugSave("debug.sav");

    if (!debugSave.fail() && GetKeyState(VK_LSHIFT) >= 0)
    {
        debugSave.read((char*)&sActiveQuicksaveData_BAF7F8, sizeof(sActiveQuicksaveData_BAF7F8));
        Quicksave_LoadActive_4C9170();
        debugSave.close();
        if (pPauseMenu_5C9300 == nullptr)
        {
            pPauseMenu_5C9300 = alive_new<PauseMenu>();
            pPauseMenu_5C9300->ctor_48FB80();
            pPauseMenu_5C9300->field_1C_update_delay = 0;
        }
    }
    /////////////////////////
#endif
    
    // Main loop start
    Game_Loop_467230();

    // Shut down start
    Game_Free_LoadingIcon_482D40();
    DDCheat::ClearProperties_415390();
    gMap_5C3030.sub_4804E0();

    if (gObjList_animations_5C1A24)
    {
        gObjList_animations_5C1A24->dtor_40CAD0();
        Mem_Free_495540(gObjList_animations_5C1A24);
    }

    if (gObjList_drawables_5C1124)
    {
        gObjList_drawables_5C1124->dtor_40CAD0();
        Mem_Free_495540(gObjList_drawables_5C1124);
    }

    if (gFG1List_5D1E28)
    {
        gFG1List_5D1E28->dtor_40CAD0();
        Mem_Free_495540(gFG1List_5D1E28);
    }

    if (gBaseGameObject_list_BB47C4)
    {
        gBaseGameObject_list_BB47C4->dtor_40CAD0();
        Mem_Free_495540(gBaseGameObject_list_BB47C4);
    }

    if (ObjList_5C1B78)
    {
        ObjList_5C1B78->dtor_40CAD0();
        Mem_Free_495540(ObjList_5C1B78);
    }

    if (ObjList_5BD4D8)
    {
        ObjList_5BD4D8->dtor_40CAD0();
        Mem_Free_495540(ObjList_5BD4D8);
    }

    if (sShadow_dArray_5C1B80)
    {
        sShadow_dArray_5C1B80->dtor_40CAD0();
        Mem_Free_495540(sShadow_dArray_5C1B80);
    }

    if (gBaseAliveGameObjects_5C1B7C)
    {
        gBaseAliveGameObjects_5C1B7C->dtor_40CAD0();
        Mem_Free_495540(gBaseAliveGameObjects_5C1B7C);
    }

    if (sCollisions_DArray_5C1128)
    {
        sCollisions_DArray_5C1128->dtor_4189F0();
        Mem_Free_495540(sCollisions_DArray_5C1128);
    }

    pMusicController_5C3020 = nullptr; // Note: OG bug - should have been set to nullptr after shutdown call?
    MusicController::Shutdown_47FD20();

    SND_Clear_4CB4B0();
    SND_Shutdown_4CA280();
    PSX_CdControlB_4FB320(8, 0, 0);
    PSX_ResetCallBack_4FAA20();
    PSX_StopCallBack_4FAA30();
    sInputObject_5BD4E0.ShutDown_45F020();
    PSX_ResetGraph_4F8800(0);
}

EXPORT void CC Game_SetExitCallBack_4F2BA0(TExitGameCallBack callBack)
{
    sGame_OnExitCallback_BBFB00 = callBack;
}

EXPORT void CC Game_ExitGame_4954B0()
{
    PSX_EMU_VideoDeAlloc_4FA010();
}

EXPORT void CC IO_Stop_ASync_IO_Thread_4F26B0()
{
    if (sIoThreadHandle_BBC55C)
    {
        ::CloseHandle(sIoThreadHandle_BBC55C);
        sIoThreadHandle_BBC55C = nullptr;
    }
}

EXPORT void CC Game_Shutdown_4F2C30()
{
    if (sGame_OnExitCallback_BBFB00)
    {
        sGame_OnExitCallback_BBFB00();
        sGame_OnExitCallback_BBFB00 = nullptr;
    }

    CreateTimer_4EDEC0(0, nullptr); // Creates a timer that calls a call back which is always null, therefore seems like dead code?
    Input_DisableInput_4EDDC0();
    //SND_MCI_Close_4F0060(nullptr); // TODO: Seems like more dead code because the mci is never set?
    SND_Close_4EFD50(); // TODO: This appears to terminate the game, re-impl function to figure out why
    IO_Stop_ASync_IO_Thread_4F26B0();
    VGA_Shutdown_4F3170();
}

EXPORT signed int TMR_Init_4EDE20()
{
    struct timecaps_tag ptc = {};
    if (::timeGetDevCaps(&ptc, sizeof(timecaps_tag)))
    {
        Error_PushErrorRecord_4F2920("C:\\abe2\\code\\POS\\TMR.C", 25, 0, "TMR_Init: timeGetDevCaps() failed !");
        return -1;
    }

    sTimer_period_BBB9D4 = ptc.wPeriodMin;
    // This makes timers as accurate as possible increasing cpu/power usage as a trade off
    ::timeBeginPeriod(ptc.wPeriodMin);
    return 0;
}

EXPORT signed int CC Init_Input_Timer_And_IO_4F2BF0(bool forceSystemMemorySurfaces)
{
    static bool sbGameShutdownSet_BBC560 = false;
    if (!sbGameShutdownSet_BBC560)
    {
        atexit(Game_Shutdown_4F2C30);
        sbGameShutdownSet_BBC560 = 1;
        gVGA_force_sys_memory_surfaces_BC0BB4 = forceSystemMemorySurfaces;
    }

    Input_EnableInput_4EDDD0();
    Input_InitKeyStateArray_4EDD60();
    TMR_Init_4EDE20();

    if (!sIoThreadHandle_BBC55C)
    {
        sIoThreadHandle_BBC55C = ::CreateThread(
            0,
            0x4000u,
            FS_IOThread_4F25A0,
            0,
            0,
            &sIoThreadId_BBC558);
        if (!sIoThreadHandle_BBC55C)
        {
            return -1;
        }
    }

    if (strstr(Sys_GetCommandLine_4EE176(), "-syncread"))
    {
        sIOSyncReads_BD2A5C = TRUE;
    }
    else
    {
        sIOSyncReads_BD2A5C = FALSE;
    }
    return 0;
}

EXPORT void CC Game_Main_4949F0()
{
    Path_Get_Bly_Record_460F30(0, 0);

    // Inits
    Init_Input_Timer_And_IO_4F2BF0(false);
    
    Main_ParseCommandLineArguments_494EA0(Sys_GetCommandLine_4EE176(), Sys_GetCommandLine_4EE176());
    Game_SetExitCallBack_4F2BA0(Game_ExitGame_4954B0);
    Sys_SetWindowProc_Filter_4EE197(Sys_WindowMessageHandler_494A40);
    
    // Only returns once the engine is shutting down
    Game_Run_466D40();

    if (sGame_OnExitCallback_BBFB00)
    {
        sGame_OnExitCallback_BBFB00();
        sGame_OnExitCallback_BBFB00 = nullptr;
    }

    Game_Shutdown_4F2C30();
}



ALIVE_VAR(1, 0x5C2FE0, short, sBreakGameLoop_5C2FE0, 0);
ALIVE_VAR(1, 0x5C1B66, short, word_5C1B66, 0);
ALIVE_VAR(1, 0x5C2F78, int, dword_5C2F78, 0);
ALIVE_VAR(1, 0x5C2FA0, short, word_5C2FA0, 0);

EXPORT void CC sub_422DA0()
{
    NOT_IMPLEMENTED();
}

EXPORT void CC sub_449A90()
{
    NOT_IMPLEMENTED();
}

ALIVE_VAR(1, 0xBD146C, BYTE, byte_BD146C, 0);

struct PSX_Pos16
{
    short x, y;
};

EXPORT int CC PSX_getTPage_4F60E0(char tp, char abr, int x, __int16 y)
{
    return ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3ff) >> 6) |(((y) & 0x200) << 2));
}

EXPORT int CC PSX_getClut_4F6350(int x, int y)
{
    return (y << 6) | (x >> 4) & 63;
}

#include <gmock/gmock.h>

namespace Test
{
    static void Test_PSX_getTPage_4F60E0()
    {
        ASSERT_EQ(0, PSX_getTPage_4F60E0(0, 0, 0, 0));

        ASSERT_EQ(32, PSX_getTPage_4F60E0(0, 1, 0, 0));
        ASSERT_EQ(64, PSX_getTPage_4F60E0(0, 2, 0, 0));
        ASSERT_EQ(96, PSX_getTPage_4F60E0(0, 3, 0, 0));

        ASSERT_EQ(128, PSX_getTPage_4F60E0(1, 0, 0, 0));
        ASSERT_EQ(256, PSX_getTPage_4F60E0(2, 0, 0, 0));
        ASSERT_EQ(384, PSX_getTPage_4F60E0(3, 0, 0, 0));
  
        ASSERT_EQ(1, PSX_getTPage_4F60E0(0, 0, 64, 0));
        ASSERT_EQ(2, PSX_getTPage_4F60E0(0, 0, 64 * 2, 64));
        ASSERT_EQ(18, PSX_getTPage_4F60E0(0, 0, 64 * 2, 64* 4));

    }

    void GameTests()
    {
        Test_PSX_getTPage_4F60E0();
    }
}

EXPORT void CC Init_SetTPage_4F5B60(Prim_SetTPage* pPrim, int /*notUsed1*/, int /*notUsed2*/, int tpage)
{
    pPrim->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPrim->field_0_header.field_B_code = 0x80u;
    pPrim->field_C_tpage = tpage;
}

EXPORT void CC Init_PrimClipper_4F5B80(Prim_PrimClipper* pPrim, const PSX_RECT* pClipRect)
{
    // The memory layout of this is crazy..
    pPrim->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPrim->field_0_header.field_B_code = 0x81u;
    pPrim->field_C_x = pClipRect->x;
    pPrim->field_E_y = pClipRect->y;
    pPrim->field_0_header.field_4.mRect.w = pClipRect->w;
    pPrim->field_0_header.field_4.mRect.h = pClipRect->h;
}

EXPORT void CC InitType_ScreenOffset_4F5BB0(Prim_ScreenOffset* pPrim, const PSX_Pos16* pOffset)
{
    pPrim->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPrim->field_0_header.field_B_code = 0x82u;
    pPrim->field_C_xoff = pOffset->x;
    pPrim->field_E_yoff = pOffset->y;
}

struct Prim_Sprt
{
    PrimHeader field_0_header;
    short field_C_x0;
    short field_E_y0;
    BYTE field_10_u0;
    BYTE field_11_v0;
    WORD field_12_clut;
    __int16 field_14_w;
    __int16 field_16_h;
};
ALIVE_ASSERT_SIZEOF(Prim_Sprt, 0x18);

EXPORT void CC Sprt_Init_4F8910(Prim_Sprt* pPrim)
{
    pPrim->field_0_header.field_4.mNormal.field_4_num_longs = 4;
    pPrim->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPrim->field_0_header.field_B_code = 0x64;
}

// Note: Inlined everywhere in real game
void PolyF3_Init(Poly_F3* pPoly)
{
    pPoly->field_0_header.field_4.mNormal.field_4_num_longs = 4;
    pPoly->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPoly->field_0_header.field_B_code = 0x20;
}

// Note: Inlined everywhere in real game
void LineG2_Init(Line_G2* pLine)
{
    pLine->field_0_header.field_4.mNormal.field_4_num_longs = 4;
    pLine->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pLine->field_0_header.field_B_code = 0x50;
}

// Note: Inlined everywhere in real game
void LineG4_Init(Line_G4* pLine)
{
    pLine->field_0_header.field_4.mNormal.field_4_num_longs = 9;
    pLine->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pLine->field_0_header.field_B_code = 0x5C;
    pLine->field_28_pad = 0x55555555;
}


EXPORT void CC PolyG3_Init_4F8890(Poly_G3* pPoly)
{
    pPoly->field_0_header.field_4.mNormal.field_4_num_longs = 6;
    pPoly->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPoly->field_0_header.field_B_code = 0x30;
}

EXPORT void CC PolyG4_Init_4F88B0(Poly_G4* pPoly)
{
    pPoly->field_0_header.field_4.mNormal.field_4_num_longs = 8;
    pPoly->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPoly->field_0_header.field_B_code = 0x38;
}

EXPORT void CC PolyF4_Init_4F8830(Poly_F4* pPoly)
{
    pPoly->field_0_header.field_4.mNormal.field_4_num_longs = 5;
    pPoly->field_0_header.field_4.mNormal.field_5_unknown = byte_BD146C;
    pPoly->field_0_header.field_B_code = 0x28;
}

EXPORT void CC Poly_FT4_Get_Rect_409DA0(PSX_RECT* pRect, const Poly_FT4* pPoly)
{
    if ((pPoly->field_0_header.field_B_code & 0xFC) == 0x2C) // TODO: Add helper for this check, also used in DrawOTag?
    {
        pRect->x = pPoly->field_C_x0;
        pRect->y = pPoly->field_E_y0;
        pRect->w = pPoly->field_24_x3;
        pRect->h = pPoly->field_26_y3;
    }
    else
    {
        pRect->h = 0;
        pRect->w = 0;
        pRect->y = 0;
        pRect->x = 0;
    }
}

EXPORT void CC Poly_Set_unknown_4F8A20(PrimHeader* pPrim, int bFlag1)
{
    pPrim->field_4.mNormal.field_5_unknown = byte_BD146C;
    if (bFlag1)
    {
        pPrim->field_B_code = pPrim->field_B_code | 1;
    }
    else
    {
        pPrim->field_B_code = pPrim->field_B_code & ~1;
    }
}

EXPORT void CC Poly_Set_SemiTrans_4F8A60(PrimHeader* pPrim, int bSemiTrans)
{
    pPrim->field_4.mNormal.field_5_unknown = byte_BD146C;
    if (bSemiTrans)
    {
        pPrim->field_B_code = pPrim->field_B_code | 2;
    }
    else
    {
        pPrim->field_B_code = pPrim->field_B_code & ~2;
    }
}

EXPORT void CC OrderingTable_Add_4F8AA0(int** pOt, void* pItem)
{
    NOT_IMPLEMENTED();
}

class RenderTest : public BaseGameObject
{
public:
    RenderTest()
    {
        // Don't hack the vtable else our virtuals won't get called and we can't hack the correct one back since we don't know the address of our vtable.
        DisableVTableHack disableHack;

        BaseGameObject_ctor_4DBFA0(1, 1);

        InitTestRender();
        
        field_6_flags |= BaseGameObject::eDrawable;

        gObjList_drawables_5C1124->Push_Back(this);
    }

    virtual void VDestructor(signed int flags) override
    {
        Destruct();
        if (flags & 1)
        {
            Mem_Free_495540(this);
        }
    }

    virtual void VRender(int** pOrderingTable) override
    {
        OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mPolyG3.field_0_header.field_0_tag); // 30 being the "layer"
        OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mPolyF4.field_0_header.field_0_tag);
        OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mPolyG4.field_0_header.field_0_tag);

        static PSX_Pos16 xy = {};
        static short ypos = 0;
        ypos++;
        if (ypos > 30)
        {
            ypos = 0;
        }
        xy.x = ypos;
        xy.y = ypos;
        InitType_ScreenOffset_4F5BB0(&mScreenOffset, &xy);
        OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mScreenOffset.field_0_header.field_0_tag);

        static PSX_RECT clipRect = {};
        clipRect.x = 80;
        clipRect.y = 50;
        clipRect.w = 640 - 300;
        clipRect.h = 480 - 200;

        Init_PrimClipper_4F5B80(&mPrimClipper, &clipRect);
       // OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mPrimClipper.field_0_header.field_0_tag);

       OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mLineG2.field_0_header.field_0_tag);

       OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mPolyF3.field_0_header.field_0_tag);

       OrderingTable_Add_4F8AA0(&pOrderingTable[30], &mLineG4.field_0_header.field_0_tag);

       
    }

    void Destruct()
    {
        gObjList_drawables_5C1124->Remove_Item(this);
    }

private:
    void InitTestRender()
    {
        {
            PolyG3_Init_4F8890(&mPolyG3);

            mPolyG3.field_0_header.field_8_r0 = 255;
            mPolyG3.field_0_header.field_9_g0 = 255;
            mPolyG3.field_0_header.field_A_b0 = 255;
            mPolyG3.field_C_x0 = 50;
            mPolyG3.field_E_y0 = 50;

            mPolyG3.field_10_r1 = 0;
            mPolyG3.field_11_g1 = 0;
            mPolyG3.field_12_b1 = 255;
            mPolyG3.field_14_x1 = 200;
            mPolyG3.field_16_y1 = 50;

            mPolyG3.field_18_r2 = 255;
            mPolyG3.field_19_g2 = 0;
            mPolyG3.field_1A_b2 = 255;
            mPolyG3.field_1C_x2 = 150;
            mPolyG3.field_1E_y2 = 100;
        }

        {
            PolyF3_Init(&mPolyF3);

            mPolyF3.field_0_header.field_8_r0 = 255;
            mPolyF3.field_0_header.field_9_g0 = 255;
            mPolyF3.field_0_header.field_A_b0 = 0;

            mPolyF3.field_C_x0 = 180+50;
            mPolyF3.field_E_y0 = 50;

            mPolyF3.field_10_x1 = 180+200;
            mPolyF3.field_12_y1 = 50;

            mPolyF3.field_14_x2 = 180+150;
            mPolyF3.field_16_y2 = 100;
        }

        {
            PolyF4_Init_4F8830(&mPolyF4);
            mPolyF4.field_0_header.field_8_r0 = 255;
            mPolyF4.field_0_header.field_9_g0 = 255;
            mPolyF4.field_0_header.field_A_b0 = 255;

            struct XY { short x; short y; };
            XY points[4] =
            {
                { 150, 150 },
                { 150, 200 },
                { 200, 150 },
                { 200, 200 },
            };

            mPolyF4.field_C_x0 = points[0].x;
            mPolyF4.field_E_y0 = points[0].y;

            mPolyF4.field_10_x1 = points[1].x;
            mPolyF4.field_12_y1 = points[1].y;

            mPolyF4.field_14_x2 = points[2].x;
            mPolyF4.field_16_y2 = points[2].y;

            mPolyF4.field_18_x3 = points[3].x;
            mPolyF4.field_1A_y3 = points[3].y;
        }

        {
            PolyG4_Init_4F88B0(&mPolyG4);
            struct XY { short x; short y; };
            XY points[4] =
            {
                { 150 + 100, 150 + 10 },
                { 150 + 100, 200 + 10 },
                { 200 + 100, 150 + 10 },
                { 200 + 100, 200 + 10 },
            };

            mPolyG4.field_C_x0 = points[0].x;
            mPolyG4.field_E_y0 = points[0].y;

            mPolyG4.field_0_header.field_8_r0 = 255;
            mPolyG4.field_0_header.field_9_g0 = 0;
            mPolyG4.field_0_header.field_A_b0 = 0;


            mPolyG4.field_14_x1 = points[1].x;
            mPolyG4.field_16_y1 = points[1].y;

            mPolyG4.field_10_r1 = 0;
            mPolyG4.field_11_g1 = 0;
            mPolyG4.field_12_b1 = 255;


            mPolyG4.field_1C_x2 = points[2].x;
            mPolyG4.field_1E_y2 = points[2].y;

            mPolyG4.field_18_r2 = 0;
            mPolyG4.field_19_g2 = 255;
            mPolyG4.field_1A_b2 = 0;

            mPolyG4.field_24_x3 = points[3].x;
            mPolyG4.field_26_y3 = points[3].y;

            mPolyG4.field_20_r3 = 255;
            mPolyG4.field_21_g3 = 0;
            mPolyG4.field_22_b3 = 255;
        }

        {
            LineG2_Init(&mLineG2);

            mLineG2.field_0_header.field_8_r0 = 255;
            mLineG2.field_0_header.field_9_g0 = 255;
            mLineG2.field_0_header.field_A_b0 = 0;

            mLineG2.field_C_x0 = 250;
            mLineG2.field_E_y0 = 80;

            mLineG2.field_10_r1 = 255;
            mLineG2.field_11_g1 = 0;
            mLineG2.field_12_b1 = 255;

         
            mLineG2.field_14_x1 = 350;
            mLineG2.field_16_y1 = 110;
        }
        
        {
            LineG4_Init(&mLineG4);

            mLineG4.field_0_header.field_8_r0 = 255;
            mLineG4.field_0_header.field_9_g0 = 255;
            mLineG4.field_0_header.field_A_b0 = 0;
            mLineG4.field_C_x0 = 280;
            mLineG4.field_E_y0 = 120;

            mLineG4.field_10_r1 = 255;
            mLineG4.field_11_g1 = 0;
            mLineG4.field_12_b1 = 255;
            mLineG4.field_14_x1 = 300;
            mLineG4.field_16_y1 = 150;


            mLineG4.field_1C_x2 = 20;
            mLineG4.field_1E_y2 = 20;
            mLineG4.field_18_r2 = 255;
            mLineG4.field_19_g2 = 255;
            mLineG4.field_22_b3 = 0;

            mLineG4.field_24_x3 = 200;
            mLineG4.field_26_y3 = 50;
            mLineG4.field_20_r3 = 255;
            mLineG4.field_21_g3 = 0;
            mLineG4.field_22_b3 = 50;
        }
    }

    Line_G2 mLineG2 = {};
    Line_G4 mLineG4 = {};

    Poly_G3 mPolyG3 = {};
    Poly_F3 mPolyF3 = {};

    Poly_G4 mPolyG4 = {};
    Poly_F4 mPolyF4 = {};

    Prim_ScreenOffset mScreenOffset = {};
    Prim_PrimClipper mPrimClipper = {};

    // TODO: Test SetTPage
    // TODO: Test Prim_Sprt
    // TODO: Test Prim_Tile
    // TODO: Test Poly_FT4
};

EXPORT void CC Game_Loop_467230()
{
    alive_new<RenderTest>(); // Will get nuked at LVL/Path change

    dword_5C2F78 = 0;
    sBreakGameLoop_5C2FE0 = 0;
    bool bPauseMenuObjectFound = false;
    while (!gBaseGameObject_list_BB47C4->IsEmpty())
    {
        sub_422DA0();
        sub_449A90();
        word_5C2FA0 = 0;

        // Update objects
        for (int baseObjIdx = 0; baseObjIdx < gBaseGameObject_list_BB47C4->Size(); baseObjIdx++)
        {
            BaseGameObject* pBaseGameObject = gBaseGameObject_list_BB47C4->ItemAt(baseObjIdx);

            if (!pBaseGameObject || word_5C2FA0)
            {
                break;
            }

            if (pBaseGameObject->field_6_flags & BaseGameObject::eUpdatable 
                && !(pBaseGameObject->field_6_flags & BaseGameObject::eDead) 
                && (!word_5C1B66 || pBaseGameObject->field_6_flags &  BaseGameObject::eUpdatableExtra))
            {
                if (pBaseGameObject->field_1C_update_delay <= 0)
                {
                    if (pBaseGameObject == pPauseMenu_5C9300)
                    {
                        bPauseMenuObjectFound = true;
                    }
                    else
                    {
                        pBaseGameObject->VUpdate();
                    }
                }
                else
                {
                    pBaseGameObject->field_1C_update_delay--;
                }
            }
        }

        // Animate everything
        if (word_5C1B66 <= 0)
        {
            Animation::AnimateAll_40AC20(gObjList_animations_5C1A24);
        }

        /*
        static int hack = 0;
        hack++;
        if (hack == 100)
        {
            static BackgroundAnimation_Params params = {};
            params.field_10_res_id = 8001;
            params.field_8_xpos = 90;
            params.field_A_ypos = 90;
            params.field_C_width = 50;
            params.field_E_height = 50;
            params.field_1A_layer = 1;
            ResourceManager::LoadResourceFile_49C170("STDOOR.BAN", 0);
            Factory_BackgroundAnimation_4D84F0(&params, 4, 4, 4);
        }
        */

        int** pOtBuffer = gPsxDisplay_5C1130.field_10_drawEnv[gPsxDisplay_5C1130.field_C_buffer_index].field_70_ot_buffer;
        
        // Render objects
        for (int i=0; i < gObjList_drawables_5C1124->Size(); i++)
        {
            BaseGameObject* pObj = gObjList_drawables_5C1124->ItemAt(i);
            if (!pObj)
            {
                break;
            }

            if (pObj->field_6_flags & BaseGameObject::eDead)
            {
                pObj->field_6_flags = pObj->field_6_flags & ~BaseGameObject::eCantKill;
            }
            else if (pObj->field_6_flags & BaseGameObject::eDrawable)
            {
                pObj->field_6_flags |= BaseGameObject::eCantKill;
                pObj->VRender(pOtBuffer);
            }
        }

        // Render FG1's
        for (int i=0; i < gFG1List_5D1E28->Size(); i++)
        {
            FG1* pFG1 = gFG1List_5D1E28->ItemAt(i);
            if (!pFG1)
            {
                break;
            }

            if (pFG1->field_6_flags & BaseGameObject::eDead)
            {
                pFG1->field_6_flags = pFG1->field_6_flags & ~BaseGameObject::eCantKill;
            }
            else if (pFG1->field_6_flags & BaseGameObject::eDrawable)
            {
                pFG1->field_6_flags |= BaseGameObject::eCantKill;
                pFG1->VRender(pOtBuffer);
            }
        }
        
        DebugFont_Flush_4DD050();
        PSX_DrawSync_4F6280(0);
        pScreenManager_5BB5F4->VRender(pOtBuffer);
        sub_494580(); // Exit checking?
        
        gPsxDisplay_5C1130.PSX_Display_Render_OT_41DDF0();
        
        // Destroy objects with certain flags
        for (short idx = 0; idx < gBaseGameObject_list_BB47C4->Size(); idx++)
        {
            BaseGameObject* pObj = gBaseGameObject_list_BB47C4->ItemAt(idx);
            if (!pObj)
            {
                break;
            }

            const int flags = pObj->field_6_flags;
            if (flags & 4 && !(flags & BaseGameObject::eCantKill))
            {
                DynamicArrayIter it;
                it.field_0_pDynamicArray = gBaseGameObject_list_BB47C4;
                it.field_4_idx = idx + 1;

                it.Remove_At_Iter_40CCA0();
                pObj->VDestructor(1);
            }
        }

        if (bPauseMenuObjectFound && pPauseMenu_5C9300)
        {
            pPauseMenu_5C9300->VUpdate();
        }

        bPauseMenuObjectFound = false;

        gMap_5C3030.sub_480B80();
        sInputObject_5BD4E0.Update_45F040();

        if (!word_5C1B66)
        {
            sGnFrame_5C1B84++;
        }

        if (sBreakGameLoop_5C2FE0)
        {
            break;
        }
        
        // Enabled only for ddfast option
        if (byte_5CA4D2)
        {
            pResourceManager_5C1BB0->sub_465590(0);
        }
    } // Main loop end

    // Clear the screen to black
    PSX_RECT rect = {};
    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 240;
    PSX_ClearImage_4F5BD0(&rect, 0, 0, 0);
    PSX_DrawSync_4F6280(0);
    PSX_VSync_4F6170(0);

    pResourceManager_5C1BB0->Shutdown_465610();

    // Destroy all game objects
    while (!gBaseGameObject_list_BB47C4->IsEmpty())
    {
        DynamicArrayIter iter = {};
        iter.field_0_pDynamicArray = gBaseGameObject_list_BB47C4;
        for (short idx =0; idx < gBaseGameObject_list_BB47C4->Size(); idx++)
        {
            BaseGameObject* pObj = gBaseGameObject_list_BB47C4->ItemAt(idx);
            iter.field_4_idx = idx + 1;
            if (!pObj)
            {
                break;
            }
            iter.Remove_At_Iter_40CCA0();
            pObj->VDestructor(1);
        }
    }
}
