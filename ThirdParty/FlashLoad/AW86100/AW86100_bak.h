#pragma once

#if defined(Q_OS_WIN)
    #define DLLEXPORT Q_DECL_EXPORT
    #define DLLIMPORT Q_DECL_IMPORT
#else
    #define DLLIMPORT __declspec(dllimport)
    #define DLLEXPORT __declspec(dllexport)
#endif

//Extern function list
typedef INT (*ptrReadI2CDev)(BYTE DevId, BYTE AddrSize, BYTE* pAddr, BYTE Rdsize, BYTE* pRdBuf);
typedef INT (*ptrWriteI2CDev)(BYTE DevId, BYTE WrSize, BYTE* WrData);
typedef INT (*ptrOutputLog)(const char* str);

enum ExtFuncInitError 
{
    RET_EXTFUNCINIT_OK = 0,
    RET_EXTFUNCINIT_NULL = -1,
};

enum CommuRet
{
    RET_Commmu_OK = 0,
};

enum DLNumType 
{
    DL_NumType_0 = 0,
    DL_NumType_1 = 1,
};

enum OisRet
{
    RET_OK = 0,

    //resetchip
    RET_RESETCHIP_W_ASYN_ADDR_FAIL1 = -1,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL2 = -2,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL3 = -3,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL4 = -4,
    RET_RESETCHIP_R_ASYN_ADDR_FAIL1 = -5,
    RET_RESETCHIP_R_ASYN_ADDR_FAIL2 = -6,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL5 = -7,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL6 = -8,
    RET_RESETCHIP_W_ASYN_ADDR_FAIL7 = -9,
    RET_RESETCHIP_R_READY_FLAG_FAIL = -10,

    //bootcontrol
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL1 = -11,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL2 = -12,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL3 = -13,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL4 = -14,
    RET_BOOTCONTROL_RESET_R_ASYN_ADDR_FAIL1 = -15,
    RET_BOOTCONTROL_RESET_R_ASYN_ADDR_FAIL2 = -16,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL5 = -17,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL6 = -18,
    RET_BOOTCONTROL_RESET_W_ASYN_ADDR_FAIL7 = -19,
    RET_BOOTCONTROL_STOP_BOOTLOADER = -20,

    //hankconnectcheck
    RET_HANCONNECTCHECK_W_FAIL          = -21,
    RET_HANCONNECTCHECK_R_FAIL          = -22,
    RET_HANCONNECTCHECK_COMPARE_FAIL    = -23,


    //download
    RET_ISP_PBUF_ERROR      = -24,
    RET_ISP_ADDR_ERROR      = -25,
    RET_ISP_SPACE_ERROR     = -26,
    RET_ISP_NUM_ERROR       = -27,
    RET_ISP_FLASH_ERROR     = -28,
    RET_ISP_FLASH_ERROR1    = -29,
    RET_ISP_FLASH_ERROR2    = -30,
    RET_ISP_JUMP_ERROR      = -31,

    //flash read
    RET_FLASH_NULL_POINT_ERROR  = -32,
    RET_FLASH_READ_W_ERROR      = -33,
    RET_FLASH_READ_R_ERROR      = -34,
    RET_FLASH_READ_C_ERROR      = -35,
    RET_FLASH_LEN_ERROR         = -36,

    //TLS
    RET_TLS_SETVALUE_ERROR      = -37,
    RET_TLS_GETVALUE_ERROR_BYTE = 0xFF,
};

typedef struct _sDLL_ExtFuncList
{
    ptrReadI2CDev   pFunc_I2C_Read;
    ptrWriteI2CDev  pFunc_I2C_Write;
    ptrOutputLog    pFunc_OutputLog;
} DLL_ExtFuncList; 

#ifdef __cplusplus
extern "C" {
#endif
    DLLEXPORT INT AwGetDllVersion(BYTE* byBuff);
    
    //Init
    DLLEXPORT INT AwOIS_ExtFuncInit(DLL_ExtFuncList* pExtFuncList);
    //DelInit
    DLLEXPORT INT Aw_OIS_ExtFuncDeInit();

    //BootControl
    DLLEXPORT INT AwBootcontrol(INT times = 20, INT nDelayTime = 7);


    //Download Firmare program
    //u32NumOfPackType: DL_NumType_0, 16 word data DL_NumType_1, 8 word data
    DLLEXPORT INT  AwMoveBinDownload(BYTE* bypBinBuf, UINT32 u32Len, DLNumType eNumOfPackType);
    //Address is dynamic, effective address:  0x01000000 - 0x010FFFF
    DLLEXPORT INT AwFlashDownload(UINT32  u32FlashDownAddr, BYTE* bypBinBuf, UINT32 u32Len, DLNUMType eNumOfPackType);

    //ResetChip
    DLLEXPORT INT AwResetChip();

    //Set and Get Address
    DLLEXPORT INT AwSet7bitI2CSlaveAddr(BYTE byAddr);
    DLLEXPORT BYTE AwGet7bitI2CSlaveAddr();
    DLLEXPORT INT AwSet7bitI2CAsynAddr(BYTE byAddr);
    DLLEXPORT BYTE AwGet7bitI2CAsynAddr();

    //flash read
    DLLEXPORT INT AwFlashRead(BYTE* bypAddrBuf, UINT32 u32ReadLen,  BYTE* bypData);

#ifdef __cplusplus
}
#defif
