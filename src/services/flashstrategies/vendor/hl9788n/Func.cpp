#include "Func.h"
#include "StdAfx.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

// Global I2C Target Slave ID
uint8_t _SLV_OIS_ = 0x24;

// External function pointers (defined in Func.h)
ptrWriteI2CDev WriteI2CDev;
ptrReadI2CDev ReadI2CDev;
ptrOutputLog OutputLog;

// Placeholder implementation for Wait_usec (Assuming external dependency)
void Wait_usec(uint32_t u32_usec) {
  // Implementation needed
}

/* ---------------------------------------------
 * A Group: 16bit Addr / 16bit Data (Word) Access
 * --------------------------------------------- */

void RamWriteA(uint16_t u16_addr, uint16_t u16_data) {
  uint8_t dat[4] = {0};

  dat[0] = (uint8_t)(u16_addr >> 8);
  dat[1] = (uint8_t)(u16_addr & 0xFF);
  dat[2] = (uint8_t)(u16_data >> 8);
  dat[3] = (uint8_t)(u16_data & 0xFF);

  WriteI2CDev(_SLV_OIS_ >> 1, 4, dat);
}

void RamReadA(uint16_t u16_addr, uint16_t *u16_data) {
  uint8_t u08_addr[2];
  uint8_t u08_dat[2];

  u08_addr[0] = (uint8_t)(u16_addr >> 8);
  u08_addr[1] = (uint8_t)(u16_addr & 0xFF);

  ReadI2CDev(_SLV_OIS_ >> 1, 2, u08_addr, 2, u08_dat);

  *u16_data = (uint16_t)(u08_dat[0] * 0x100 + u08_dat[1]); // Big-endian
}

void RamWriteA_Burst(uint16_t u16_addr, const uint16_t *u16_data,
                     int32_t u32_count) {
  int32_t i = 0;
  // Memory allocation for [addrH, addrL, d0H, d0L, d1H, d1L, ...]
  uint8_t *tmp = new uint8_t[sizeof(uint8_t) * u32_count * 2 + 2];
  memset(tmp, 0, sizeof(uint8_t) * u32_count * 2 + 2);

  tmp[0] = (uint8_t)(u16_addr >> 8);
  tmp[1] = (uint8_t)(u16_addr & 0xFF);

  for (i = 0; i < u32_count; i++) {
    tmp[2 + 2 * i] = (uint8_t)(u16_data[i] >> 8);
    tmp[2 + 2 * i + 1] = (uint8_t)(u16_data[i] & 0xFF);

    if (u16_addr == 0x8000) {
      log_tprintf(1, _T("[W] Addr:%04X -- Data:%04X\r\n"), 0x8000 + i,
                  u16_data[i]);
    }
  }

  WriteI2CDev(_SLV_OIS_ >> 1, (uint8_t)(u32_count * 2 + 2), tmp);
  delete[] tmp;
}

void RamReadA_Burst(uint16_t u16_addr, uint16_t *u16_data, int32_t u32_count) {
  uint8_t *tmp = new uint8_t[sizeof(uint8_t) * u32_count * 2];
  memset(tmp, 0, sizeof(uint8_t) * u32_count * 2);

  uint8_t addr_tmp[2] = {0};
  int32_t i = 0;

  addr_tmp[0] = (uint8_t)(u16_addr >> 8);
  addr_tmp[1] = (uint8_t)(u16_addr & 0xFF);

  ReadI2CDev(_SLV_OIS_ >> 1, 2, addr_tmp, (uint8_t)(2 * u32_count), tmp);

  for (i = 0; i < u32_count; i++) {
    u16_data[i] = (uint16_t)(tmp[2 * i] << 8 | tmp[2 * i + 1]);

    if (u16_addr == 0x8000) {
      log_tprintf(1, _T("[R] ADDR:%04X -- Data:%04X\r\n"), 0x8000 + i,
                  u16_data[i]);
    }
  }
  delete[] tmp;
}

/* ---------------------------------------------
 * B Group: 16bit Addr / 8bit Data (Byte) Access
 * --------------------------------------------- */

void RamWriteB(uint16_t u16_addr, uint8_t u08_data) {
  // 16bit address, 8bit data
  uint8_t dat[3] = {0};

  dat[0] = (uint8_t)(u16_addr >> 8);
  dat[1] = (uint8_t)(u16_addr & 0xFF);
  dat[2] = (uint8_t)(u08_data & 0xFF);

  WriteI2CDev(_SLV_OIS_ >> 1, 3, dat);
}

void RamReadB(uint16_t u16_addr, uint8_t *u08_data) {
  // 16bit address, 8bit data
  uint8_t u08_addr[2];

  u08_addr[0] = (uint8_t)(u16_addr >> 8);
  u08_addr[1] = (uint8_t)(u16_addr & 0xFF);

  ReadI2CDev(_SLV_OIS_ >> 1, 2, u08_addr, 1, u08_data);
}

