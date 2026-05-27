/**
  ******************************************************************************
  * File Name          : Func.h
  * Description        : Main program header (Refactored)
  * DongWoon Anatech 	 : 
  * Version						 : 0.3 (Fully standardized using <stdint.h>)
  ******************************************************************************
  *
  * COPYRIGHT(c) 2018 DWANATECH
  * Revision History
  * 2019.07.14 original release
  * 2025.10.24 refactored (applying uXX_name convention and stdint types)
  ******************************************************************************
**/

#ifndef __FUNC_H
#define __FUNC_H

#include <stdint.h>
#include <tchar.h> // TCHAR definition for log_tprintf

//========== Extern I2C Functions ====================================
// Note: BYTE is now explicitly defined as uint8_t for standard usage.
typedef uint8_t BYTE; 

typedef int32_t (*ptrWriteI2CDev)(uint8_t u08_slave_id, uint8_t u08_count, uint8_t *u08_data);
typedef int32_t (*ptrReadI2CDev)( uint8_t u08_slave_id, uint8_t u08_addr_count, uint8_t *u08_addr, uint8_t u08_read_count, uint8_t *u08_data );
typedef int32_t (*ptrOutputLog)( const char * );
//=====================================================================

#define Wait(a)     Wait_usec(a*1000UL)

// Function prototypes
int32_t	log_tprintf(int32_t print_log, TCHAR *format, ...);
void Wait_usec(uint32_t u32_usec); 

/* ---------------------------------------------
 * RAM access helpers (I2C/SPI via _SLV_OIS_)
 * A: 16-bit addr / 16-bit data (Word)
 * B: 16-bit addr / 8-bit  data (Byte)
 * C: 8-bit  addr / 8-bit  data (Byte) - SlaveID explicit
 * --------------------------------------------- */

// --- A Group: 16bit Addr / 16bit Data (Word) Access ---
/** Write 16-bit data to 16-bit address. */
void RamWriteA(uint16_t u16_addr, uint16_t u16_data);

/** Read  16-bit data from 16-bit address. */
void RamReadA (uint16_t u16_addr, uint16_t *u16_data);

/** Burst write: write [u32_count] words (16-bit each) starting at [u16_addr]. */
void RamWriteA_Burst(uint16_t u16_addr, const uint16_t *u16_data, int32_t u32_count);

/** Burst read: read [u32_count] words (16-bit each) starting at [u16_addr]. */
void RamReadA_Burst (uint16_t u16_addr, uint16_t *u16_data, int32_t u32_count);


// --- B Group: 16bit Addr / 8bit Data (Byte) Access ---
/** Write 8-bit data to 16-bit address. */
void RamWriteB(uint16_t u16_addr, uint8_t u08_data);

/** Read  8-bit data from 16-bit address. */
void RamReadB (uint16_t u16_addr, uint8_t *u08_data);

/** Burst write (8-bit data): write [u32_count] bytes starting at [u16_addr]. */
void RamWriteB_Burst(uint16_t u16_addr, const uint8_t *u08_data, int32_t u32_count);

/** Burst read (8-bit data): read [u32_count] bytes starting at [u16_addr]. */
void RamReadB_Burst (uint16_t u16_addr, uint8_t *u08_data, int32_t u32_count);


// --- C Group: 8bit Addr / 8bit Data (Byte) Access (I2C Standard) ---
/** Write 8-bit data to 8-bit address (Standard I2C single reg write). */
void RamWriteC(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t u08_data);

/** Read 8-bit data from 8-bit address (Standard I2C single reg read). */
void RamReadC(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t *u08_data);

/** Burst write (8-bit data): write [u32_count] bytes starting at [u08_addr]. */
void RamWriteC_Burst(uint8_t u08_slave_id, uint8_t u08_addr, const uint8_t *u08_data, int32_t u32_count);

/** Burst read (8-bit data): read [u32_count] bytes starting at [u08_addr]. */
void RamReadC_Burst(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t *u08_data, int32_t u32_count);


// --- Utility and Conversion Functions ---
int16_t H2D( uint16_t u16_inpdat );
uint16_t D2H( int16_t s16_inpdat);


#endif // __FUNC_H