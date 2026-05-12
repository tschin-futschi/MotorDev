// AW86100 stub DLL implementation.
// All exports return success / typical placeholder values.

#include "../AW86100.h"

extern "C" {

DLLEXPORT INT AwGetDllVersion(BYTE* byBuff)
{
    (void)byBuff;
    return RET_OK;
}

DLLEXPORT INT AwOIS_ExtFuncInit(DLL_ExtFuncList* pExtFuncList)
{
    (void)pExtFuncList;
    return RET_EXTFUNCINIT_OK;
}

DLLEXPORT INT Aw_OIS_ExtFuncDeInit()
{
    return RET_OK;
}

DLLEXPORT INT AwBootcontrol(INT times, INT nDelayTime)
{
    (void)times;
    (void)nDelayTime;
    return RET_OK;
}

DLLEXPORT INT AwMoveBinDownload(BYTE* bypBinBuf, UINT32 u32Len, DLNumType eNumOfPackType)
{
    (void)bypBinBuf;
    (void)u32Len;
    (void)eNumOfPackType;
    return RET_OK;
}

DLLEXPORT INT AwFlashDownload(UINT32 u32FlashDownAddr, BYTE* bypBinBuf, UINT32 u32Len, DLNumType eNumOfPackType)
{
    (void)u32FlashDownAddr;
    (void)bypBinBuf;
    (void)u32Len;
    (void)eNumOfPackType;
    return RET_OK;
}

DLLEXPORT INT AwResetChip()
{
    return RET_OK;
}

DLLEXPORT INT AwSet7bitI2CSlaveAddr(BYTE byAddr)
{
    (void)byAddr;
    return RET_OK;
}

DLLEXPORT BYTE AwGet7bitI2CSlaveAddr()
{
    return 0x77;
}

DLLEXPORT INT AwSet7bitI2CAsynAddr(BYTE byAddr)
{
    (void)byAddr;
    return RET_OK;
}

DLLEXPORT BYTE AwGet7bitI2CAsynAddr()
{
    return 0x77;
}

DLLEXPORT INT AwFlashRead(BYTE* bypAddrBuf, UINT32 u32ReadLen, BYTE* bypData)
{
    (void)bypAddrBuf;
    (void)u32ReadLen;
    (void)bypData;
    return RET_OK;
}

} // extern "C"