void RamWriteB_Burst(uint16_t u16_addr, const uint8_t *u08_data,
                     int32_t u32_count) {
  int32_t i = 0;
  uint8_t *tmp = new uint8_t[sizeof(uint8_t) * (u32_count + 2)];
  memset(tmp, 0, sizeof(uint8_t) * (u32_count + 2));

  // [addrH, addrL, d0, d1, ...]
  tmp[0] = (uint8_t)(u16_addr >> 8);
  tmp[1] = (uint8_t)(u16_addr & 0xFF);

  for (i = 0; i < u32_count; i++) {
    tmp[2 + i] = u08_data[i];

    if (u16_addr == 0x8000) {
      log_tprintf(1, _T("[W] Addr:%04X -- Data:%02X\r\n"), (0x8000 + i),
                  u08_data[i]);
    }
  }

  WriteI2CDev(_SLV_OIS_ >> 1, (uint8_t)(u32_count + 2), tmp);
  delete[] tmp;
}

void RamReadB_Burst(uint16_t u16_addr, uint8_t *u08_data, int32_t u32_count) {
  int32_t i = 0;
  uint8_t *tmp = new uint8_t[sizeof(uint8_t) * u32_count];
  memset(tmp, 0, sizeof(uint8_t) * u32_count);

  uint8_t addr_tmp[2] = {0};
  addr_tmp[0] = (uint8_t)(u16_addr >> 8);
  addr_tmp[1] = (uint8_t)(u16_addr & 0xFF);

  // 연속 바이트 읽기Dev_WriteI2C
  ReadI2CDev(_SLV_OIS_ >> 1, 2, addr_tmp, (uint8_t)u32_count, tmp);

  for (i = 0; i < u32_count; i++) {
    u08_data[i] = tmp[i];

    if (u16_addr == 0x8000) {
      log_tprintf(1, _T("[R] ADDR:%04X -- Data:%02X\r\n"), (0x8000 + i),
                  u08_data[i]);
    }
  }

  delete[] tmp;
}

/* ---------------------------------------------
 * C Group: 8bit Addr / 8bit Data (Byte) Access (Standard I2C)
 * --------------------------------------------- */

void RamWriteC(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t u08_data) {
  uint8_t dat[2] = {0};

  dat[0] = u08_addr;
  dat[1] = u08_data;

  WriteI2CDev(u08_slave_id >> 1, 2, dat);
}

void RamReadC(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t *u08_data) {
  ReadI2CDev(u08_slave_id >> 1, 1, &u08_addr, 1, u08_data);
}

void RamWriteC_Burst(uint8_t u08_slave_id, uint8_t u08_addr,
                     const uint8_t *u08_data, int32_t u32_count) {
  int32_t i = 0;
  uint8_t *tmp = new uint8_t[sizeof(uint8_t) * u32_count + 1];
  memset(tmp, 0, sizeof(uint8_t) * u32_count + 1);

  tmp[0] = u08_addr;

  for (i = 0; i < u32_count; i++) {
    tmp[1 + i] = u08_data[i];
  }

  WriteI2CDev(u08_slave_id >> 1, (uint8_t)(u32_count + 1), tmp);
  delete[] tmp;
}

void RamReadC_Burst(uint8_t u08_slave_id, uint8_t u08_addr, uint8_t *u08_data,
                    int32_t u32_count) {
  ReadI2CDev(u08_slave_id >> 1, 1, &u08_addr, (uint8_t)u32_count, u08_data);
}

/* ---------------------------------------------
 * Utility and Conversion Functions (log, H2D, D2H)
 * --------------------------------------------- */

int32_t log_tprintf(int32_t print_log, TCHAR *format, ...) {
  if (print_log == 0)
    return 0;

  TCHAR str[2048];
  int32_t r;
  va_list va;

  int32_t length = (int32_t)_tcslen(format);

  if (length >= sizeof(str)) {
    _tprintf(_T("length of %s: %d\n"), format, length);
    return -1;
  }

  va_start(va, format);
  r = _vstprintf_s(str, _countof(str), format, va);
  va_end(va);

  if (OutputLog == NULL) {
    OutputDebugString(str);
  } else {
    OutputLog((const char *)str); // Assuming OutputLog takes const char*
  }
  return r;
}

int16_t H2D(uint16_t u16_inpdat) {
  int16_t s16_temp;

  s16_temp = (int16_t)u16_inpdat;

  if (u16_inpdat > 32767) {
    s16_temp = (int16_t)(u16_inpdat - 65536L);
  }

  return s16_temp;
}

uint16_t D2H(int16_t s16_inpdat) {
  uint16_t u16_temp;

  if (s16_inpdat < 0) {
    u16_temp = (uint16_t)(s16_inpdat + 65536);
  } else {
    u16_temp = (uint16_t)s16_inpdat;
  }

  return u16_temp;
}
