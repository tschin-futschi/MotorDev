
#include "StdAfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "Func.h"

BYTE _SLV_OIS_ = 0x24;

ptrWriteI2CDev WriteI2CDev;
ptrReadI2CDev ReadI2CDev;
ptrOutputLog  OutputLog;


void CntWrt( unsigned short addr, unsigned short *dat, int size )
{
	int i = 0;
	UCHAR* tmp = new UCHAR[ sizeof(UCHAR) * size * 2 + 2];
	memset(tmp, 0, sizeof(UCHAR)*size *2 + 2);

	tmp[0] = addr >> 8;
	tmp[1] = addr & 0xFF;

	for(i=0; i<size; i++)
	{
		tmp[2+2*i] = dat[i] >> 8;
		tmp[2+2*i+1] = dat[i] & 0xFF;

		if(addr == 0x8000)
		{
			log_tprintf(1, _T("[W] Addr:%04X -- Data:%04X\r\n"), 0x8000+i, dat[i]);
		}
	}

	WriteI2CDev(_SLV_OIS_>>1, size*2+2, tmp);
	delete [] tmp; 
}


void CnRd(unsigned short addr, unsigned short *dat, int size)
{
	UCHAR* tmp = new UCHAR[ sizeof(UCHAR) * size * 2];
	memset(tmp, 0, sizeof(UCHAR)*size *2);

	UCHAR addr_tmp[2] = {0};
	int i = 0;

	addr_tmp[0] = addr >> 8;
	addr_tmp[1] = addr & 0xFF;
	
	ReadI2CDev(_SLV_OIS_>>1, 2, addr_tmp, 2*size, tmp);

	for(i=0; i<size; i++)
	{
		dat[i] = tmp[2*i] << 8 | tmp[2*i+1];

		if(addr == 0x8000)
		{
			log_tprintf(1, _T("[R] ADDR:%04X -- Data:%04X\r\n"), 0x8000+i, dat[i]);
		}
	}
	delete [] tmp; 
}


void RamWriteA(unsigned short u16_addr, unsigned short u16_dat)
{
	BYTE dat[4] = {0};

	dat[0] = (u16_addr >> 8) & 0xFF;
	dat[1] = (u16_addr & 0xFF);		
	dat[2] = (u16_dat >> 8) & 0xFF;
	dat[3] = (u16_dat & 0xFF);

	WriteI2CDev( _SLV_OIS_>>1, 4, dat);
}

void ByteWriteA(unsigned char Slave_ID, unsigned short u8_addr, unsigned short u8_dat)
{
	BYTE dat[2] = {0};

	dat[0] = (u8_addr & 0xFF);		
	dat[1] = (u8_dat & 0xFF);

	WriteI2CDev( Slave_ID>>1, 2, dat);
}

void RamReadA(unsigned short u16_addr, unsigned short* u16_dat)
{
	BYTE addr[2];
	BYTE dat[2];

	addr[0] = (u16_addr >> 8) & 0xFF;
	addr[1] = (u16_addr & 0xFF);

	ReadI2CDev(_SLV_OIS_>>1, 2, addr, 2, dat);
	
	*u16_dat = dat[0] * 0x100 + dat[1];
}


int log_tprintf(int print_log, TCHAR *format, ...)
{
	if(print_log == 0)
		return 0;

	TCHAR str[2048];
	int r;
	va_list va;

	int length = (int)lstrlen(format);

	if(	length >= sizeof(str) ){
		_tprintf(_T("length of %s: %d\n"), format, length);
		return -1;
	}

	va_start(va, format);
	r = _vstprintf_s(str, _countof(str), format, va);
	va_end(va);

	if(OutputLog == NULL){
		OutputDebugString(str);
	}
	else
	{
		OutputLog(str);
	}
	return r;
}



short H2D( unsigned short u16_inpdat )
{
	short s16_temp;

	s16_temp = u16_inpdat;

	if( u16_inpdat > 32767 )
	{
		s16_temp = (unsigned short)(u16_inpdat - 65536L);
	}

	return s16_temp;
}

unsigned short D2H( short s16_inpdat)
{

	unsigned short u16_temp;

	if( s16_inpdat < 0 )
	{
		u16_temp = (unsigned short)(s16_inpdat + 65536);
	}
	else
	{
		u16_temp = s16_inpdat;
	}

	return u16_temp;
}

void ByteReadA(BYTE Slave_ID, BYTE u08_addr, BYTE* u08_dat)
{
	ReadI2CDev( Slave_ID>>1, 1, &u08_addr, 1, u08_dat);
}

void UserByteReadA(unsigned char Slave_ID, unsigned short u16_addr, unsigned char* u8_dat)
{
	//16bit address, 8bit data
	BYTE addr[2];
	BYTE dat[2];

	addr[0] = (u16_addr >> 8) & 0xFF;
	addr[1] = (u16_addr & 0xFF);

	ReadI2CDev(Slave_ID>>1, 2, addr, 1, u8_dat);
}

void UserByteWriteA(unsigned char Slave_ID, unsigned short u16_addr, unsigned char u8_dat)
{
	//16bit address, 8bit data
	BYTE dat[3] = {0};

	dat[0] = (u16_addr >> 8) & 0xFF;
	dat[1] = (u16_addr & 0xFF);	
	dat[2] = (u8_dat & 0xFF);

	WriteI2CDev( Slave_ID>>1, 3, dat);
}

void i2c_write_byte(BYTE SlaveID, BYTE u08_adr, BYTE u08_dat)
{
	//8bit address, 8bit data
	BYTE dat[2] = {0};

	dat[0] = u08_adr;
	dat[1] = u08_dat;		

	WriteI2CDev( SlaveID>>1 , 2, dat);
}

void i2c_read_byte(BYTE SlaveID, BYTE u08_adr, BYTE* u08_dat)
{
	//8bit address, 8bit data
	ReadI2CDev( SlaveID>>1, 1, &u08_adr, 1, u08_dat);
}

void i2c_block_write( BYTE SlaveID, BYTE addr, BYTE *dat, int size )
{
	int i = 0;
	UCHAR* tmp = new UCHAR[ sizeof(UCHAR) * size + 1];
	memset(tmp, 0, sizeof(UCHAR)*size + 1);

	tmp[0] = addr;

	for(i=0; i<size; i++)
	{
		tmp[1+i] = dat[i];
	}

	WriteI2CDev(SlaveID>>1, size+1, tmp);
	delete [] tmp; 
}


void i2c_block_read( BYTE SlaveID, BYTE addr, BYTE *dat, int size)
{
	ReadI2CDev(SlaveID>>1, 1, &addr, size, dat);
}