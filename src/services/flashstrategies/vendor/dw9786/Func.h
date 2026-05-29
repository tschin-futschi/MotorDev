
/**
  ******************************************************************************
  * File Name          : Func.h
  * Description        : Main program header
  * DongWoon Anatech 	 : 
  * Version						 : 0.1
  ******************************************************************************
  *
  * COPYRIGHT(c) 2018 DWANATECH
  * Revision History
	*2019.07.14 release
  ******************************************************************************
**/

#define Wait(a)     Wait_usec(a*1000UL)


//========== Extern I2C Functions ====================================
typedef int (*ptrWriteI2CDev)(BYTE, BYTE, BYTE *);
typedef int (*ptrReadI2CDev)( BYTE, BYTE, BYTE *, BYTE, BYTE * );
typedef int (*ptrOutputLog)( const char * );
//=====================================================================


int	log_tprintf(int print_log, TCHAR *format, ...);
void RamWriteA(unsigned short u16_addr, unsigned short u16_dat);
void RamReadA(unsigned short u16_addr, unsigned short* u16_dat);
void ByteWriteA(unsigned char Slave_ID, unsigned short u8_addr, unsigned short u8_dat);
void ByteReadA(BYTE Slave_ID, BYTE u08_addr, BYTE* u08_dat);

void UserByteReadA(unsigned char Slave_ID, unsigned short u16_addr, unsigned char* u8_dat);
void UserByteWriteA(unsigned char Slave_ID, unsigned short u16_addr, unsigned char u8_dat);

void CntWrt( unsigned short addr, unsigned short *dat, int size);
void CnRd(unsigned short addr, unsigned short *dat, int size);

short H2D( unsigned short u16_inpdat );
unsigned short D2H( short s16_inpdat);



void i2c_read_byte(BYTE slaveid, BYTE u08_adr, BYTE* u08_dat);
void i2c_write_byte(BYTE slaveid, BYTE u08_adr, BYTE u08_dat);
void i2c_block_write( BYTE SlaveID, BYTE addr, BYTE *dat, int size );
void i2c_block_read( BYTE SlaveID, BYTE addr, BYTE *dat, int size);
