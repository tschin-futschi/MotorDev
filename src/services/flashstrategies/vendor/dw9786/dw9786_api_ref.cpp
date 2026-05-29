/************************************************************************/
/* Dongwoon anatech OIS
/***********************************************************************/
/**
  ******************************************************************************
  * File Name           : DW9786_API.c
  * Description         : Main program source
  * DongWoon Anatech    : 
  * Version             : 0.1
  ******************************************************************************
  *
  * COPYRIGHT(c) 2020 DWANATECH
  * DW9786 Setup Program for Module Company
  * Revision History
  * 2022.08.30 SJ cho // v1.0.0.1
  *	2022.10.18 SJ cho // for DJI, hall_cal WAIT_TIME increase v1.0.0.2
  *	2022.11.18 SJ cho // register modification of new firmware concept v6.0.0.0
  * 2023.03.02 md kim // v6.0.0.3
  * 2023.05.08 SJ cho // v6.0.0.4 for semco release
  * 2023.05.10 SJ Cho // v6.0.0.5 add function: dw9786_3_axis_servo_off,dw9786_3_axis_servo_on, dw9786_3_axis_servo_on
  *												dw9786_spi_master_mode, dw9786_spi_intercept_mode
  *								  update linearity: write step, point of linearity
  *					  // 5/15 y-axis gyro polarity update
  * 2023.05.23 SJ Cho // update api version v6.0.0.6
  * 2023.06.13 SJ Cho // add int OpenLoopAging(int loop_cnt, int ms_delay) v6.0.0.7
  * 2023.06.22 Juhee  // add dw9786_ldt0_osc_recovery()
  * 				  // modify dw9786_slave_id_change() v6.0.0.8
  * 2024.03.14 SJ Cho // It supports 31 points of AF Linearity compensation. v6.0.0.9
  ******************************************************************************
**/

#include "stdafx.h"
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "dw9786_api_ref.h"
#include "Func.h"
#include "string.h"
#include "math.h"
#include <tchar.h>
#include "mmsystem.h"

#define USER_PT_A	RamWriteA(0xEDB0, 0xA23F)
#define USER_PT_B 	RamWriteA(0xEDBC, 0xDB01)
#define CODE_PT 	RamWriteA(0xE2F8, 0xC0DE)
#define LOGIC_RESET RamWriteA(0xE008, 0x0001)
#define SET_PT		RamWriteA(0xB03E, 0x5959)
#define MODULE_PT	RamWriteA(0xB03E, 0xA6A6)

DW978x_ExtFuncList* g_stExtFnList = NULL;

extern	ptrWriteI2CDev WriteI2CDev;
extern	ptrReadI2CDev ReadI2CDev;
extern	ptrOutputLog  OutputLog;

extern BYTE _SLV_OIS_;
u8 MEM_SLAVE_ID = 0x46;
u8 _SLV_AF_ = 0x18;
int af_control_bit = 12;

int MCS_PKT_SIZE = 16;
tLin Lin[2];
tCross Cross[2];
tCoordi Coordi[2];
stPosture PostureCoor;
sHallAccuracy HallAccuray;
float CoeffiX;
float CoeffiY;

char FW_File_Path[MAX_PATH];

//af linearity compensation
double g_fRealStroke[50] = { 0 };
double g_fIdealStroke[50] = { 0 };
double g_fRealTargetScale[50] = { 0 };
double target_interval;

double g_nTargetPoint[50];
double g_nCompenPoint[50];
int nTargetcnt = 21; // 31
int Target_range = 4095; // 16383

#if 0
TCHAR modulePath[_MAX_DIR];
TCHAR moduleFileName[_MAX_PATH];
#else
extern TCHAR modulePath[_MAX_DIR];
extern TCHAR moduleFileName[_MAX_PATH];
#endif

int REF_STROKE = 120;
int REF_GYRO_RESULT = 4000;

void m_delay(unsigned int delay)
{
	DWORD msec = delay;
	DWORD dwStart;
	if (msec == 0) {
		msec = 1;
	}
	timeBeginPeriod(2);
	dwStart = timeGetTime();
	while(1) {
		// counter retuens zero when the system continues to work for 49.7 days
		if((timeGetTime() - dwStart) >= msec){
			break;
		}
	}
	timeEndPeriod(2);
}

int wait_check_register(u16 reg, u16 ref)
{
	/* 
	reg : read target register
	ref : compare reference data
	*/
	int ret = 0;
	u16 r_data;
	int i=0;

	for(i = 0; i < LOOP_A; i++) {
		RamReadA(reg, &r_data); //Read status
		if(r_data == ref) {
			ret = FUNC_PASS;
			break;
		}
		else {
			if (i >= LOOP_B) {
				log_tprintf(1, _T("[wait_check_register]fail: 0x%04X"), r_data);
				return EXECUTE_ERROR;
			}
		}
		m_delay(WAIT_TIME);
	}	
	return ret;
}

int dw978x_extfuncinit(DW978x_ExtFuncList* pExtFuncList)
{
	g_stExtFnList = pExtFuncList;

	log_tprintf(1, _T("[dw978x_extfuncinit] start...."));
	
	if (g_stExtFnList == NULL) {
		return EXTFUNC_LOAD_ERROR;
	}
	else {
		WriteI2CDev = (ptrWriteI2CDev)g_stExtFnList->pFunc_I2C_Write;
		ReadI2CDev = (ptrReadI2CDev)g_stExtFnList->pFunc_I2C_Read;
		OutputLog = (ptrOutputLog) g_stExtFnList->pFunc_OutputLog;
	}

	/* Get DLL version */
	int verInfoSize = GetFileVersionInfoSize(moduleFileName, 0);
	if( verInfoSize ) {
		TCHAR	*versionInfo = new TCHAR[verInfoSize+1];
		LPVOID	pvBuf = NULL;
		PUINT	uLen = NULL;

		GetFileVersionInfo(moduleFileName, 0, verInfoSize, versionInfo);
		VerQueryValue(versionInfo, _T("\\"), &pvBuf, uLen);

		VS_FIXEDFILEINFO *pvfi = (VS_FIXEDFILEINFO *)pvBuf;
		log_tprintf(1, _T("DLL Version = %d.%d.%d.%d"), pvfi->dwFileVersionMS >> 16, pvfi->dwFileVersionMS & 0x0000ffff, pvfi->dwFileVersionLS >> 16, pvfi->dwFileVersionLS & 0x0000ffff);
		delete[] versionInfo;
	}
	return FUNC_PASS;
}


int dw9786_id_check(void)
{
	/*
	Error code definition
	0 : No Error
	-1 : DW9786_CHIP_ID miss match
	*/
	u16 chip_id;
	log_tprintf(1, _T("[dw9786_id_check] start...."));
	
	RamReadA(0xE018, &chip_id); // ID read
	/* Check the chip_id of OIS ic */
	if (chip_id != DW9786_CHIP_ID) {
		log_tprintf(1, _T("[dw9786_id_check] the module's OIS IC is not dw9786 (0x%04X)"), chip_id);
		return -1;
	}
	log_tprintf(1, _T("[dw9786_id_check] The module's OIS IC is (0x%04X)"), chip_id);
	return chip_id;
}

int dw9786_auto_read_check(void)
{
	/*
	Error code definition
	0 : No Error
	others : auto read fail
	*/
	u16 cksum[2];
	u16 autord_rv_error = 0;
	log_tprintf(1, _T("[dw9786_auto_read_check] start...."));

#if 1
	/* Set the checksum area */
	CnRd(0xED60, cksum, 2);
	log_tprintf(1, _T("MCS_CHKSUM: 0x%4X%4X"), cksum[0], cksum[1]);
#endif

	/* Check if flash data is normally auto read */
	RamReadA(0xED5C, &autord_rv_error);
	if (autord_rv_error != AUTORD_RV_OK) {
		dw9786_chip_enable(MODE_OF);
		log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 auto_read fail (0x%04X)"), autord_rv_error);
		if ((autord_rv_error & 0xFFFF) == 0x0001)
			log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 LUT_ERR"));
		if ((autord_rv_error & 0xFFFF) == 0x0002)
			log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 PGM_ERR"));
		if ((autord_rv_error & 0xFFFF) == 0x0004)
			log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 LDT0_ERR"));
		if ((autord_rv_error & 0xFFFF) == 0x0008)
			log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 RED_ERR"));
		if ((autord_rv_error & 0xFFFF) == 0x0010)
			log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 DCT_ERR"));
	}
	else {
		log_tprintf(1, _T("[dw9786_auto_read_check] dw9786 auto-lead function passed"));
	}

	return autord_rv_error;
}

void dw9786_file_path(char *path)
{
	 memcpy(FW_File_Path,path, MAX_PATH);
	 log_tprintf(1, _T("[dw9786_file_path] fw file path: %s"), FW_File_Path);
}

/* ============================================================
 * [MOTORDEV PATCH 2026-05-29] Progress / cancel hook globals.
 * Set via dw9786_set_progress_cb / _cancel_check; NULL when unused.
 * ============================================================ */
static dw9786_progress_cb_t  g_dw9786_progress_cb = NULL;
static dw9786_cancel_check_t g_dw9786_cancel_check = NULL;

void dw9786_set_progress_cb(dw9786_progress_cb_t cb)   { g_dw9786_progress_cb = cb; }
void dw9786_set_cancel_check(dw9786_cancel_check_t cb) { g_dw9786_cancel_check = cb; }

#define DW9786_PATCH_REPORT(pct)    do { if (g_dw9786_progress_cb) g_dw9786_progress_cb(pct); } while (0)
#define DW9786_PATCH_CANCELLED()    (g_dw9786_cancel_check && g_dw9786_cancel_check())

/* ============================================================
 * [MOTORDEV PATCH 2026-05-29] FW update accepting external buffer.
 * Body lifted from dw9786_fw_download(); fopen/fgets parsing removed.
 * Erase loop + sequential write loop instrumented with progress + cancel hooks.
 * Progress model:
 *   ERASE phase:  0% .. 5%   (FMC_PAGE sectors)
 *   WRITE phase:  5% .. 100% (MCS_SIZE_W / MCS_PKT_SIZE packets)
 * ============================================================ */
int dw9786_fw_download_with_buffer(const u16 *fw_data, u32 word_count)
{
	int i = 0;
	int msg = 0;
	u16 addr = 0;
	u32 checksum_criteria, checksum_flash;

	log_tprintf(1, _T("[dw9786_download_fw_with_buffer] start (word_count=%u)"), word_count);

	if (fw_data == NULL || word_count != (u32)MCS_SIZE_W) {
		log_tprintf(1, _T("[dw9786_download_fw_with_buffer] invalid args"));
		return EXECUTE_ERROR;
	}

	/*Flash lut unlocked*/
	dw9786_flash_acess(MCS_CODE);

	/* [MOTORDEV PATCH] 进度上报单位 = 千分位 (0..10000)，为提升 UI 平滑度。 */
	DW9786_PATCH_REPORT(0);

	/* Sector erase — 不上报进度，进度条停在 0% 对应真实 IC erase 操作（~100ms）。
	 * 与 HL9788N v4 一致：按"已烧字节 / 总字节"线性，erase/checksum/verify 自然停顿。
	 */
	for (i = 0; i < FMC_PAGE; i++) {
		if (DW9786_PATCH_CANCELLED()) {
			log_tprintf(1, _T("[dw9786_download_fw_with_buffer] cancelled at erase i=%d"), i);
			return DW9786_FW_CANCELLED;
		}
		if (!i) addr = MCS_START_ADDRESS;
		RamWriteA(0xED08, addr);   // Set erase address
		RamWriteA(0xED0C, 0x0002); // Sector Erase(2KB)
		addr += 0x800;
		m_delay(5);
	}

	/* flash fw write — 每个 packet 后按"已烧字节 / 总字节"线性上报 0~100%。
	 * (i + MCS_PKT_SIZE) 是已写完的 word 数，total = MCS_SIZE_W = 20480 word。
	 */
	for (i = 0; i < (int)MCS_SIZE_W; i += MCS_PKT_SIZE) {
		if (DW9786_PATCH_CANCELLED()) {
			log_tprintf(1, _T("[dw9786_download_fw_with_buffer] cancelled at write i=%d"), i);
			return DW9786_FW_CANCELLED;
		}
		if (i == 0) RamWriteA(0xED28, MCS_START_ADDRESS); // 16bit write
		/* program sequential write — vendor original cast: (u16 *)(buf + i) */
		CntWrt(0xED2C, (u16 *)(fw_data + i), MCS_PKT_SIZE); // 16bit write
		/* 千分位精度 (0..10000)：1280 packet × 7.8 ‰/packet → 每 packet ~7.8 milli 增量，UI 顺滑 */
		DW9786_PATCH_REPORT((int)((long)(i + MCS_PKT_SIZE) * 10000L / (long)MCS_SIZE_W));
	}

	/* Checksum calculation for mcs data */
	checksum_criteria = checksum_func((u16 *)fw_data, MCS_SIZE_W);
	/* Set the checksum area */
	checksum_flash = dw9786_checksum_func(MCS_CODE);

	if (checksum_criteria != checksum_flash) {
		msg = CHECKSUM_ERROR;
		log_tprintf(1, _T("[dw9786_download_fw_with_buffer] flash checksum failed!, checksum_criteria: 0x%08X, checksum_flash: 0x%08X"), checksum_criteria, checksum_flash);
		dw9786_chip_enable(MODE_OF);
		return msg;
	} else {
		log_tprintf(1, _T("[dw9786_download_fw_with_buffer] flash checksum passed!, checksum_criteria: 0x%08X, checksum_flash: 0x%08X"), checksum_criteria, checksum_flash);
	}

	LOGIC_RESET;
	log_tprintf(1, _T("[dw9786_download_fw_with_buffer] Logic reset"));
	log_tprintf(1, _T("[dw9786_download_fw_with_buffer] finished!"));
	m_delay(10);

	DW9786_PATCH_REPORT(10000);  /* 千分位 = 100% */
	return msg;
}

// FLASH_MCS FW File : Hex
int dw9786_fw_download(void)
{
	/* [MOTORDEV PATCH 2026-05-29] 原函数保留，内部 fopen 解析后 forward 到 _with_buffer，
	 * 保持 vendor 内部调用向后兼容。MotorDev 上位机不会调用此函数（走 _with_buffer 路径）。
	 */
	FILE *p_file = NULL;
	int idx = 0;
	int temp = 0;
	char buffer[MAX_CHAR_LENGTH];
	static u16 DW9786_MCS_Buf[MCS_SIZE_W];
	int i;

	for (i = 0; i < MCS_SIZE_W; i++) DW9786_MCS_Buf[i] = 0;

	log_tprintf(1, _T("[dw9786_download_fw] start...."));

	if (0 == fopen_s(&p_file, FW_File_Path, "rt")) {
		while (fgets(buffer, MAX_CHAR_LENGTH, p_file) != NULL) {
			temp = strtoul(buffer, NULL, 16);
			DW9786_MCS_Buf[idx + 0] = (u16)(temp >> 16);
			DW9786_MCS_Buf[idx + 1] = (u16)(temp >> 0);
			idx += 2;
		}
		fclose(p_file);
		log_tprintf(1, _T("fw file open complete"));
	} else {
		log_tprintf(1, _T("fw file open failed"));
		return -1;
	}

	return dw9786_fw_download_with_buffer(DW9786_MCS_Buf, (u32)MCS_SIZE_W);
}

void dw9786_module_init_set(void)
{
	log_tprintf(1, _T("[dw9786_module_init_set] start...."));

	/* Fixed servo loop gain */
	RamWriteA(0xB416, 0x0800); //X Servo_loop_gain
	RamWriteA(0xB516, 0x0800); //Y Servo_loop_gain
	RamWriteA(0xB616, 0x0800); //Z Servo_loop_gain
	/* main gyro bottom 180 degree */
	RamWriteA(0xB836, 0x0005); //ICM-42631 AUX1
	RamWriteA(0xB800, 0x0001); //X Gyro gain polarity
	RamWriteA(0xB802, 0xFFFF); //Y Gyro gain polarity //230515
	RamWriteA(0xB812, 0x8000); //GYRO_ROT_MAT_X cosine theta
	RamWriteA(0xB814, 0x8000); //GYRO_ROT_MAT_Y cosine theta
	RamWriteA(0xB818, 0x0000); //GYRO_ROT_MAT_X -sine theta
	RamWriteA(0xB81A, 0x0000); //GYRO_ROT_MAT_Y sine theta
	log_tprintf(1, _T("[dw9786_module_init_set] finish...."));
}

void dw9786_module_DJI_init_set(void)
{
	log_tprintf(1, _T("[dw9786_module_DJI_init] start...."));
	RamWriteA(0xB4A0, 0x02E4); // T  (ADDR : 0xB4A0) :  740
	RamWriteA(0xB4A2, 0x00DC); // Ta (ADDR : 0xB4A2) :  220  
	RamWriteA(0xB4A4, 0x0172); // Tb (ADDR : 0xB4A4) :  370 
	log_tprintf(1, _T("[dw9786_module_DJI_init] finish...."));
}

void dw9786_flash_acess(int type)
{	
	log_tprintf(1, _T("[dw9786_flash_acess] start...."));

	dw9786_chip_enable(MODE_ON);
	dw9786_mcu_active(MODE_OF);	
	
	if(type == MCS_LUT) {
		USER_PT_B;
	}
	else if(type == MCS_CODE) {
		CODE_PT;
	}
	else if(type == MCS_USER) {
		USER_PT_A;
	}
	RamWriteA(0xED00, 0x0000);
	m_delay(1);
	log_tprintf(1, _T("[dw9786_flash_acess] finish...."));
}

int dw9786_checksum_func(int type)
{
	/*
	Error code definition
	0 : No Error
	-1 : CHECKSUM_ERROR
	*/
	u16 csh, csl;
	unsigned int mcs_checksum_flash;
	log_tprintf(1, _T("[dw9786_checksum_func] start...."));

	/* Set the checksum area */
	if(type == MCS_LUT) {
		//not used
	}
	else if(type == MCS_CODE) {
		dw9786_flash_acess(MCS_CODE);
		RamWriteA(0xED48, MCS_START_ADDRESS);
		RamWriteA(0xED4C, MCS_CHECKSUM_SIZE);
		RamWriteA(0xED50, 0x0001);		
	}
	wait_check_register(0xED04, 0x00);	
	
	RamReadA(0xED54, &csh);
	RamReadA(0xED58, &csl);
	mcs_checksum_flash = ((unsigned int)(csh << 16)) | csl;
	log_tprintf(1, _T("[dw9786_checksum_func] flash memory checksum , [0x%08X]"), mcs_checksum_flash);
	log_tprintf(1, _T("[dw9786_checksum_func] finish...."));
	return mcs_checksum_flash;
}

int dw9786_hall_calibration(int axis_cnt)
{
	/*
	* dw9786 hall calibration
	
	Error code definition
		-1 : EXECUTE_ERROR
		0 : No Error

	*/
	int msg = 0;
	u16 r_data;
	log_tprintf(1, _T("[dw9786_hall_calibration] start...."));

	dw9786_reset();
	if( axis_cnt >= 1)
		msg += dw9786_x_hall_calibration();
	
	if( axis_cnt >= 2)
		msg += dw9786_y_hall_calibration();
	
	if( axis_cnt >= 3)
		msg += dw9786_af_hall_calibration();
	
	if(msg ==0){
		dw9786_module_cal_store();
	}
	else {
		log_tprintf(1, _T("[dw9786_hall_calibration] hall calibration fail"));
	}
	return msg;
}

int dw9786_x_hall_calibration(void)
{
	/*
	* dw9786 hall calibration
	Error code definition
		-1 : EXECUTE_ERROR
		0 : No Error
	*/
	int msg = 0;
	u16 r_data;
	log_tprintf(1, _T("[dw9786_x_hall_calibration] start...."));
	
	RamWriteA(0xB026, 0x0003); // hall calibration mode
	m_delay(1);

	if(wait_check_register(0xB020, 0x3000) == FUNC_PASS){
		RamWriteA(0xB022, 0x0001); //x axis hall calibration execute command
		m_delay(100);
	}
	else{
		log_tprintf(1, _T("[dw9786_x_hall_calibration] hall status error"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0x3001) == FUNC_PASS){
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis_calibration function finish"));
	}
	else {
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis_calibration function error"));
			return EXECUTE_ERROR;
	}
	
	RamReadA(0xB700, &r_data);
	if((r_data & 0x8000) == 0x8000)
	{
		if (r_data & X_AXIS_HALL_CAL_PASS) { 
			msg += OK; 
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis_hall_cal_pass"));
		}
		if (r_data & X_AXIS_AMP_OFS_ERR) { 
			msg += X_AXIS_AMP_OFS_ERR; 
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis hall amp offset error"));
		}
		if (r_data & X_AXIS_RETRY_ERR) { 
			msg += X_AXIS_RETRY_ERR; 
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis hall calibration retry over"));
		}
		if (r_data & X_AXIS_DIRECTION_ERR) { 
			msg += X_AXIS_DIRECTION_ERR; 
			log_tprintf(1, _T("[dw9786_x_hall_calibration] x_axis hall direction error"));
		}
	}else
	{
		log_tprintf(1, _T("[dw9786_x_hall_calibration] calibration done fail"));
		return HALL_CAL_DONE_FAIL;
	}
	return msg;

}

int dw9786_y_hall_calibration(void)
{
	/*
	* dw9786 hall calibration
	Error code definition
		-1 : EXECUTE_ERROR
		0 : No Error
	*/
	int msg = 0;
	u16 r_data;
	log_tprintf(1, _T("[dw9786_y_hall_calibration] start...."));
	
	RamWriteA(0xB026, 0x0003); // hall calibration mode
	m_delay(1);

	if(wait_check_register(0xB020, 0x3000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0003); //y axis hall calibration execute command
		m_delay(300);
	}
	else {
			log_tprintf(1, _T("[dw9786_y_hall_calibration] hall status EXECUTE_ERROR"));
			return EXECUTE_ERROR;
	}
	if(wait_check_register(0xB020, 0x3003) == FUNC_PASS){
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis_calibration function finish"));
	}
	else {
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis_calibration function error"));
			return EXECUTE_ERROR;
	}
	RamReadA(0xB700, &r_data);
	if((r_data & 0x8000) == 0x8000)
	{
		if (r_data & Y_AXIS_HALL_CAL_PASS) { 
			msg += OK; 
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis_hall_cal_pass"));
		}
		if (r_data & Y_AXIS_AMP_OFS_ERR) { 
			msg += Y_AXIS_AMP_OFS_ERR; 
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis hall amp offset error"));
		}
		if (r_data & Y_AXIS_RETRY_ERR) { 
			msg += Y_AXIS_RETRY_ERR; 
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis hall calibration retry over"));
		}
		if (r_data & Y_AXIS_DIRECTION_ERR) { 
			msg += Y_AXIS_DIRECTION_ERR; 
			log_tprintf(1, _T("[dw9786_y_hall_calibration] y_axis hall direction error"));
		}
	}else
	{
		log_tprintf(1, _T("[dw9786_y_hall_calibration] calibration done fail"));
		return HALL_CAL_DONE_FAIL;
	}
	return msg;
}

int dw9786_z_hall_calibration(void)
{
	/*
	* dw9786 hall calibration
	Error code definition
		-1 : EXECUTE_ERROR
		0 : No Error
	*/
	int msg = 0;
	u16 r_data;
	log_tprintf(1, _T("[dw9786_z_hall_calibration] start "));
	
	RamWriteA(0xB026, 0x0003); // hall calibration mode
	m_delay(1);

	if(wait_check_register(0xB020, 0x3000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0005); //z axis hall calibration execute command
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_z_hall_calibration] hall status EXECUTE_ERROR"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0x3005) == FUNC_PASS){
		log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis_calibration function finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis_calibration function error"));
		return EXECUTE_ERROR;
	}

	RamReadA(0xB700, &r_data);

	if((r_data & 0x8000) == 0x8000)
	{
		if (r_data & Z_AXIS_HALL_CAL_PASS) { 
			msg += OK; 
			log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis_hall_cal_pass"));
		}
		if (r_data & Z_AXIS_AMP_OFS_ERR) { 
			msg += Z_AXIS_AMP_OFS_ERR; 
			log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis hall amp offset error"));
		}
		if (r_data & Z_AXIS_RETRY_ERR) { 
			msg += Z_AXIS_RETRY_ERR; 
			log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis hall calibration retry over"));
		}
		if (r_data & Z_AXIS_DIRECTION_ERR) { 
			msg += Z_AXIS_DIRECTION_ERR; 
			log_tprintf(1, _T("[dw9786_z_hall_calibration] z_axis hall direction error"));
		}
	}else{
		log_tprintf(1, _T("[dw9786_z_hall_calibration] calibration done fail"));
		return HALL_CAL_DONE_FAIL;
	}

	return msg;
}

int dw9786_af_hall_calibration(void)
{
	/*
	* dw9786 hall calibration
	Error code definition
		-1 : EXECUTE_ERROR
		0 : No Error
	*/
	int msg = 0;
	u16 r_data;
	log_tprintf(1, _T("[dw9786_af_hall_calibration] start "));
	
	RamWriteA(0xB026, 0x0003); // hall calibration mode
	m_delay(1);

	if(wait_check_register(0xB020, 0x3000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0005); //af axis hall calibration execute command
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_af_hall_calibration] hall status EXECUTE_ERROR"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0x3005) == FUNC_PASS){
		log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis_calibration function finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis_calibration function error"));
		return EXECUTE_ERROR;
	}

	RamReadA(0xB700, &r_data);

	if((r_data & 0x8000) == 0x8000)
	{
		if (r_data & AF_AXIS_HALL_CAL_PASS) { 
			msg += OK; 
			log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis_hall_cal_pass"));
		}
		if (r_data & AF_AXIS_AMP_OFS_ERR) { 
			msg += AF_AXIS_AMP_OFS_ERR; 
			log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis hall amp offset error"));
		}
		if (r_data & AF_AXIS_RETRY_ERR) { 
			msg += AF_AXIS_RETRY_ERR; 
			log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis hall calibration retry over"));
		}
		if (r_data & AF_AXIS_DIRECTION_ERR) { 
			msg += AF_AXIS_DIRECTION_ERR; 
			log_tprintf(1, _T("[dw9786_af_hall_calibration] af_axis hall direction error"));
		}
	}else
	{
		log_tprintf(1, _T("[dw9786_af_hall_calibration] calibration done fail"));
		return HALL_CAL_DONE_FAIL;
	}

	return msg;
}

int dw9786_servo_loop_calibration(int axis_cnt)
{
	int msg, i = 0;
	u16 r_data, srv_loop_x, srv_loop_y, srv_loop_z;
	log_tprintf(1, _T("[dw9786_servo_loop_calibration] start...."));
	
	RamWriteA(0xB026, 0x0004);		// servo loop calibration mode

	if(wait_check_register(0xB020, 0x4000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0001);		// servo loop calibration execute command
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] lens offset cal status EXECUTE_ERROR"));
		return EXECUTE_ERROR;
	}
	
	if(wait_check_register(0xB020, 0x4001) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] calibration function finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] calibration function error"));
		return EXECUTE_ERROR;
	}
	
	RamReadA(0xB702, &r_data); //servo loop gain status
	log_tprintf(1, _T("[dw9786_servo_loop_calibration] srv_status: 0x%04X"), r_data);

	RamReadA(0xB416, &srv_loop_x);
	RamReadA(0xB516, &srv_loop_y);
	RamReadA(0xB616, &srv_loop_z);
	
	if(axis_cnt >= 1) /* 1-axis, x */
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] x_axis servo loop gain: 0x%04X(%d)"), srv_loop_x, (short)srv_loop_x);
	if(axis_cnt >= 2) /* 2-axis x,y */
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] y_axis servo loop gain: 0x%04X(%d)"), srv_loop_y, (short)srv_loop_y);
	if(axis_cnt >= 3) /* 3-axis x,y,z*/
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] z_axis servo loop gain: 0x%04X(%d)"), srv_loop_z, (short)srv_loop_z);
		

	if((r_data & 0x8000) == 0x8000) 
	{		// Read Gyro offset cailbration result status
		if (axis_cnt >= 1)
		{
			if (r_data & X_AXIS_LENS_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] x_axis servo gain calibration pass"));
			}
			if (r_data & X_AXIS_SERVO_GAIN_MAX_ERR) { 
				msg += X_AXIS_SERVO_GAIN_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] x_axis servo gain max. error"));
			}
			if (r_data & X_AXIS_SERVO_GAIN_MIN_ERR) { 
				msg += X_AXIS_SERVO_GAIN_MIN_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] x_axis servo gain min. error"));
			}
		}
		
		if (axis_cnt >= 2)
		{
			if (r_data & Y_AXIS_SERVO_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] y_axis servo gain calibration pass"));
			}
			if (r_data & Y_AXIS_SERVO_GAIN_MAX_ERR) { 
				msg += Y_AXIS_SERVO_GAIN_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] y_axis servo gain max. error"));
			}
			if (r_data & Y_AXIS_SERVO_GAIN_MIN_ERR) { 
				msg += Y_AXIS_SERVO_GAIN_MIN_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] y_axis servo gain min. error"));
			}
		}
		
		if (axis_cnt >= 3)
		{
			if (r_data & Z_AXIS_SERVO_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] z_axis servo gain calibration pass"));
			}
			if (r_data & Z_AXIS_SERVO_GAIN_MAX_ERR) { 
				msg += Z_AXIS_SERVO_GAIN_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] y_axis servo gain max. error"));
			}
			if (r_data & Z_AXIS_SERVO_GAIN_MIN_ERR) { 
				msg += Z_AXIS_SERVO_GAIN_MIN_ERR; 
				log_tprintf(1, _T("[dw9786_servo_loop_calibration] z_axis servo gain min. error"));
			}
		}
	}else{
		log_tprintf(1, _T("[dw9786_servo_loop_calibration] calibration done fail"));
		return SERVO_CAL_DONE_FAIL;
	}
	return msg;
}

int dw9786_lens_ofs_calibration(int axis_cnt)
{

	int msg, i = 0;
	u16 r_data, lens_x, lens_y, lens_z;
	log_tprintf(1, _T("[dw9786_lens_ofs_calibration] start...."));
	
	RamWriteA(0xB026, 0x0005);		// Hall Calibration mode

	if(wait_check_register(0xB020, 0x5000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0001);		// Lens ofs calibration execute command
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] lens offset cal status EXECUTE_ERROR"));
		return EXECUTE_ERROR;
	}
	
	if(wait_check_register(0xB020, 0x5001) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] calibration function finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] calibration function error"));
		return EXECUTE_ERROR;
	}
	
	RamReadA(0xB706, &r_data); //lens offset status
	log_tprintf(1, _T("[dw9786_lens_ofs_calibration] lens_status: 0x%04X"), r_data);

	RamReadA(0xB414, &lens_x);
	RamReadA(0xB514, &lens_y);
	RamReadA(0xB614, &lens_z);
	
	if (axis_cnt >= 1)
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] x_lens_ofs: 0x%04X(%d)"), lens_x, (short)lens_x);
	if (axis_cnt >= 2)
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] y_lens_ofs: 0x%04X(%d)"), lens_y, (short)lens_y);
	if (axis_cnt >= 3)
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] z_lens_ofs: 0x%04X(%d)"), lens_z, (short)lens_z);


	if((r_data & 0x8000) == 0x8000) {
		
		if (axis_cnt >= 1)
		{
			if (r_data & X_AXIS_LENS_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] x_axis lens offset calibration pass"));
			}
			if (r_data & X_AXIS_LENS_OFS_MAX_ERR) { 
				msg += X_AXIS_LENS_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] x_axis lens offset max. limit error"));
			}
		}
		if (axis_cnt >= 2)
		{
			if (r_data & Y_AXIS_LENS_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] y_axis lens offset calibration pass"));
			}
			if (r_data & Y_AXIS_LENS_OFS_MAX_ERR) { 
				msg += Y_AXIS_LENS_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] y_axis lens offset max. limit error"));
			}
		}
		if (axis_cnt >= 3)
		{
			if (r_data & Z_AXIS_LENS_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] y_axis lens offset calibration pass"));
			}
			if (r_data & Z_AXIS_LENS_OFS_MAX_ERR) { 
				msg += Z_AXIS_LENS_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_lens_ofs_calibration] y_axis lens offset max. limit error"));
			}
		}
	}else{
		log_tprintf(1, _T("[dw9786_lens_ofs_calibration] calibration done fail"));
		return LENS_OFS_CAL_DONE_FAIL;
	}
	return msg;
}

int dw9786_gyro_ofs_calibration(int axis_cnt)
{
	/*
	* dw9786 gyro offset calibration
	Error code definition	
		-1 : EXECUTE_ERROR
		0 : No Error

	*/
	int msg = 0;
	u16 r_data, x_ofs, y_ofs, z_ofs;
	log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] start...."));
	
	RamWriteA(0xB026, 0x0006); // gyro offset calibration
	m_delay(100);

	if(wait_check_register(0xB020, 0x6000) == FUNC_PASS) {
		RamWriteA(0xB022, 0x0001);		// Lens ofs calibration execute command
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] gyro offset EXECUTE_ERROR"));
		return EXECUTE_ERROR;
	}


	if(wait_check_register(0xB020, 0x6001) == FUNC_PASS) { // when calibration is done, Status changes to 0x6001
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] calibration function finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] calibration function error"));
		return EXECUTE_ERROR;
	}
	
	RamReadA(0xB80C, &x_ofs); //x gyro offset
	RamReadA(0xB80E, &y_ofs); //y gyro offset
	RamReadA(0xB810, &z_ofs); //z gyro offset
	RamReadA(0xB838, &r_data); //gyro offset status
	
	log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] gyro_status: 0x%04X"), r_data);
	
	if (axis_cnt >= 1)
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] x gyro offset: 0x%04X(%d)"), x_ofs, (short)x_ofs);
	
	if (axis_cnt >= 2)
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] y gyro offset: 0x%04X(%d)"), y_ofs, (short)y_ofs);
	
	if (axis_cnt >= 3)
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] z gyro offset: 0x%04X(%d)"), z_ofs, (short)z_ofs);
	
	

	if((r_data & 0x8000) == 0x8000) { // Read Gyro offset cailbration result status
		if (axis_cnt >= 1)
		{
			if (r_data & X_AXIS_GYRO_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] x_axis gyro offset calibration pass"));
			}
			if (r_data & X_AXIS_GYRO_OFS_MAX_ERR) { 
				msg += X_AXIS_GYRO_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] x_axis gyro offset max. error"));
			}		
		}
		if (axis_cnt >= 2)
		{
			if (r_data & Y_AXIS_GYRO_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] y_axis gyro offset calibration pass"));
			}
			if (r_data & Y_AXIS_GYRO_OFS_MAX_ERR) { 
				msg += Y_AXIS_GYRO_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] y_axis gyro offset max. error"));
			}		
		}
		if (axis_cnt >= 3)
		{
			if (r_data & Z_AXIS_GYRO_OFS_CAL_PASS) { 
				msg += OK; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] z_axis gyro offset calibration pass"));
			}
			if (r_data & Z_AXIS_GYRO_OFS_MAX_ERR) { 
				msg += Z_AXIS_GYRO_OFS_MAX_ERR; 
				log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] z_axis gyro offset max. error"));
			}
		}
	} 
	else {
		msg += GYRO_OFS_CAL_DONE_FAIL;
		log_tprintf(1, _T("[dw9786_gyro_ofs_calibration] calibration done fail"));
	}

	return msg;
}

int dw9786_sel_gyro_sensor(int sel_gyro)
{
	log_tprintf(1, _T("[dw9786_sel_gyro_sensor] start...."));
	if(sel_gyro == 0)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ICM_20690"));
	else if(sel_gyro == 2)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ST_LSM6DSM"));
	else if(sel_gyro == 4)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ST_LSM6DSOQ"));
	else if(sel_gyro == 5)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ICM-42631 - AUX1"));
	else if(sel_gyro == 6)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] BMI260"));
	else if(sel_gyro == 7)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ICM42602"));
	else if(sel_gyro == 8)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ICM-42631 - AUX2"));
	else if(sel_gyro == 9)
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] ICM-40608"));
	else{
		log_tprintf(1, _T("[dw9786_sel_gyro_sensor] gyro sensor selection failed"));
		return FUNC_FAIL;
	}
	RamWriteA(0xB836, sel_gyro);

	return FUNC_PASS;
}

int dw9786_hall_margin_adjustment(stHallMargin *hall_epa)
{
	int msg = 0;
	log_tprintf(1, _T("[dw9786_hall_margin_adjustment] start...."));

	//msg = dw9786_servo_on();
	msg = dw9786_3_axis_servo_on(); // for semco : x,y,af

	if(msg == FUNC_PASS)
	{
		RamWriteA(0xB40A, hall_epa->x_pos_margin * 10); // set x axis positive position margin code
		RamWriteA(0xB40C, hall_epa->x_neg_margin * 10); // set x axis negative position margin code
		RamWriteA(0xB50A, hall_epa->y_pos_margin * 10); // set y axis positive position margin code
		RamWriteA(0xB50C, hall_epa->y_neg_margin * 10); // set y axis negative position margin code
		RamWriteA(0xB60A, hall_epa->af_pos_margin * 10); // set af axis positive position margin code
		RamWriteA(0xB60C, hall_epa->af_neg_margin * 10); // set af axis negative position margin code
		log_tprintf(1, _T("[dw9786_hall_margin_adjustment] adjustment finish"));
	}
	else {
		log_tprintf(1, _T("[dw9786_hall_margin_adjustment] adjustment failed"));
	}

	return msg;
}

void dw9786_chip_info(void)
{
	u16 r_data;
	log_tprintf(1, _T("[dw9786_chip_info] start...."));

	RamReadA(0xE018, &r_data);
	log_tprintf(1, _T("[dw9786_chip_info] product_id: 0x%04X"), r_data);
	RamReadA(0xB000, &r_data);
	log_tprintf(1, _T("[dw9786_chip_info] chip_id: 0x%04X"), r_data);
	RamReadA(0xB002, &r_data);
	log_tprintf(1, _T("[dw9786_chip_info] Fw_ver_H: 0x%04X"), r_data);
	RamReadA(0xB004, &r_data);
	log_tprintf(1, _T("[dw9786_chip_info] Fw_ver_L: 0x%04X"), r_data);
	RamReadA(0xB006, &r_data);
	log_tprintf(1, _T("[dw9786_chip_info] Fw_date: 0x%04X"), r_data);
}

void dw9786_fw_info(void)
{
	u16 r_data;
	log_tprintf(1, _T("[dw9786_fw_info] start...."));

	RamReadA(0x9800, &r_data);
	log_tprintf(1, _T("[dw9786_fw_info] set version: 0x%02X [MSB], project no.:0x%02X [LSB]"), r_data>>8, r_data&0xff);
	RamReadA(0x9802, &r_data);
	log_tprintf(1, _T("[dw9786_fw_info] fw version: 0x%02X [MSB], pid version:0x%02X [LSB]"), r_data>>8, r_data&0xff);
	RamReadA(0x9804, &r_data);
	log_tprintf(1, _T("[dw9786_fw_info] fw_date: 0x%04X"), r_data);
}

int dw9786_module_cal_store(void)
{
	// sj_update
	/*
	Error code definition
	0 : No Error
	-1 : EXECUTE_ERROR
	*/
	log_tprintf(1, _T("[dw9786_module_cal_store] start...."));
	UserByteWriteA(MEM_SLAVE_ID, 0x97E6, 0xEA); //USER_PT_C off
	UserByteWriteA(MEM_SLAVE_ID, 0x3FFF, 0x00); //User memory dummy write
	m_delay(10); //Absolutely necessary
	RamWriteA(0xB026, 0x000A); //Store&Erase
	
	if(wait_check_register(0xB020, 0xA000) == FUNC_PASS) {
		USER_PT_A;
		MODULE_PT;
		RamWriteA(0xB022, 0x0001); //Store execute
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_module_cal_store]execute fail"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0xA001) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_module_cal_store]complete"));
	}
	else {
		log_tprintf(1, _T("[dw9786_module_cal_store]fail"));
		return EXECUTE_ERROR;
	}

	return FUNC_PASS;
}

int dw9786_set_cal_store(void)
{
	// sj_update
	/*
	Error code definition
	0 : No Error
	-1 : EXECUTE_ERROR
	*/
	log_tprintf(1, _T("[dw9786_set_store] start...."));
	UserByteWriteA(MEM_SLAVE_ID, 0x97E6, 0xEA); //USER_PT_C off
	UserByteWriteA(MEM_SLAVE_ID, 0x3FFF, 0x00); //User memory dummy write
	m_delay(10); //Absolutely necessary
	RamWriteA(0xB026, 0x000A); //Store&Erase
	
	if(wait_check_register(0xB020, 0xA000) == FUNC_PASS) {
		USER_PT_A;
		SET_PT;
		RamWriteA(0xB022, 0x0001); //Store execute
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_set_cal_store]execute fail"));
		return EXECUTE_ERROR;
	}

	if (wait_check_register(0xB020, 0xA001) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_set_cal_store]complete"));
	}
	else {
		log_tprintf(1, _T("[dw9786_set_cal_store]fail"));
		return EXECUTE_ERROR;
	}

	return FUNC_PASS;
}


int dw9786_module_cal_erase(void)
{
	// sj_update
	/*
	Error code definition
	0 : No Error
	-1 : EXECUTE_ERROR
	*/

	u16 r_data;
	log_tprintf(1, _T("[dw9786_module_cal_erase] start...."));

	RamWriteA(0xB026, 0x000B); //Store&Erase

	if(wait_check_register(0xB020, 0xB000) == FUNC_PASS) {	//Read Status
		USER_PT_A;
		MODULE_PT;
		RamWriteA(0xB022, 0x0001); //Erase
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_module_cal_erase]fail"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0xB001) == FUNC_PASS) {	//Read Status
		log_tprintf(1, _T("[dw9786_module_cal_erase]complete"));
	}
	else {
		log_tprintf(1, _T("[dw9786_module_cal_erase]fail"));
		RamWriteA(0x0220, 0x0000); // Code PT on
		return EXECUTE_ERROR;
	}

	return FUNC_PASS;
}

int dw9786_set_cal_erase(void)
{
	// sj_update
	/*
	Error code definition
	0 : No Error
	-1 : EXECUTE_ERROR
	*/

	u16 r_data;
	log_tprintf(1, _T("[dw9786_set_cal_erase] start...."));

	RamWriteA(0xB026, 0x000B); //Store&Erase

	if(wait_check_register(0xB020, 0xB000) == FUNC_PASS) {	//Read Status
		USER_PT_A;
		SET_PT;
		RamWriteA(0xB022, 0x0001); //Erase
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_set_cal_erase]fail"));
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0xB001) == FUNC_PASS) {	//Read Status
		log_tprintf(1, _T("[dw9786_set_cal_erase]complete"));
	}
	else {
		log_tprintf(1, _T("[dw9786_set_cal_erase]fail"));
		RamWriteA(0x0220, 0x0000); // Code PT on
		return EXECUTE_ERROR;
	}

	return FUNC_PASS;
}

int dw9786_load(void)
{
	/*
	Error code definition
	0 : No Error
	-1 : EXECUTE_ERROR
	*/
	u16 r_data;
	log_tprintf(1, _T("[dw9786_load] start...."));

	RamWriteA(0xB026, 0x000C); //Load

	if(wait_check_register(0xB020, 0xC000) == FUNC_PASS) {	//Read Status
		RamWriteA(0xB022, 0x0001); //Load execute
		m_delay(100);
	}
	else {
		log_tprintf(1, _T("[dw9786_load]execute fail : 0x%04X"), r_data);
		return EXECUTE_ERROR;
	}

	if(wait_check_register(0xB020, 0xC001) == FUNC_PASS) {	//Read Status
		log_tprintf(1, _T("[dw9786_load]complete"));
	}
	else {
		log_tprintf(1, _T("[dw9786_load]fail: 0x%04X"), r_data);
	}

	return FUNC_PASS;
}

int dw9786_reset(void)
{
	log_tprintf(1, _T("[dw9786_reset] start...."));

	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(10);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);
	RamWriteA(0xE004, 0x0001); //idle mode MCU active
	m_delay(100);
	log_tprintf(1, _T("[dw9786_reset]reset execution complete"));
	return FUNC_PASS;
}

int dw9786_mcu_active(u16 mode)
{
	if(mode == 0) {
		log_tprintf(1, _T("[dw9786_mcu_active]dw9786_standby"));
		RamWriteA(0xE004, mode);
	}
	else {
		log_tprintf(1, _T("[dw9786_mcu_active]dw9786_idle mode"));
		RamWriteA(0xE004, mode);
	}
	return FUNC_PASS;
}

int dw9786_chip_enable(u16 en)
{
	if(en == 0) {
		log_tprintf(1, _T("[dw9786_chip_enable]dw9786_sleep"));
		RamWriteA(0xE000, en);
	}
	else {
		log_tprintf(1, _T("[dw9786_chip_enable]dw9786_standby"));
		RamWriteA(0xE000, en);
	}
	return FUNC_PASS;
}

int dw9786_user_protection(void)
{
	USER_PT_A;
	log_tprintf(1, _T("[dw9786_user_protection]excute"));
	return FUNC_PASS;
}


int dw9786_servo_off(void)
{
	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0000); //x,y servo off
	m_delay(1);

	if(wait_check_register(0xB020, 0x1000) == FUNC_PASS) {	//busy check
		log_tprintf(1, _T("[dw9786_servo_off]servo off success"));
		return FUNC_PASS;
	}
	
	return FUNC_FAIL;
}

int dw9786_servo_on(void)
{
	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0002); //servo on
	m_delay(1);
	if(wait_check_register(0xB020, 0x1002) == FUNC_PASS) { //busy check
		log_tprintf(1, _T("[dw9786_servo_on]servo on success"));
		return FUNC_PASS;
	}

	return FUNC_FAIL;
}

int dw9786_ois_on(void)
{
	u16 dat;

	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0001); //x,y ois on
	m_delay(1);

	if(wait_check_register(0xB020, 0x1001) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_ois_on]ois on success"));
		return FUNC_PASS;
	}

	return FUNC_FAIL;
}

int dw9786_3_axis_servo_off(void)
{
	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0000); //ois servo off
	m_delay(1);
	RamWriteA(0xB024, 0x0000); //af servo off
	m_delay(1);
	if(wait_check_register(0xB020, 0x1000) == FUNC_PASS) { //busy check
		log_tprintf(1, _T("[dw9786_3_axis_servo_off]servo off success"));
		return FUNC_PASS;
	}

	return FUNC_FAIL;
}

int dw9786_3_axis_servo_on(void)
{
	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0002); //ois servo on
	m_delay(1);
	RamWriteA(0xB024, 0x0002); //af servo on
	m_delay(1);
	if(wait_check_register(0xB020, 0x1022) == FUNC_PASS) { //busy check
		log_tprintf(1, _T("[dw9786_3_axis_servo_on]servo on success"));
		return FUNC_PASS;
	}

	return FUNC_FAIL;
}

int dw9786_3_axis_ois_on(void)
{
	u16 dat;

	RamWriteA(0xB026, 0x0001); //operate mode
	m_delay(1);
	RamWriteA(0xB022, 0x0001); //x,y ois on
	m_delay(1);
	RamWriteA(0xB024, 0x0002); //af servo on
	m_delay(1);
	if(wait_check_register(0xB020, 0x1021) == FUNC_PASS) {
		log_tprintf(1, _T("[dw9786_3_axis_ois_on]ois on success"));
		return FUNC_PASS;
	}

	return FUNC_FAIL;
}

int OpenLoopAging(int loop_cnt, int ms_delay)
{
	int msg = 0;
	log_tprintf(1, _T("[OpenLoopAging] %d times, %dms delay  setting"), loop_cnt, ms_delay);
	
	dw9786_reset();
	dw9786_3_axis_servo_off();
	RamWriteA(0xB100, 0x0000); // 0
	RamWriteA(0xB200, 0x0000); // 0
	m_delay(10);

	log_tprintf(1, _T("[OpenLoopAging] x-axis open loop aging start"));
	for (int cnt = 0; cnt < loop_cnt; cnt++)
	{
		log_tprintf(1, _T("[OpenLoopAging] loop count : %d"), cnt);
		RamWriteA(0xB100, 0x03FF); // +1023
		m_delay(ms_delay);
		RamWriteA(0xB100, 0xFC01); //-1023
		m_delay(ms_delay);
	}
	RamWriteA(0xB100, 0x0000); // 0
	RamWriteA(0xB200, 0x0000); // 0
	m_delay(10);
	log_tprintf(1, _T("[OpenLoopAging] x-axis open loop aging finished"));

	log_tprintf(1, _T("[OpenLoopAging] y-axis open loop aging start"));
	for (int cnt = 0; cnt < loop_cnt; cnt++)
	{
		log_tprintf(1, _T("[OpenLoopAging] loop count : %d"), cnt);
		RamWriteA(0xB200, 0x03FF); // +1023
		m_delay(ms_delay);
		RamWriteA(0xB200, 0xFC01); //-1023
		m_delay(ms_delay);
	}
	RamWriteA(0xB100, 0x0000); // 0
	RamWriteA(0xB200, 0x0000); // 0
	log_tprintf(1, _T("[OpenLoopAging] y-axis open loop aging finished"));

	return 0;
}

int dw9786_spi_master_mode(void)
{
	log_tprintf(1, _T("[dw9786_spi_master_mode] set to gyro spi master mode."));
	RamWriteA(0xB040, 0x0000); //spi master mode
	return FUNC_PASS;
}

int dw9786_spi_intercept_mode(void)
{
	log_tprintf(1, _T("[dw9786_spi_intercept_mode] set to gyro spi intercept mode."));
	RamWriteA(0xB040, 0x0001); //spi intercept mode
	return FUNC_PASS;
}

int dw9786_ldt0_osc_recovery(void){

	u16 buf[256] = { 0, };
	u16 dat = 0;
	unsigned char ois_id;
	unsigned char mem_id;

	u16 status = 0;

	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] start...."));

	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(2);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);

	RamWriteA(0xE004, 0x0000); //dsp off
	m_delay(5);

	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ldt1 osc read"));

	// read ldt1 : osc info
	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	RamWriteA(0xED00, 0x0004); //select ldt1
	RamWriteA(0xED28, 0x01E9); //In-direct mode : set read address 

	RamReadA(0xED30, &buf[245]); // 72M osc
	RamReadA(0xED30, &buf[247]); // 64M osc
	RamReadA(0xED30, &buf[249]); // 48M osc
	
	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ldt1 48M_osc : %04X"), buf[249]);
	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ldt1 64M_osc : %04X"), buf[247]);
	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ldt1 72M_osc : %04X"), buf[245]);

	
	// read ldt0 : ois slave id, user mem id
	RamWriteA(0xED00, 0x0002); //select ldt0
	RamWriteA(0xED28, 0x0001); //In-direct mode : set read address 

	RamReadA(0xED30, &buf[1]);
	RamReadA(0xED30, &buf[3]); // ois_id, mem_id

	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ois_id: 0x%02X, mem_id: 0x%02X"), buf[3] >> 8, buf[3] & 0xFF);

	RamWriteA(0xED0C, 0x0001); //start erase
	m_delay(5);

	for (int i = 0; i < 10; i++)
	{
		RamReadA(0xED04, &status); //busy check

		if (status == 0)
			break;
		if (i == 9)
			log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] ldt0 erase fail"));
		m_delay(5);
	}

	/* flash ldt0 write */
	for (int i = 0; i < 256; i += 64) {
		if (i == 0) RamWriteA(0xED28, 0x0000);
		CntWrt(0xED2C, (u16*)(buf + i), 64); // 16bit write
	}

	log_tprintf(1, _T("[dw9786_ldt0_osc_recovery] done!!"));

	return FUNC_PASS;
}

int dw9786_slave_id_change(unsigned char ois_id, unsigned char mem_id)
{
    // update 23.06.22 To solve the issue of osc triming data being deleted
	u16 dat = 0;
	u16 status = 0;
	u16 buf[256] = { 0, };
	
	log_tprintf(1, _T("[dw9786_slave_id_change] start...."));
	
	log_tprintf(1, _T("[dw9786_slave_id_change] ois_id: 0x%02X, mem_id: 0x%02X"), ois_id, mem_id);
	dat = (u16) ois_id<<8 | mem_id;
	
	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(2);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);
	
	RamWriteA(0xE004, 0x0000); //dsp off
	m_delay(5);

	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	RamWriteA(0xED00, 0x0002); //select ldt0
	RamWriteA(0xED28, 0x0001); //In-direct mode : set read address 

	for (int i = 0; i < 4; i++){
		CnRd(0xED2C, (unsigned short*) buf + (64*i), 64);
	}

	buf[1] = 0x0300; //enable the function to allow the user to change the slave_id
	buf[3] = dat; //MSB: ois_slave_id, LSB: memory_slave_id

	RamWriteA(0xED0C, 0x0001); //start erase
	m_delay(5);

	for (int i = 0; i < 10; i++)
	{
		RamReadA(0xED04, &status); //busy check

		if (status == 0)
			break;
		if (i == 9)
			log_tprintf(1, _T("[dw9786_slave_id_change] ldt0 erase fail"));
		m_delay(5);
	}
		
	/* flash ldt0 write */
	for (int i = 0; i < 256; i += 64) {
		if (i == 0) RamWriteA(0xED28, 0x0000);
		CntWrt(0xED2C, (u16*)(buf + i), 64); // 16bit write
	}
	
	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(2);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);
	log_tprintf(1, _T("[dw9786_slave_id_change] slave_id change completed"));
	
	return FUNC_PASS;
}


int dw9786_osc_red_mem_change(int OSC)
{
	// OSC: 1 (48MHz)
	// OSC: 2 (64MHz)
	// OSC: 3 (72MHz)
	u16 dat = 0;
	u16 status = 0;
	u16 OSC_48M_Trim = 0;
	u16 OSC_64M_Trim = 0;
	u16 OSC_72M_Trim = 0;
	log_tprintf(1, _T("[dw9786_osc_red_mem_change] start...."));
	
	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(2);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);
	
	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	RamWriteA(0xED00, 0x0002); //select ldt0

	//Read OSC register with address to change OSC frequency in Flash Analog Trimming(LDT0) area 
	RamWriteA(0xED28, 0x01F1);
	RamReadA(0xED30, &OSC_48M_Trim);

	RamWriteA(0xED28, 0x01ED);
	RamReadA(0xED30, &OSC_64M_Trim);

	RamWriteA(0xED28, 0x01E9);
	RamReadA(0xED30, &OSC_72M_Trim);

	//F. In the Flash Analog Trimming (RED) area, write a value to the address to change the OSC frequency.
	RamWriteA(0xED00, 0x0008); //select RED
	RamWriteA(0xED34, 0x0002); //EN_PG_BUF enable
	RamWriteA(0xED28, 0x0098); //mem_address
	if(OSC == OSC_48M)
	{
		RamWriteA(0xED30, OSC_48M_Trim); //mem_data LSB
		log_tprintf(1, _T("[dw9786_osc_red_mem_change] 48MHz OSC is selected. Trim_code: 0x%04X"),OSC_48M_Trim);
	}
	else if(OSC == OSC_64M)
	{
		RamWriteA(0xED30, OSC_64M_Trim); //mem_data LSB
		log_tprintf(1, _T("[dw9786_osc_red_mem_change] 64MHz OSC is selected. Trim_code: 0x%04X"),OSC_64M_Trim);
	}
	else if(OSC == OSC_72M)
	{
		RamWriteA(0xED30, OSC_72M_Trim); //mem_data LSB
		log_tprintf(1, _T("[dw9786_osc_red_mem_change] 72MHz OSC is selected. Trim_code: 0x%04X"),OSC_72M_Trim);
	}
	else
	{
		log_tprintf(1, _T("[dw9786_osc_change] NG! Choose the correct OSC value."));
	}

	RamWriteA(0xED34, 0x0000); //EN_PG_BUF disable
	m_delay(10); //it is absolutely necessary
	RamWriteA(0xED00, 0x0000); //select fw
	RamWriteA(0xE2FC, 0x0000); //all protection on

	log_tprintf(1, _T("[dw9786_osc_red_mem_change] OSC changes have been completed"));
	
	return FUNC_PASS;
}

u16 gOSC_48M_Trim = 0;
u16 gOSC_64M_Trim = 0;
u16 gOSC_72M_Trim = 0;

int dw9786_osc_param_change(int OSC)
{
	// OSC: 1 (48MHz)
	// OSC: 2 (64MHz)
	// OSC: 3 (72MHz)
	u16 dat = 0;
	u16 status = 0;
	log_tprintf(1, _T("[dw9786_osc_param_change] start...."));
	
	dw9786_servo_off();

	RamWriteA(0xE2FC, 0xAC1E); //all protection off

	if(OSC == OSC_48M)
	{
		RamWriteA(0xE298, gOSC_48M_Trim); //48MHz
		log_tprintf(1, _T("[dw9786_osc_param_change] 48MHz OSC is selected. Trim_code: 0x%04X"),gOSC_48M_Trim);
	}
	if(OSC == OSC_64M)
	{
		RamWriteA(0xE298, gOSC_64M_Trim); //64MHz
		log_tprintf(1, _T("[dw9786_osc_param_change] 64MHz OSC is selected. Trim_code: 0x%04X"),gOSC_64M_Trim);
	}
	if(OSC == OSC_72M)
	{
		RamWriteA(0xE298, gOSC_72M_Trim); //72MHz
		log_tprintf(1, _T("[dw9786_osc_param_change] 72MHz OSC is selected. Trim_code: 0x%04X"),gOSC_72M_Trim);
	}

	RamWriteA(0xE2FC, 0x0000); //all protection on

	dw9786_servo_on();

	log_tprintf(1, _T("[dw9786_osc_param_change] change osc_param complete"));
	
	return FUNC_PASS;
}

int dw9786_read_osc_param(void)
{
	// OSC: 1 (48MHz)
	// OSC: 2 (64MHz)
	// OSC: 3 (72MHz)
	u16 dat = 0;
	u16 status = 0;
	log_tprintf(1, _T("[dw9786_read_osc_param] start...."));
	
	RamWriteA(0xE000, 0x0000); //shutdown mode
	m_delay(2);
	RamWriteA(0xE000, 0x0001); //standby mode
	m_delay(5);
	
	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	RamWriteA(0xED00, 0x0002); //select ldt0

	//Read OSC register with address to change OSC frequency in Flash Analog Trimming(LDT0) area 
	
	RamWriteA(0xED28, 0x01F1);
	RamReadA(0xED30, &gOSC_48M_Trim); //read 48MHz OSC trimming data
	log_tprintf(1, _T("[dw9786_read_osc_param] OSC_48MHz Trim %04X"), gOSC_48M_Trim);

	RamWriteA(0xED28, 0x01ED);
	RamReadA(0xED30, &gOSC_64M_Trim); //read 64MHz OSC trimming data
	log_tprintf(1, _T("[dw9786_read_osc_param] OSC_64MHz Trim %04X"), gOSC_64M_Trim);

	RamWriteA(0xED28, 0x01E9);
	RamReadA(0xED30, &gOSC_72M_Trim); //read 72MHz OSC trimming data
	log_tprintf(1, _T("[dw9786_read_osc_param] OSC_72MHz Trim %04X"), gOSC_72M_Trim);

	RamWriteA(0xED00, 0x0000); //select fw
	m_delay(1);
	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	RamWriteA(0xE004, 0x0001); //idle mode MCU active
	m_delay(100);
	log_tprintf(1, _T("[dw9786_read_osc_param] OSC_param read complete"));

	return FUNC_PASS;
}

int dw9786_osc_param_change_80MHz(void)
{
	u16 dat = 0;
	u16 status = 0;
	u16 OSC_80MHZ = 0;
	log_tprintf(1, _T("[dw9786_osc_param_change_80MHz] start...."));
	
	dw9786_servo_off();

	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	
	OSC_80MHZ = make_80MHZ_osc(gOSC_72M_Trim);

	RamWriteA(0xE298, OSC_80MHZ); //72MHz
	m_delay(5);

	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	dw9786_servo_on();

	log_tprintf(1, _T("[dw9786_osc_param_change_80MHz] change osc_param complete"));
	
	return FUNC_PASS;
}

unsigned short make_80MHZ_osc(u16 osc_72m)
{
	char before_osc = 0;
	char after_osc = 0;
	char up_80m_osc = 17; // Based on 72MHz, 17code is increased.
	u16 osc_80m = 0;
	
	before_osc = (char)(osc_72m >> 8) & 0xFF; // 72MHz OSC_MSB
	
	if(before_osc > 63)
		before_osc -= 128;
	
	after_osc = before_osc + up_80m_osc;
		
	if(after_osc < 0)
		after_osc += 128;
	
	osc_80m = (u16)(after_osc << 8) & 0xFF00;
	
	log_tprintf(1, _T("[osc_freq_up] osc_72MHz : %04X, osc_80MHz : %04X,"), osc_72m, osc_80m);
	
	return osc_80m;	
}

int dw9786_osc_param_change_78MHz(void)
{
	u16 dat = 0;
	u16 status = 0;
	u16 OSC_78MHZ = 0;
	log_tprintf(1, _T("[dw9786_osc_param_change_78MHz] start...."));
	
	dw9786_servo_off();

	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	
	OSC_78MHZ = make_78MHZ_osc(gOSC_72M_Trim);

	RamWriteA(0xE298, OSC_78MHZ); //72MHz
	m_delay(5);

	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	dw9786_servo_on();

	log_tprintf(1, _T("[dw9786_osc_param_change] change osc_param complete"));
	
	return FUNC_PASS;
}

unsigned short make_78MHZ_osc(u16 osc_72m)
{
	char before_osc = 0;
	char after_osc = 0;
	char up_78m_osc = 12; // Based on 72MHz, 12code is increased.
	u16 osc_78m = 0;
	
	before_osc = (char)(osc_72m >> 8) & 0xFF; // 72MHz OSC_MSB
	
	if(before_osc > 63)
		before_osc -= 128;
	
	after_osc = before_osc + up_78m_osc;
		
	if(after_osc < 0)
		after_osc += 128;
	
	osc_78m = (u16)(after_osc << 8) & 0xFF00;
	
	log_tprintf(1, _T("[osc_freq_up] osc_72MHz : %04X, osc_78MHz : %04X,"), osc_72m, osc_78m);
	
	return osc_78m;	
}

int dw9786_osc_param_change_75MHz(void)
{
	u16 dat = 0;
	u16 status = 0;
	u16 OSC_75MHZ = 0;
	log_tprintf(1, _T("[dw9786_osc_param_change_75MHz] start...."));
	
	dw9786_servo_off();

	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	
	OSC_75MHZ = make_75MHZ_osc(gOSC_72M_Trim);

	RamWriteA(0xE298, OSC_75MHZ); //75MHz
	m_delay(5);

	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	dw9786_servo_on();

	log_tprintf(1, _T("[dw9786_osc_param_change_75MHz] change osc_param complete"));
	
	return FUNC_PASS;
}

unsigned short make_75MHZ_osc(u16 osc_72m)
{
	char before_osc = 0;
	char after_osc = 0;
	char up_75m_osc = 7; // Based on 72MHz, 7code is increased.
	u16 osc_75m = 0;
	
	before_osc = (char)(osc_72m >> 8) & 0xFF; // 72MHz OSC_MSB
	
	if(before_osc > 63)
		before_osc -= 128;
	
	after_osc = before_osc + up_75m_osc;
		
	if(after_osc < 0)
		after_osc += 128;
	
	osc_75m = (u16)(after_osc << 8) & 0xFF00;
	
	log_tprintf(1, _T("[osc_freq_up] osc_72MHz : %04X, osc_75MHz : %04X,"), osc_72m, osc_75m);
	
	return osc_75m;	
}

int dw9786_ldo_change(int dec_cnt)
{
	// Step down the dw9786's internal ldo voltage with the input code.
	u16 org_ldo = 0;
	u16 new_ldo = 0;
	short tmp = 0;

	RamWriteA(0xE004, 0x0000); // MCU disable
	m_delay(10);
		
	RamWriteA(0xE2FC, 0xAC1E); // all protection off
	m_delay(1);
	RamReadA(0xE204, &org_ldo); // read ldo code
	log_tprintf(1, _T("[dw9786_ldo_change] org_ldo code: %04X,"), org_ldo);
	
	if ((org_ldo & 0x000F) >= 8) tmp = ((org_ldo | 0xFFF0) + dec_cnt);
	else tmp = org_ldo + dec_cnt;
	
	if( dec_cnt < 0) { // decrese ldo code
		if (tmp <= -8){
			tmp = 8;
			log_tprintf(1, _T("[dw9786_ldo_change] minimum code spec 0xXXX8 : %04X,"), tmp);
		}
	}
	else { // increase ldo code
		if (tmp > 7)
		{
			tmp = 7;
			log_tprintf(1, _T("[dw9786_ldo_change] maximum code spec 0xXXX7 : %04X,"), tmp);
		}
	}
	
	new_ldo = (org_ldo & 0xFFF0) + (tmp & 0x000F);
	log_tprintf(1, _T("[dw9786_ldo_change] origin ldo : %04X, new ldo : %04X,"), org_ldo, new_ldo);

	// write new ldo value
	RamWriteA(0xE204, new_ldo);
	m_delay(50);

	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);

	RamWriteA(0xE004, 0x0001); //idle mode MCU active
	m_delay(100);
	dw9786_servo_on();
	
	return 0;
}

int debug_cnt_check(void)
{
	u16 debug_cnt[10];
	memset(debug_cnt, 0, sizeof(unsigned short) *10);
	
	for(int i = 1; i < 10; i++)
	{
		RamReadA(0xE188, &debug_cnt[i]);
		m_delay(10);
		if( debug_cnt[i-1] == debug_cnt[i]) 
		{
			log_tprintf(1, _T("[debug_cnt_check]%d times fail, debug_count: %d"), i, (short)debug_cnt[i]);
			return FUNC_FAIL;
		}
		log_tprintf(1, _T("[debug_cnt_check]%d times pass, debug_count: %d"), i, (short)debug_cnt[i]);
	}
	return FUNC_PASS;
}

int filter_out_check(void)
{
	u16 filter_out;
	u16 ref_filter[8] = {0x00E7, 0x00F2, 0x00FD, 0x0108, 0x0113, 0x011D, 0x0128, 0x0133};

	m_delay(30); // 30ms delay is required after ois_on.
	for(int i = 0; i < 8; i++)
	{
		RamReadA(0xB150+(i*2), &filter_out);
		m_delay(1);
		if( ref_filter[i] != filter_out) 
		{
			log_tprintf(1, _T("[filter_out_check] fail - ref_filter[%d]: 0x%04X, filter_out: 0x%04X"), i, ref_filter[i], filter_out);
			return FUNC_FAIL;
		}
		log_tprintf(1, _T("[filter_out_check] pass - ref_filter[%d]: 0x%04X, filter_out: 0x%04X"), i, ref_filter[i], filter_out);

	}

	log_tprintf(1, _T("[filter_out_check] verification of filter output completed."));
	return FUNC_PASS;
}

int dw9786_read_week_info(void)
{
	/*
	[dw9786_read_week_info] start....
	[dw9786_read_week_info] IC production week data = 0x1521, Year: 21, week: 33
	[dw9786_read_week_info] read dw9786_ic_week complete
	*/
	u16 week = 0;
	log_tprintf(1, _T("[dw9786_read_week_info] start...."));
	
	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	m_delay(1);
	RamWriteA(0xED00, 0x0008); //select red
	m_delay(1);
	RamWriteA(0xED28, 0x00A6+1);
	m_delay(1);

	RamReadA(0xED30, &week);
	log_tprintf(1, _T("[dw9786_read_week_info] IC production week data = 0x%04X, Year: %d, week: %d"), week, (week>>8), (week & 0xff));

	RamWriteA(0xED00, 0x0000); //select fw
	m_delay(1);
	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	
	log_tprintf(1, _T("[dw9786_read_week_info] read dw9786_ic_week complete"));
	
	return FUNC_PASS;
}

int dw9786_check_ic_year(void)
{
	u16 week = 0;
	log_tprintf(1, _T("[dw9786_check_ic_year] start...."));
	
	RamWriteA(0xE2FC, 0xAC1E); //all protection off
	m_delay(1);
	RamWriteA(0xED00, 0x0008); //select red
	m_delay(1);
	RamWriteA(0xED28, 0x00A6+1);
	m_delay(1);

	RamReadA(0xED30, &week);
	log_tprintf(1, _T("[dw9786_check_ic_year] IC production week data = 0x%04X, Year: %d, week: %d"), week, (week>>8), (week & 0xff));

	RamWriteA(0xED00, 0x0000); //select fw
	m_delay(1);
	RamWriteA(0xE2FC, 0x0000); //all protection on
	m_delay(1);
	
	if((short)(week>>8) < 22)
	{
		log_tprintf(1, _T("[dw9786_check_ic_year] dw9786 IC produced in 20%d"), (week>>8));
		log_tprintf(1, _T("[dw9786_check_ic_year] stop dw9786IC sorting"));
		return NG;
	}

	log_tprintf(1, _T("[dw9786_check_ic_year] read dw9786_ic_week complete"));
	
	return FUNC_PASS;
}

unsigned int checksum_func(u16 *data, int leng)
{
	unsigned int csum = 0xFFFFFFFF;

	for (int i = 0; i < leng; i+= 2)
	{
		//csum += (unsigned int)(*(data + i) << 24) + (unsigned int)(*(data + i + 1) << 16) + (unsigned int)(*(data + i + 2) << 8) + (unsigned int)(*(data + i + 3));
		csum += (unsigned int)(*(data + i) << 16) + (unsigned int)(*(data + i + 1) << 0);
	}
	return ~csum;
}

unsigned int cal_checksum2_func(u32 checksum, u16 gyro_gain_x, u16 gyro_gain_y)
{
	unsigned int csum = ~checksum;

		//csum += (unsigned int)(*(data + i) << 24) + (unsigned int)(*(data + i + 1) << 16) + (unsigned int)(*(data + i + 2) << 8) + (unsigned int)(*(data + i + 3));
	csum -=  gyro_gain_x;
	csum -=  gyro_gain_y;

	return ~csum;
}

int dw9786_cal_checksum(void)
{
	// This is the process of saving the cal_checksum value after gyro gain calibration at the module company.
	// After this function, the store function should be executed.
	int checksum_1 = 0;
	int checksum_2 = 0;
	unsigned short gyro_flag, dat_H, dat_L = 0;
	unsigned short main_gyro_gain_x, main_gyro_gain_y, sub_gyro_gain_x, sub_gyro_gain_y = 0;
	
	RamReadA(0xB7F6, &gyro_flag); //gyro flag
	RamReadA(0xB4E2, &main_gyro_gain_x); //read gyro gain x
	RamReadA(0xB5E2, &main_gyro_gain_y); //read gyro gain y
	RamReadA(0xB4F6, &sub_gyro_gain_x); //read gyro gain x
	RamReadA(0xB5F6, &sub_gyro_gain_y); //read gyro gain y
	
	if(gyro_flag == 0 && main_gyro_gain_x != 0 && main_gyro_gain_y != 0)
	{
		RamReadA(0xB00C, &dat_H); //cal_checksum_H
		RamReadA(0xB00E, &dat_L); //cal_checksum_L
		checksum_1 = (dat_H << 16) + dat_L;
		m_delay(1);
		RamWriteA(0xB7F8, dat_H); //write cal_checksum-1_H
		RamWriteA(0xB7FA, dat_L); //write cal_checksum-1_L

		checksum_2 = ~((~checksum_1) - main_gyro_gain_x - main_gyro_gain_y - sub_gyro_gain_x - sub_gyro_gain_y);
		
		RamWriteA(0xB7FC, u16(checksum_2 >> 16)); //write cal_checksum-2_H
		RamWriteA(0xB7FE, u16(checksum_2 & 0xFFFF)); //write cal_checksum-2_L
		
		log_tprintf(1, _T("[dw9786_cal_checksum] cal_checksum-1 : 0x%08X)\r\n"), checksum_1);
		log_tprintf(1, _T("[dw9786_cal_checksum] cal_checksum-2 : 0x%08X)\r\n"), checksum_2);
		
		log_tprintf(1, _T("[dw9786_cal_checksum] cal_checksum pass\r\n"));
	}else
	{	
		if(gyro_flag)
			log_tprintf(1, _T("[dw9786_cal_checksum] the gyro_flag value must be 0 - NG (gyro flag : 0x%04X)\r\n"), gyro_flag);
		if(main_gyro_gain_x == 0)
			log_tprintf(1, _T("[dw9786_cal_checksum] x-axis main_gyro gain NG. (x gyro gain : 0x%04X)\r\n"), main_gyro_gain_x);
		if(main_gyro_gain_y == 0)
			log_tprintf(1, _T("[dw9786_cal_checksum] y-axis main_gyro gain NG. (y gyro gain : 0x%04X)\r\n"), main_gyro_gain_y);
		
		log_tprintf(1, _T("[dw9786_cal_checksum] dw9786_cal_checksum NG\r\n"));
		return FUNC_FAIL;
	}
	return FUNC_PASS;
}


void dw978x_ois_slvid(BYTE dw978x_slvid_8BIT)
{
	log_tprintf(1, _T("[dw978x_ois_slvid] before DW9786_SLVID is 0x%2X"), _SLV_OIS_);
	_SLV_OIS_ = dw978x_slvid_8BIT;
	log_tprintf(1, _T("[dw978x_ois_slvid] after DW9786_SLVID to 0x%2X"), _SLV_OIS_);
}

void dw978x_mem_slvid(BYTE dw978x_slvid_8BIT)
{
	log_tprintf(1, _T("[dw978x_mem_slvid] before DW9786_MEMID is 0x%2X"), MEM_SLAVE_ID);
	MEM_SLAVE_ID = dw978x_slvid_8BIT;
	log_tprintf(1, _T("[dw978x_mem_slvid] after DW9786_MEMID to 0x%2X"), MEM_SLAVE_ID);
}

void set_af_ic_setting(BYTE AF_SLVID_8BIT, int ctrl_bit)
{
	log_tprintf(1, _T("original AF_SLVID is 0x%2X\r\n"), _SLV_AF_);
	_SLV_AF_ = AF_SLVID_8BIT;
	af_control_bit = ctrl_bit;
	log_tprintf(1, _T("change AF_SLVID to 0x%2X\r\n"), _SLV_AF_);
	log_tprintf(1, _T("AF control bit: %dbit\r\n"), af_control_bit);
}


double get_pixel_semco_4point(double left, double right, double top, double bottom)
{
	double center = 0;
	int point =  4;

	if(left < PIXEL_MIN)
	{
		point--;
		left = 0;
	}
	
	if(right < PIXEL_MIN)
	{
		point--;
		right = 0;
	}

	if(top < PIXEL_MIN)
	{
		point--;
		top = 0;
	}

	if(bottom < PIXEL_MIN)
	{
		point--;
		bottom = 0;
	}
	center = (left + right  + top + bottom) / point;

	return center;
}

void dw9786_getpixel_coordinate(stPixelCoordinate *p_coor, short p_num)
{
	u16 r_dat = 0;
	int i;
	log_tprintf(1, _T("[dw9786_getpixel_coordinate] start "));

	Lin[X_AXIS].PointNumber = p_num; //x,y point number

	RamReadA(0xB710, &r_dat);
	log_tprintf(1, _T("[GetPixel_Corrdinate] Linearity enable status : 0x%04x "), r_dat);
	RamReadA(0xB712, &r_dat);
	log_tprintf(1, _T("[GetPixel_Corrdinate] Crosstalk enable status : 0x%04x "), r_dat);

	for( i = 0; i < p_num; i++) {
		// Y move
		Lin[X_AXIS].Coordinate[X_AXIS][i] = p_coor->XonXmove[i];
		Lin[X_AXIS].Coordinate[Y_AXIS][i] = p_coor->YonXmove[i];
		// Y move
		Lin[Y_AXIS].Coordinate[X_AXIS][i] = p_coor->XonYmove[i];
		Lin[Y_AXIS].Coordinate[Y_AXIS][i] = p_coor->YonYmove[i];
	}
	for( i = 0; i < p_num; i++) {
		log_tprintf(1, _T("[GetPixel_Corrdinate] XonXmove[%d] = %6.2f\t YonXmove[%d] = %6.2f"),i, p_coor->XonXmove[i],i, p_coor->YonXmove[i]);
	}
	for( i = 0; i < p_num; i++) {
		log_tprintf(1, _T("[GetPixel_Corrdinate] XonYmove[%d] = %6.2f\t  YonYmove[%d] = %6.2f"),i, p_coor->XonYmove[i],i, p_coor->YonYmove[i]);
	}
	log_tprintf(1, _T("[dw9786_getpixel_coordinate] finished "));
}


void dw9786_af_write_linearity_compensation(int *LinCoeff) 
{
	int i=0;
	unsigned short start_addr;
	unsigned short point_addr;
	log_tprintf(1, _T("[dw9786_af_write_linearity_compensation] start...."));
	
	if(nTargetcnt == 21)
	{
		point_addr = 0xB6BE;
		start_addr = 0xB61A;
	}
	else if(nTargetcnt > 21)
	{
		// From 22 points or higher, the firmware must be changed to use the AF drift address.
		point_addr = 0xB730;
		start_addr = 0xB734;
	}
	
	RamWriteA( point_addr, (unsigned short) (nTargetcnt) );
	log_tprintf(1, _T("[dw9786_af_write_linearity_compensation] Addr:0x%04X -- Linearity point:%d\r\n"), (unsigned short)point_addr, (unsigned short)(nTargetcnt));

	for(i=0; i<(nTargetcnt -1); i++)
	{
		RamWriteA( start_addr + (i*2), (unsigned short)LinCoeff[i]);
		log_tprintf(1, _T("[dw9786_af_write_linearity_compensation] Addr:0x%04X -- Coeff:%04X\r\n"), start_addr + (i*2), (unsigned short)LinCoeff[i]);
	}
	log_tprintf(1, _T("[dw9786_af_write_linearity_compensation] finished "));
}

void dw9786_write_linearity_compensation(void) 
{
	int i=0;
	int num = Lin[X_AXIS].PointNumber;
	log_tprintf(1, _T("[dw9786_write_linearity_compensation] start "));

	for(i=0; i<num; i++)
	{
		RamWriteA( 0xB41A + (i*2), Lin[X_AXIS].Coeffi[i]); 
	}

	for(i=0; i<num-1; i++)
	{
		RamWriteA( 0xB444 + (i*2), Lin[X_AXIS].SlopeFixed[i]); 
	}

	for(i=0; i<num; i++)
	{
		RamWriteA( 0xB51A + (i*2), Lin[Y_AXIS].Coeffi[i]); 
	}

	for(i=0; i<num-1; i++)
	{
		RamWriteA( 0xB544 + (i*2), Lin[Y_AXIS].SlopeFixed[i]); 
	}
	log_tprintf(1, _T("[dw9786_write_linearity_compensation] finished "));
}

void dw9786_write_crosstalk_compensation(void)
{
	int i=0;
	int num = Lin[X_AXIS].PointNumber;
	log_tprintf(1, _T("[write_crosstalk_comp] start"));
	
	for(i=0; i<num; i++)
	{
		RamWriteA( 0xB46C + (i*2), Cross[X_AXIS].Coeffi[i]);
	}
	for(i=0; i<num-1; i++)
	{
		RamWriteA( 0xB496 + (i*2), Cross[X_AXIS].slope[i]);
	}
	for(i=0; i<num; i++)
	{
		RamWriteA( 0xB56C + (i*2), Cross[Y_AXIS].Coeffi[i]);
	}
	for(i=0; i<num-1; i++)
	{
		RamWriteA( 0xB596 + (i*2), Cross[Y_AXIS].slope[i]);
	}
	log_tprintf(1, _T("[write_crosstalk_comp] finished "));
}

void dw9786_crosstalk_compensation(void)
{
	int i = 0;
	short polarity[2];
	//short rem_polarity[2];
	short point_number;
	log_tprintf(1, _T("[dw9786_crosstalk_compensation] start "));

	point_number = Lin[X_AXIS].PointNumber;


	if (Lin[X_AXIS].Coordinate[X_AXIS][0] > Lin[X_AXIS].Coordinate[X_AXIS][1])
	{
		polarity[X_AXIS] = 1;
	}
	else
	{
		polarity[X_AXIS] = -1;
	}

	if (Lin[Y_AXIS].Coordinate[Y_AXIS][0] > Lin[Y_AXIS].Coordinate[Y_AXIS][1])
	{
		polarity[Y_AXIS] = 1;
	}
	else
	{
		polarity[Y_AXIS] = -1;
	}

	for (i = 0; i < point_number; i++)
	{
		Cross[X_AXIS].MovePixel[i] = Lin[X_AXIS].Coordinate[Y_AXIS][i];
		Cross[Y_AXIS].MovePixel[i] = Lin[Y_AXIS].Coordinate[X_AXIS][i];

		log_tprintf(1, _T("[crosstalk_compensation] Cross[X_AXIS].MovePixel[%d] %04f \tCross[Y_AXIS].MovePixel[%d] %04f "), i, Cross[0].MovePixel[i], i, Cross[1].MovePixel[i]);
	}

	log_tprintf(1, _T(""));

	for (i = 0; i < point_number; i++)
	{
		Cross[X_AXIS].zeroPixel[i] = Cross[X_AXIS].MovePixel[i] - Cross[X_AXIS].MovePixel[(point_number-1)/2];
		Cross[Y_AXIS].zeroPixel[i] = Cross[Y_AXIS].MovePixel[i] - Cross[Y_AXIS].MovePixel[(point_number-1)/2];
		Cross[X_AXIS].Coeffi[i] = (short) round (Cross[X_AXIS].zeroPixel[i] / ((Lin[Y_AXIS].MeasureDist[(point_number-1)/2 -1] + Lin[Y_AXIS].MeasureDist[(point_number-1)/2])/(Lin[Y_AXIS].TargetStep*2)))*polarity[Y_AXIS];
		Cross[Y_AXIS].Coeffi[i] = (short) round (Cross[Y_AXIS].zeroPixel[i] / ((Lin[X_AXIS].MeasureDist[(point_number-1)/2 -1] + Lin[X_AXIS].MeasureDist[(point_number-1)/2])/(Lin[X_AXIS].TargetStep*2)))*polarity[X_AXIS];

		log_tprintf(1, _T("[crosstalk_compensation] Cross[X_AXIS].Coeffi[%d] %04d \t Cross[Y_AXIS].Coeffi[%d] %04d "), i, Cross[X_AXIS].Coeffi[i], i, Cross[Y_AXIS].Coeffi[i]);
	}
	for (i = 0; i < point_number-1; i++)
	{
		Cross[X_AXIS].slope[i] = (short) round (((Cross[X_AXIS].Coeffi[i+1] - Cross[X_AXIS].Coeffi[i]) / Lin[X_AXIS].TargetStep) * 32767) ;
		Cross[Y_AXIS].slope[i] = (short) round (((Cross[Y_AXIS].Coeffi[i+1] - Cross[Y_AXIS].Coeffi[i]) / Lin[Y_AXIS].TargetStep) * 32767) ;

		log_tprintf(1, _T("[crosstalk_compensation] Cross[X_AXIS].slope[%d] %04d \t Cross[Y_AXIS].slope[%d] %04d "), i, Cross[0].slope[i], i, Cross[1].slope[i]);
	}

	dw9786_write_crosstalk_compensation();
	log_tprintf(1, _T("[dw9786_crosstalk_compensation] finished "));
}

void dw9786_linearity_compensation(u16 xtarget_step, u16 ytarget_step)
{
	log_tprintf(1, _T("[linearity_compensation] XtargetStep = %d -- YtargetStep = %d "), xtarget_step, ytarget_step);

	int i = 0, k;
	int j = 0;
	short point_number;
	short half_num;
	log_tprintf(1, _T("[dw9786_linearity_compensation] start "));

	point_number = Lin[X_AXIS].PointNumber;
	half_num = (point_number -1) / 2;

	Lin[X_AXIS].TargetStep = xtarget_step;
	Lin[Y_AXIS].TargetStep = ytarget_step;


	for (i = 0; i < point_number; i++)	// n point
	{
		Lin[X_AXIS].TargetCmd[i] = Lin[X_AXIS].TargetStep * (i-half_num);		// target position X	-1536, -1024, -512, 0, 512, 1024, 1536	������.
		Lin[Y_AXIS].TargetCmd[i] = Lin[Y_AXIS].TargetStep * (i-half_num);		// target position Y	-1200, -800, -400, 0, 400, 800, 1200	������.

		Lin[X_AXIS].zeroDist[X_AXIS][i] = Lin[X_AXIS].Coordinate[X_AXIS][i] - Lin[X_AXIS].Coordinate[X_AXIS][0];	// X������ �̵��� X pixel �̵���
		Lin[X_AXIS].zeroDist[Y_AXIS][i] = Lin[X_AXIS].Coordinate[Y_AXIS][i] - Lin[X_AXIS].Coordinate[Y_AXIS][0];	// X������ �̵��� Y pixel �̵���

		Lin[Y_AXIS].zeroDist[X_AXIS][i] = Lin[Y_AXIS].Coordinate[X_AXIS][i] - Lin[Y_AXIS].Coordinate[X_AXIS][0];	// Y������ �̵��� X pixel �̵���
		Lin[Y_AXIS].zeroDist[Y_AXIS][i] = Lin[Y_AXIS].Coordinate[Y_AXIS][i] - Lin[Y_AXIS].Coordinate[Y_AXIS][0];	// Y������ �̵��� Y pixel �̵���
	}

	log_tprintf(1, _T(""));

	for (i = 1; i < point_number; i++)
	{
		Lin[X_AXIS].MeasureDist[i - 1] = abs(Lin[X_AXIS].zeroDist[X_AXIS][i] - Lin[X_AXIS].zeroDist[X_AXIS][i - 1]);
		Lin[Y_AXIS].MeasureDist[i - 1] = abs(Lin[Y_AXIS].zeroDist[Y_AXIS][i] - Lin[Y_AXIS].zeroDist[Y_AXIS][i - 1]);
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].MeasureDist[%d] %04f \t Lin[Y_AXIS].MeasureDist[%d] %04f "), i-1, Lin[X_AXIS].MeasureDist[i-1], i-1, Lin[Y_AXIS].MeasureDist[i-1]);
	}

	Lin[X_AXIS].NegStroke = 0;
	Lin[Y_AXIS].NegStroke = 0;
	for (i = 0; i < half_num; i++)
	{
		Lin[X_AXIS].NegStroke += Lin[X_AXIS].MeasureDist[i];
		Lin[Y_AXIS].NegStroke += Lin[Y_AXIS].MeasureDist[i];
	};

	Lin[X_AXIS].PosStroke = 0;
	Lin[Y_AXIS].PosStroke = 0;
	for (i = half_num; i < point_number-1; i++)
	{
		Lin[X_AXIS].PosStroke += Lin[X_AXIS].MeasureDist[i];
		Lin[Y_AXIS].PosStroke += Lin[Y_AXIS].MeasureDist[i];
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].NegStroke %04f "), Lin[X_AXIS].NegStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].PosStroke %04f "), Lin[X_AXIS].PosStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].NegStroke %04f "), Lin[Y_AXIS].NegStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].PosStroke %04f "), Lin[Y_AXIS].PosStroke);

	Lin[X_AXIS].TargetStroke = (Lin[X_AXIS].PosStroke + Lin[X_AXIS].NegStroke) / 2;
	Lin[Y_AXIS].TargetStroke = (Lin[Y_AXIS].PosStroke + Lin[Y_AXIS].NegStroke) / 2;
		
	log_tprintf(1, _T(""));
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].TargetStroke %04f "), Lin[X_AXIS].TargetStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].TargetStroke %04f "), Lin[Y_AXIS].TargetStroke);

	Lin[X_AXIS].InterStroke = Lin[X_AXIS].TargetStroke / half_num;
	Lin[Y_AXIS].InterStroke = Lin[Y_AXIS].TargetStroke / half_num;

	log_tprintf(1, _T(""));

	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].InterStroke %04f "), Lin[X_AXIS].InterStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].InterStroke %04f "), Lin[Y_AXIS].InterStroke);

	log_tprintf(1, _T(""));

	for (i = 0; i < point_number; i++)
	{
		Lin[X_AXIS].IdealStroke[i] = Lin[X_AXIS].InterStroke * (i-half_num);
		Lin[Y_AXIS].IdealStroke[i] = Lin[Y_AXIS].InterStroke * (i-half_num);

		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].IdealStroke %04f,\t\t Lin[Y_AXIS].IdealStroke %04f "), Lin[X_AXIS].IdealStroke[i], Lin[Y_AXIS].IdealStroke[i] );
	}
	memset(Lin[X_AXIS].MeasureStroke, 0, sizeof(Lin[X_AXIS].MeasureStroke));
	memset(Lin[Y_AXIS].MeasureStroke, 0, sizeof(Lin[Y_AXIS].MeasureStroke));

	for (i = 0; i < half_num; i++)
	{
		for( k = i; k < half_num; k++)
		{
			Lin[X_AXIS].MeasureStroke[i] += Lin[X_AXIS].MeasureDist[k];
			Lin[Y_AXIS].MeasureStroke[i] += Lin[Y_AXIS].MeasureDist[k];
		}
		Lin[X_AXIS].MeasureStroke[i] *= (-1);
		Lin[Y_AXIS].MeasureStroke[i] *= (-1);
	}
	Lin[X_AXIS].MeasureStroke[half_num] = 0;
	Lin[Y_AXIS].MeasureStroke[half_num] = 0;
	
	for (i = half_num + 1; i < point_number; i++)
	{
		for( k = half_num; k < i; k++)
		{
			Lin[X_AXIS].MeasureStroke[i] += Lin[X_AXIS].MeasureDist[k];
			Lin[Y_AXIS].MeasureStroke[i] += Lin[Y_AXIS].MeasureDist[k];
		}
	}

	Lin[X_AXIS].StrokePerCmd = (Lin[X_AXIS].NegStroke + Lin[X_AXIS].PosStroke) / (Lin[X_AXIS].TargetStep * half_num * 2);
	Lin[Y_AXIS].StrokePerCmd = (Lin[Y_AXIS].NegStroke + Lin[Y_AXIS].PosStroke) / (Lin[Y_AXIS].TargetStep * half_num * 2);

	log_tprintf(1, _T(""));

	for (i = 0; i < point_number; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].MeasureStroke[%d] : %04f \t Lin[Y_AXIS].MeasureStroke[%d] : %04f"),i,Lin[X_AXIS].MeasureStroke[i], i, Lin[Y_AXIS].MeasureStroke[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));
	// Coeffi = (measuerstroke - idealstroke) / ( (measestroke[i-1] - measurestroke[i]) / targetstep )
	for (i = 0; i < point_number; i++)
	{
		if (i == point_number-1) 
		{
			// i == 6
			CoeffiX = (Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].IdealStroke[i]) / ((Lin[X_AXIS].MeasureStroke[i-1] - Lin[X_AXIS].MeasureStroke[i]) / Lin[X_AXIS].TargetStep);
			Lin[X_AXIS].Coeffi[i] = (u16)(Lin[X_AXIS].TargetStep * (i)) + round( CoeffiX );

			CoeffiY = (Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].IdealStroke[i]) / ((Lin[Y_AXIS].MeasureStroke[i-1] - Lin[Y_AXIS].MeasureStroke[i]) / Lin[Y_AXIS].TargetStep);
			Lin[Y_AXIS].Coeffi[i] = (u16)((Lin[Y_AXIS].TargetStep * (i)) + round( CoeffiY ));
		}
		else
		{
			CoeffiX= (Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].IdealStroke[i]) / ((Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].MeasureStroke[i+1]) / Lin[X_AXIS].TargetStep);
			Lin[X_AXIS].Coeffi[i] = (u16)(Lin[X_AXIS].TargetStep * (i)) + round( CoeffiX );

			CoeffiY = (Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].IdealStroke[i]) / ((Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].MeasureStroke[i+1]) / Lin[Y_AXIS].TargetStep);
			Lin[Y_AXIS].Coeffi[i] = (u16)((Lin[Y_AXIS].TargetStep * (i)) + round( CoeffiY ));
		}

		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].Coeffi[%d] : %04d \t\t Lin[Y_AXIS].Coeffi[%d] : %04d"),i,Lin[X_AXIS].Coeffi[i], i,Lin[Y_AXIS].Coeffi[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));

	for (i = 0; i < point_number-1; i++)
	{
		Lin[X_AXIS].Slope[i] = (Lin[X_AXIS].Coeffi[i+1] - Lin[X_AXIS].Coeffi[i]) / Lin[X_AXIS].TargetStep;
		Lin[X_AXIS].SlopeFixed[i] = (u16)round((Lin[X_AXIS].Slope[i] * 1024));

		Lin[Y_AXIS].Slope[i] = (Lin[Y_AXIS].Coeffi[i+1] - Lin[Y_AXIS].Coeffi[i]) / Lin[Y_AXIS].TargetStep;
		Lin[Y_AXIS].SlopeFixed[i] = (u16)round((Lin[Y_AXIS].Slope[i] * 1024));
	}

	for (i = 0; i < point_number-1; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].Slope[%d] : %04f \t\t Lin[Y_AXIS].Slope[%d] : %04f"),i,Lin[X_AXIS].Slope[i], i,Lin[Y_AXIS].Slope[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));
	for (i = 0; i < point_number-1; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].SlopeFixed[%d] : %04d \t\t Lin[Y_AXIS].SlopeFixed[%d] : %04d"),i,Lin[X_AXIS].SlopeFixed[i], i,Lin[Y_AXIS].SlopeFixed[i]);
	}
	/* Included in firmware as PARAM area */
	// Write Step Register
	RamWriteA(0xB4BE, point_number); //x point number
	RamWriteA(0xB4C0, xtarget_step); //x step

	RamWriteA(0xB5BE, point_number); //y point number
	RamWriteA(0xB5C0, ytarget_step); //y step
	
	dw9786_write_linearity_compensation();
	
	log_tprintf(1, _T("[dw9786_linearity_compensation] finished "));
}


void dw9786_fixed_linearity_compensation(unsigned short xtarget_step, unsigned short ytarget_step, unsigned short X_TargetStroke, unsigned short Y_TargetStroke)
{
	log_tprintf(1, _T("[linearity_compensation] XtargetStep = %d -- YtargetStep = %d "), xtarget_step, ytarget_step);

	int i = 0, k;
	int j = 0;
	short point_number;
	short half_num;
	log_tprintf(1, _T("[dw9786_linearity_compensation] start "));
	

	point_number = Lin[X_AXIS].PointNumber;
	half_num = (point_number -1) / 2;

	Lin[X_AXIS].TargetStep = xtarget_step;
	Lin[Y_AXIS].TargetStep = ytarget_step;


	for (i = 0; i < point_number; i++)	// n point
	{
		Lin[X_AXIS].TargetCmd[i] = Lin[X_AXIS].TargetStep * (i-half_num);		// target position X	-1536, -1024, -512, 0, 512, 1024, 1536	������.
		Lin[Y_AXIS].TargetCmd[i] = Lin[Y_AXIS].TargetStep * (i-half_num);		// target position Y	-1200, -800, -400, 0, 400, 800, 1200	������.

		Lin[X_AXIS].zeroDist[X_AXIS][i] = Lin[X_AXIS].Coordinate[X_AXIS][i] - Lin[X_AXIS].Coordinate[X_AXIS][0];	// X������ �̵��� X pixel �̵���
		Lin[X_AXIS].zeroDist[Y_AXIS][i] = Lin[X_AXIS].Coordinate[Y_AXIS][i] - Lin[X_AXIS].Coordinate[Y_AXIS][0];	// X������ �̵��� Y pixel �̵���

		Lin[Y_AXIS].zeroDist[X_AXIS][i] = Lin[Y_AXIS].Coordinate[X_AXIS][i] - Lin[Y_AXIS].Coordinate[X_AXIS][0];	// Y������ �̵��� X pixel �̵���
		Lin[Y_AXIS].zeroDist[Y_AXIS][i] = Lin[Y_AXIS].Coordinate[Y_AXIS][i] - Lin[Y_AXIS].Coordinate[Y_AXIS][0];	// Y������ �̵��� Y pixel �̵���
	}

	log_tprintf(1, _T(""));

	for (i = 1; i < point_number; i++)
	{
		Lin[X_AXIS].MeasureDist[i - 1] = abs(Lin[X_AXIS].zeroDist[X_AXIS][i] - Lin[X_AXIS].zeroDist[X_AXIS][i - 1]); 
		Lin[Y_AXIS].MeasureDist[i - 1] = abs(Lin[Y_AXIS].zeroDist[Y_AXIS][i] - Lin[Y_AXIS].zeroDist[Y_AXIS][i - 1]);
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].MeasureDist[%d] %04f \t Lin[Y_AXIS].MeasureDist[%d] %04f "), i-1, Lin[X_AXIS].MeasureDist[i-1], i-1, Lin[Y_AXIS].MeasureDist[i-1]);
	}

	Lin[X_AXIS].NegStroke = 0;
	Lin[Y_AXIS].NegStroke = 0;
	for (i = 0; i < half_num; i++)
	{
		Lin[X_AXIS].NegStroke += Lin[X_AXIS].MeasureDist[i];
		Lin[Y_AXIS].NegStroke += Lin[Y_AXIS].MeasureDist[i];
	};

	Lin[X_AXIS].PosStroke = 0;
	Lin[Y_AXIS].PosStroke = 0;
	for (i = half_num; i < point_number-1; i++) //2022.05.31
	{
		Lin[X_AXIS].PosStroke += Lin[X_AXIS].MeasureDist[i];
		Lin[Y_AXIS].PosStroke += Lin[Y_AXIS].MeasureDist[i];
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].NegStroke %04f "), Lin[X_AXIS].NegStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].PosStroke %04f "), Lin[X_AXIS].PosStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].NegStroke %04f "), Lin[Y_AXIS].NegStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].PosStroke %04f "), Lin[Y_AXIS].PosStroke);

	Lin[X_AXIS].TargetStroke = X_TargetStroke;
	Lin[Y_AXIS].TargetStroke = Y_TargetStroke;
	
	log_tprintf(1, _T(""));
	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].TargetStroke %04f "), Lin[X_AXIS].TargetStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].TargetStroke %04f "), Lin[Y_AXIS].TargetStroke);

	Lin[X_AXIS].InterStroke = Lin[X_AXIS].TargetStroke / half_num;
	Lin[Y_AXIS].InterStroke = Lin[Y_AXIS].TargetStroke / half_num;

	log_tprintf(1, _T(""));

	log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].InterStroke %04f "), Lin[X_AXIS].InterStroke);
	log_tprintf(1, _T("[linearity_compensation] Lin[Y_AXIS].InterStroke %04f "), Lin[Y_AXIS].InterStroke);

	log_tprintf(1, _T(""));

	for (i = 0; i < point_number; i++)
	{
		Lin[X_AXIS].IdealStroke[i] = Lin[X_AXIS].InterStroke * (i-half_num);
		Lin[Y_AXIS].IdealStroke[i] = Lin[Y_AXIS].InterStroke * (i-half_num);

		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].IdealStroke %04f,\t\t Lin[Y_AXIS].IdealStroke %04f "), Lin[X_AXIS].IdealStroke[i], Lin[Y_AXIS].IdealStroke[i] );
	}
	memset(Lin[X_AXIS].MeasureStroke, 0, sizeof(Lin[X_AXIS].MeasureStroke));
	memset(Lin[Y_AXIS].MeasureStroke, 0, sizeof(Lin[Y_AXIS].MeasureStroke));

	for (i = 0; i < half_num; i++)
	{
		for( k = i; k < half_num; k++)
		{
			Lin[X_AXIS].MeasureStroke[i] += Lin[X_AXIS].MeasureDist[k];
			Lin[Y_AXIS].MeasureStroke[i] += Lin[Y_AXIS].MeasureDist[k];
		}
		Lin[X_AXIS].MeasureStroke[i] *= (-1);
		Lin[Y_AXIS].MeasureStroke[i] *= (-1);
	}
	Lin[X_AXIS].MeasureStroke[half_num] = 0;
	Lin[Y_AXIS].MeasureStroke[half_num] = 0;
	
	for (i = half_num + 1; i < point_number; i++)
	{
		for( k = half_num; k < i; k++)
		{
			Lin[X_AXIS].MeasureStroke[i] += Lin[X_AXIS].MeasureDist[k];
			Lin[Y_AXIS].MeasureStroke[i] += Lin[Y_AXIS].MeasureDist[k];
		}
	}

	Lin[X_AXIS].StrokePerCmd = (Lin[X_AXIS].NegStroke + Lin[X_AXIS].PosStroke) / (Lin[X_AXIS].TargetStep * half_num * 2);
	Lin[Y_AXIS].StrokePerCmd = (Lin[Y_AXIS].NegStroke + Lin[Y_AXIS].PosStroke) / (Lin[Y_AXIS].TargetStep * half_num * 2);

	log_tprintf(1, _T(""));

	for (i = 0; i < point_number; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].MeasureStroke[%d] : %04f \t Lin[Y_AXIS].MeasureStroke[%d] : %04f"),i,Lin[X_AXIS].MeasureStroke[i], i, Lin[Y_AXIS].MeasureStroke[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));
	// Coeffi = (measuerstroke - idealstroke) / ( (measestroke[i-1] - measurestroke[i]) / targetstep )
	for (i = 0; i < point_number; i++)
	{
		if (i == point_number-1) 
		{
			// i == 6
			CoeffiX = (Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].IdealStroke[i]) / ((Lin[X_AXIS].MeasureStroke[i-1] - Lin[X_AXIS].MeasureStroke[i]) / Lin[X_AXIS].TargetStep);
			Lin[X_AXIS].Coeffi[i] = (u16)(Lin[X_AXIS].TargetStep * (i)) + round( CoeffiX ); //2022.05.31

			CoeffiY = (Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].IdealStroke[i]) / ((Lin[Y_AXIS].MeasureStroke[i-1] - Lin[Y_AXIS].MeasureStroke[i]) / Lin[Y_AXIS].TargetStep);
			Lin[Y_AXIS].Coeffi[i] = (u16)((Lin[Y_AXIS].TargetStep * (i)) + round( CoeffiY ));//2022.05.31
		}
		else
		{
			CoeffiX= (Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].IdealStroke[i]) / ((Lin[X_AXIS].MeasureStroke[i] - Lin[X_AXIS].MeasureStroke[i+1]) / Lin[X_AXIS].TargetStep);
			Lin[X_AXIS].Coeffi[i] = (u16)(Lin[X_AXIS].TargetStep * (i)) + round( CoeffiX );//2022.05.31

			CoeffiY = (Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].IdealStroke[i]) / ((Lin[Y_AXIS].MeasureStroke[i] - Lin[Y_AXIS].MeasureStroke[i+1]) / Lin[Y_AXIS].TargetStep);
			Lin[Y_AXIS].Coeffi[i] = (u16)((Lin[Y_AXIS].TargetStep * (i)) + round( CoeffiY ));//2022.05.31
		}

		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].Coeffi[%d] : %04d \t\t Lin[Y_AXIS].Coeffi[%d] : %04d"),i,Lin[X_AXIS].Coeffi[i], i,Lin[Y_AXIS].Coeffi[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));

	for (i = 0; i < point_number-1; i++)
	{
		Lin[X_AXIS].Slope[i] = (Lin[X_AXIS].Coeffi[i+1] - Lin[X_AXIS].Coeffi[i]) / Lin[X_AXIS].TargetStep;
		Lin[X_AXIS].SlopeFixed[i] = (u16)round((Lin[X_AXIS].Slope[i] * 1024));

		Lin[Y_AXIS].Slope[i] = (Lin[Y_AXIS].Coeffi[i+1] - Lin[Y_AXIS].Coeffi[i]) / Lin[Y_AXIS].TargetStep;
		Lin[Y_AXIS].SlopeFixed[i] = (u16)round((Lin[Y_AXIS].Slope[i] * 1024));
	}

	for (i = 0; i < point_number-1; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].Slope[%d] : %04f \t\t Lin[Y_AXIS].Slope[%d] : %04f"),i,Lin[X_AXIS].Slope[i], i,Lin[Y_AXIS].Slope[i]);
	}

	log_tprintf(1, _T(""));
	log_tprintf(1, _T(""));
	for (i = 0; i < point_number-1; i++)
	{
		log_tprintf(1, _T("[linearity_compensation] Lin[X_AXIS].SlopeFixed[%d] : %04d \t\t Lin[Y_AXIS].SlopeFixed[%d] : %04d"),i,Lin[X_AXIS].SlopeFixed[i], i,Lin[Y_AXIS].SlopeFixed[i]);
	}
	// Write Step Register
	RamWriteA(0xB4BE, point_number); //x point number
	RamWriteA(0xB4C0, xtarget_step); //x step

	RamWriteA(0xB5BE, point_number); //y point number
	RamWriteA(0xB5C0, ytarget_step); //y step
	
	dw9786_write_linearity_compensation();
	log_tprintf(1, _T("[dw9786_linearity_compensation] finished "));
}

int dw9786_ois_coilflux_compensation(sCoilFlux* flux)
{
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] start "));
	RamWriteA(0xB4D4, flux->x_p_cal[0]); //base
	RamWriteA(0xB4D6, flux->x_p_cal[1]); //top
	RamWriteA(0xB4D8, flux->x_p_cal[2]); //bottom
	RamWriteA(0xB4DA, flux->x_n_cal[0]); //base
	RamWriteA(0xB4DC, flux->x_n_cal[1]); //top
	RamWriteA(0xB4DE, flux->x_n_cal[2]); //bottom
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] x_p_cal_base: %d, x_p_cal_top: %d, x_p_cal_bottom: %d,"), flux->x_p_cal[0], flux->x_p_cal[1], flux->x_p_cal[2]);
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] x_n_cal_base: %d, x_n_cal_top: %d, x_n_cal_bottom: %d,"), flux->x_n_cal[0], flux->x_n_cal[1], flux->x_n_cal[2]);
	
	RamWriteA(0xB5D4, flux->y_p_cal[0]); //base
	RamWriteA(0xB5D6, flux->y_p_cal[1]); //top
	RamWriteA(0xB5D8, flux->y_p_cal[2]); //bottom
	RamWriteA(0xB5DA, flux->y_n_cal[0]); //base
	RamWriteA(0xB5DC, flux->y_n_cal[1]); //top
	RamWriteA(0xB5DE, flux->y_n_cal[2]); //bottom
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] y_p_cal_base: %d, y_p_cal_top: %d, y_p_cal_bottom: %d,"), flux->y_p_cal[0], flux->y_p_cal[1], flux->y_p_cal[2]);
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] y_n_cal_base: %d, y_n_cal_top: %d, y_n_cal_bottom: %d,"), flux->y_n_cal[0], flux->y_n_cal[1], flux->y_n_cal[2]);
	
	RamWriteA(0xB716, 0x8000); // enable coilflux compensation
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] enable coilflux compensation "));
	log_tprintf(1, _T("[dw9786_ois_coilflux_compensation] finished "));
	return 0;
}

int dw9786_af_coilflux_compensation(sCoilFlux* flux)
{
	log_tprintf(1, _T("[dw9786_af_coilflux_compensation] start "));

	RamWriteA(0xB694, flux->af_p_cal[0]); //base
	RamWriteA(0xB696, flux->af_p_cal[1]); //top
	RamWriteA(0xB698, flux->af_p_cal[2]); //bottom
	RamWriteA(0xB6DA, flux->af_n_cal[0]); //base
	RamWriteA(0xB6DC, flux->af_n_cal[1]); //top
	RamWriteA(0xB6DE, flux->af_n_cal[2]); //bottom
	log_tprintf(1, _T("[dw9786_af_coilflux_compensation] af_p_cal_base: %d, af_p_cal_top: %d, af_p_cal_bottom: %d,"), flux->af_p_cal[0], flux->af_p_cal[1], flux->af_p_cal[2]);
	log_tprintf(1, _T("[dw9786_af_coilflux_compensation] af_n_cal_base: %d, af_n_cal_top: %d, af_n_cal_bottom: %d,"), flux->af_n_cal[0], flux->af_n_cal[1], flux->af_n_cal[2]);
	
	RamWriteA(0xB71A, 0x8000); // enable coilflux compensation
	log_tprintf(1, _T("[dw9786_af_coilflux_compensation] enable coilflux compensation "));
	log_tprintf(1, _T("[dw9786_af_coilflux_compensation] finished "));
	return 0;
}

void dw9786_ois_lin_cross_comp_enable(void)
{
	RamWriteA(0xB710, 0x8000);
	RamWriteA(0xB712, 0x8000);
}

void dw9786_ois_lin_cross_comp_disable(void)
{
	RamWriteA(0xB710, 0x0000);
	RamWriteA(0xB712, 0x0000);
}

void dw9786_af_lin_comp_enable(void)
{
	RamWriteA(0xB718, 0x8000);
}

void dw9786_af_lin_comp_disable(void)
{
	RamWriteA(0xB718, 0x0000);
}

void set_ref_stroke(int ref_stroke, int ref_gyro_filter_out)
{
	REF_STROKE = ref_stroke;
	REF_GYRO_RESULT = ref_gyro_filter_out;
	log_tprintf(1, _T("REF_STROKE = %d, REF_GYRO_RESULT = %d"), REF_STROKE, REF_GYRO_RESULT);
}

#define PI 3.14159265
#define REF_ANGLE (1)
//#define REF_STROKE (130.56f)
//#define REF_GYRO_RESULT (1000)	//Gyro 1 degree
#define GET_POSITION_COUNT (3)

int circle_motion_test(unsigned short RADIUS, float ACCURACY, unsigned short DEG_STEP, unsigned short WAIT_TIME0, unsigned short WAIT_TIME1, unsigned short WAIT_TIME2)
{
	float REF_POSITION_RADIUS[2];
	signed short circle_adc[4][512];
	signed short circle_cnt;
	signed short ADC_PER_MIC[2];
	signed short POSITION_ACCURACY[2];
	signed short RADIUS_GYRO_RESULT;
	signed short TARGET_RADIUS[2];
	unsigned short GYRO_GAIN[2];
	unsigned short TARGET_POSITION[2];
	unsigned short MEASURE_POSITION[2];
	signed short MAX_POS[2];
	signed short DIFF[2];
	signed short deg;
	signed short get;
	int ng_points = 0;
	float cos_v, sin_v;
	float dot;
	unsigned short lens_ofst_x, lens_ofst_y;
	int hallx, hally;

	RamReadA(0xB414, &lens_ofst_x);		//Lens offset
	RamReadA(0xB514, &lens_ofst_y);
	log_tprintf(1, _T("Lens ofst X = 0x%04X -- Lens ofst Y = 0x%04X"), lens_ofst_x, lens_ofst_y);


	RamReadA(0xB806, &GYRO_GAIN[X_AXIS]);	//Read Gyro gain
	RamReadA(0xB808, &GYRO_GAIN[Y_AXIS]);
	log_tprintf(1, _T("Gyro Gain X = 0x%04X -- Gyro Gain Y = 0x%04X"), GYRO_GAIN[X_AXIS], GYRO_GAIN[Y_AXIS]);

	REF_POSITION_RADIUS[X_AXIS] = ((float)REF_GYRO_RESULT * H2D(GYRO_GAIN[X_AXIS])) / 8192.0f; //[code/deg]
	REF_POSITION_RADIUS[Y_AXIS] = ((float)REF_GYRO_RESULT * H2D(GYRO_GAIN[Y_AXIS])) / 8192.0f;

	log_tprintf(1, _T("REF_POSITION_RADIUS_X = %f -- REF_POSITION_RADIUS_Y = %f"), REF_POSITION_RADIUS[X_AXIS], REF_POSITION_RADIUS[Y_AXIS]);

	ADC_PER_MIC[X_AXIS] = (signed short)REF_POSITION_RADIUS[X_AXIS] / REF_STROKE; //[code/um]
	ADC_PER_MIC[Y_AXIS] = (signed short)REF_POSITION_RADIUS[Y_AXIS] / REF_STROKE;

	log_tprintf(1, _T("ADC_PER_MIC_X = %06d[code/um] -- ADC_PER_MIC_Y = %06d[code/um]"), ADC_PER_MIC[X_AXIS], ADC_PER_MIC[Y_AXIS]);

	POSITION_ACCURACY[X_AXIS] = (signed short)((float)ADC_PER_MIC[X_AXIS] * ACCURACY);
	POSITION_ACCURACY[Y_AXIS] = (signed short)((float)ADC_PER_MIC[Y_AXIS] * ACCURACY);

	log_tprintf(1, _T("POSITION_ACCURACYX = %06d[code] -- POSITION_ACCURACY = %06d[code] -- ACCURACY : %.1f[um]"), POSITION_ACCURACY[X_AXIS], POSITION_ACCURACY[Y_AXIS], ACCURACY);

	RADIUS_GYRO_RESULT = (signed short)((float)(REF_GYRO_RESULT * RADIUS) / (float)REF_STROKE); //[deg/um]

	log_tprintf(1, _T("RADIUS_GYRO_RESULT = %06d"), RADIUS_GYRO_RESULT);

	TARGET_RADIUS[X_AXIS] = RADIUS_GYRO_RESULT * GYRO_GAIN[X_AXIS] >> 13; //[code]
	TARGET_RADIUS[Y_AXIS] = RADIUS_GYRO_RESULT * GYRO_GAIN[Y_AXIS] >> 13;

	log_tprintf(1, _T("TARGET_RADIUS_X = %06d[code] -- _Y = %06d[code]"), TARGET_RADIUS[X_AXIS], TARGET_RADIUS[Y_AXIS]);

	deg = 0;	//1st position
	TARGET_POSITION[X_AXIS] = TARGET_RADIUS[X_AXIS] * cos((double)deg);
	TARGET_POSITION[Y_AXIS] = TARGET_RADIUS[Y_AXIS] * sin((double)deg);

	RamWriteA(0xB100, TARGET_POSITION[X_AXIS]);		//Target position
	RamWriteA(0xB200, TARGET_POSITION[Y_AXIS]);

	m_delay(WAIT_TIME0);	//Move to 1st position
	circle_cnt = 0;
	ng_points = 0;

	log_tprintf(1, _T("STEP, DEGREE, TARGET_X, HALL_X, DIFF_X, TARGET_Y, HALL_Y, DIFF_Y"));

	for (deg = 0; deg <= 360; deg += DEG_STEP)
	{
		dot = deg * (3.14f / 180.0f);
		cos_v = cosf((float)dot);
		sin_v = sinf((float)dot);

		TARGET_POSITION[X_AXIS] = (unsigned short)((float)TARGET_RADIUS[X_AXIS] * cos_v);
		TARGET_POSITION[Y_AXIS] = (unsigned short)((float)TARGET_RADIUS[Y_AXIS] * sin_v);

		RamWriteA(0xB100, TARGET_POSITION[X_AXIS]);
		RamWriteA(0xB200, TARGET_POSITION[Y_AXIS]);
		m_delay(WAIT_TIME1);		//Delay for each step

		MAX_POS[X_AXIS] = -1;
		MAX_POS[Y_AXIS] = -1;

		DIFF[X_AXIS] = 0;
		DIFF[Y_AXIS] = 0;

		hallx = 0;
		hally = 0;

		for (get = 0; get < GET_POSITION_COUNT; get ++)
		{
			RamReadA(0xB102, &MEASURE_POSITION[X_AXIS]);	//Hall A/D code
			RamReadA(0xB202, &MEASURE_POSITION[Y_AXIS]);
			m_delay(WAIT_TIME2);	//Delay for each Hall Read

			hallx += H2D(MEASURE_POSITION[X_AXIS]);
			hally += H2D(MEASURE_POSITION[Y_AXIS]);
		}
		DIFF[X_AXIS] = abs(H2D(TARGET_POSITION[X_AXIS]) - hallx/GET_POSITION_COUNT);
		DIFF[Y_AXIS] = abs(H2D(TARGET_POSITION[Y_AXIS]) - hally/GET_POSITION_COUNT);

		MAX_POS[X_AXIS] = DIFF[X_AXIS];
		MAX_POS[Y_AXIS] = DIFF[Y_AXIS];

		circle_adc[0][circle_cnt] = H2D(TARGET_POSITION[X_AXIS]);
		circle_adc[1][circle_cnt] = (signed short)hallx/GET_POSITION_COUNT;
		circle_adc[2][circle_cnt] = H2D(TARGET_POSITION[Y_AXIS]);
		circle_adc[3][circle_cnt] = (signed short)hally/GET_POSITION_COUNT;	

		log_tprintf(1, _T("%6d, %6d, %6d, %6d, %6d, %6d, %6d, %6d"),circle_cnt, deg, circle_adc[0][circle_cnt], circle_adc[1][circle_cnt], 
			MAX_POS[X_AXIS], circle_adc[2][circle_cnt], circle_adc[3][circle_cnt], MAX_POS[Y_AXIS]);

		HallAccuray.g_TargetAdc[0][circle_cnt] = circle_adc[0][circle_cnt]; //X target code
		HallAccuray.g_TargetAdc[1][circle_cnt] = circle_adc[1][circle_cnt]; //X axis hall position
		HallAccuray.g_TargetAdc[2][circle_cnt] = circle_adc[2][circle_cnt]; //Y target code
		HallAccuray.g_TargetAdc[3][circle_cnt] = circle_adc[3][circle_cnt]; //Y axis hall position
		HallAccuray.g_DIFF[X_AXIS][circle_cnt] = MAX_POS[X_AXIS]; //X error code
		HallAccuray.g_DIFF[Y_AXIS][circle_cnt] = MAX_POS[Y_AXIS]; //Y error code
		HallAccuray.g_ADC_PER_MIC[X_AXIS] = ADC_PER_MIC[X_AXIS]; //X adc per um
		HallAccuray.g_ADC_PER_MIC[Y_AXIS] = ADC_PER_MIC[Y_AXIS]; //X adc per um
		
		circle_cnt++;

		// Calculate total points that over Spec
		if ((MAX_POS[X_AXIS] > POSITION_ACCURACY[X_AXIS]) ||  (MAX_POS[Y_AXIS] > POSITION_ACCURACY[Y_AXIS]))
		{
			ng_points ++;
		}
	}
	RamWriteA(0xB100, 0);
	RamWriteA(0xB200, 0);

	HallAccuray.g_NG_Point = ng_points;
	log_tprintf(1, _T("NG points = %d"), ng_points );

	return ng_points;
}

//Get Hall accuracy data
void get_hall_accuracy_data(sHallAccuracy *getHallAccuracy)
{
	memcpy(getHallAccuracy,&HallAccuray, sizeof(sHallAccuracy));
}

void dw9786_x_hall_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB400, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] hall_max = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB402, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] hall_min = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB404, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] hall_range = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB406, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] hall_adj_bias = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB408, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] hall_adj_amp_offs = 0x%04X"), r_dat);
	RamReadA(0xB410, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] dig_gain = 0x%04X"), r_dat);
	RamReadA(0xB412, &r_dat);
	log_tprintf(1, _T("[dw9786_x_hall_reg_info] dig_offs = 0x%04X"), r_dat);
}

void dw9786_y_hall_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB500, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] hall_max = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB502, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] hall_min = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB504, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] hall_range = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB506, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] hall_adj_bias = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB508, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] hall_adj_amp_offs = 0x%04X"), r_dat);
	RamReadA(0xB510, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] dig_gain = 0x%04X"), r_dat);
	RamReadA(0xB512, &r_dat);
	log_tprintf(1, _T("[dw9786_y_hall_reg_info] dig_offs = 0x%04X"), r_dat);
}

void dw9786_af_hall_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB600, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] hall_max = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB602, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] hall_min = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB604, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] hall_range = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB606, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] hall_adj_bias = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB608, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] hall_adj_amp_offs = 0x%04X"), r_dat);
	RamReadA(0xB610, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] dig_gain = 0x%04X"), r_dat);
	RamReadA(0xB612, &r_dat);
	log_tprintf(1, _T("[dw9786_af_hall_reg_info] dig_offs = 0x%04X"), r_dat);
}

void dw9786_hall_margin_adj_reg_info(void)
{
	u16 r_dat;
	RamReadA(0xB40A, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] x margin_adj_pos = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
	RamReadA(0xB40C, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] x margin_adj_neg = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
	
	RamReadA(0xB50A, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] y margin_adj_pos = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
	RamReadA(0xB50C, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] y margin_adj_neg = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
	
	RamReadA(0xB50A, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] af margin_adj_pos = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
	RamReadA(0xB50C, &r_dat);
	log_tprintf(1, _T("[dw9786_hall_margin_adj_reg_info] af margin_adj_neg = 0x%04X(%d%%)"), r_dat, (short)(r_dat/10));
}

void dw9786_lens_offs_reg_info(void)
{
	u16 r_dat;
	RamReadA(0xB414, &r_dat);
	log_tprintf(1, _T("[dw9786_lens_offs_reg_info] x lens_offset = 0x%04X(%d)"), r_dat, (short)r_dat);
	RamReadA(0xB514, &r_dat);
	log_tprintf(1, _T("[dw9786_lens_offs_reg_info] y lens_offset = 0x%04X(%d)"), r_dat, (short)r_dat);
}

void dw9786_servo_loop_gain_reg_info(void)
{
	u16 r_dat;
	RamReadA(0xB416, &r_dat);
	log_tprintf(1, _T("[dw9786_servo_loop_gain_reg_info] servo_loop_gain = 0x%04X(%d)"), r_dat, r_dat);
	RamReadA(0xB516, &r_dat);
	log_tprintf(1, _T("[dw9786_servo_loop_gain_reg_info] servo_loop_gain = 0x%04X(%d)"), r_dat, r_dat);
	RamReadA(0xB616, &r_dat);
	log_tprintf(1, _T("[dw9786_servo_loop_gain_reg_info] servo_loop_gain = 0x%04X(%d)"), r_dat, r_dat);
}

void dw9786_x_linearity_reg_info(void)
{
	u16 lin_offset;
	u16 lin_Slope;
	u16 Ois_lin_en;

	RamReadA(0xB710,&Ois_lin_en);
	log_tprintf(1, _T("[dw9786_x_linearity_reg_info] Ois_lin_en = 0x%04X"), Ois_lin_en);
		
	for(int i=0 ; i < 21 ; i++)
	{
		RamReadA(0xB41A+2*i,&lin_offset);
		log_tprintf(1, _T("[dw9786_x_linearity_reg_info] lin_offset[%d] = 0x%04X"), i, lin_offset);
	}

	for(int i=0 ; i < 20 ; i++)
	{
		RamReadA(0xB444+2*i,&lin_Slope);
		log_tprintf(1, _T("[dw9786_x_linearity_reg_info] lin_Slope[%d] = 0x%04X"), i, lin_Slope);
	}
}

void dw9786_x_crosstalk_reg_info(void)
{
	u16 crosstalk_offset;
	u16 crosstalk_slope;
	u16 Ois_Crosstalk_en;

	RamReadA(0xB712,&Ois_Crosstalk_en);
	log_tprintf(1, _T("[dw9786_x_crosstalk_reg_info] Ois_Crosstalk_en = 0x%04X"), Ois_Crosstalk_en);

	for(int i=0 ; i < 21 ; i++)
	{
		RamReadA(0xB46C+2*i,&crosstalk_offset);
		log_tprintf(1, _T("[dw9786_x_crosstalk_reg_info] crosstalk_offset[%d] = 0x%04X"), i, crosstalk_offset);
	}

	for(int i=0 ; i < 20 ; i++)
	{
		RamReadA(0xB496+2*i,&crosstalk_slope);
		log_tprintf(1, _T("[dw9786_x_crosstalk_reg_info] crosstalk_slope[%d] = 0x%04X"), i, crosstalk_slope);
	}
}

void dw9786_y_linearity_reg_info(void)
{
	u16 lin_offset;
	u16 lin_Slope;
	u16 Ois_lin_en;

	RamReadA(0xB710,&Ois_lin_en);
	log_tprintf(1, _T("[dw9786_y_linearity_reg_info] Ois_lin_en = 0x%04X"), Ois_lin_en);

	for(int i=0 ; i < 21 ; i++)
	{
		RamReadA(0xB51A+2*i,&lin_offset);
		log_tprintf(1, _T("[dw9786_y_linearity_reg_info] lin_offset[%d] = 0x%04X"), i, lin_offset);
	}

	for(int i=0 ; i < 20 ; i++)
	{
		RamReadA(0xB544+2*i,&lin_Slope);
		log_tprintf(1, _T("[dw9786_y_linearity_reg_info] lin_Slope[%d] = 0x%04X"), i, lin_Slope);
	}
}

void dw9786_y_crosstalk_reg_info(void)
{
	u16 crosstalk_offset;
	u16 crosstalk_slope;
	u16 Ois_Crosstalk_en;

	RamReadA(0xB712,&Ois_Crosstalk_en);
	log_tprintf(1, _T("[dw9786_y_crosstalk_reg_info] Ois_Crosstalk_en = 0x%04X"), Ois_Crosstalk_en);

	for(int i=0 ; i < 21 ; i++)
	{
		RamReadA(0xB56C+2*i,&crosstalk_offset);
		log_tprintf(1, _T("[dw9786_y_crosstalk_reg_info] crosstalk_offset[%d] = 0x%04X"), i, crosstalk_offset);
	}

	for(int i=0 ; i < 20 ; i++)
	{
		RamReadA(0xB596+2*i,&crosstalk_slope);
		log_tprintf(1, _T("[dw9786_y_crosstalk_reg_info] crosstalk_slope[%d] = 0x%04X"), i, crosstalk_slope);
	}
}

void dw9786_af_lin_cross_reg_info(void)
{
	u16 lin_slope;
	u16 Af_lin_en;

	RamReadA(0xB718,&Af_lin_en);
	log_tprintf(1, _T("[dw9786_af_lin_cross_reg_info] Af_lin_en = 0x%04X"), Af_lin_en);

	for(int i=0 ; i < 61 ; i++)
	{
		RamReadA(0xB61A+2*i,&lin_slope);
		log_tprintf(1, _T("[dw9786_af_lin_cross_reg_info] lin_slope[%d] = 0x%04X"), i, lin_slope);
	}
}

void dw9786_coil_flux_info(void)
{
	u16 flux_coeff;
	u16 Ois_flux_en;
	u16 Af_flux_en;

	RamReadA(0xB716,&Ois_flux_en);
	log_tprintf(1, _T("[dw9786_coil_flux_info] Ois_flux_en = 0x%04X"), Ois_flux_en);

	RamReadA(0xB71A,&Af_flux_en);
	log_tprintf(1, _T("[dw9786_coil_flux_info] Af_flux_en = 0x%04X"), Af_flux_en);
	
	for(int i=0 ; i < 6 ; i++)
	{
		RamReadA(0xB4D4+2*i,&flux_coeff);
		log_tprintf(1, _T("[dw9786_coil_flux_info] x_coil_flux_coeff[%d] = 0x%04X"), i, flux_coeff);
	}
	
	for(int i=0 ; i < 6 ; i++)
	{
		RamReadA(0xB5D4+2*i,&flux_coeff);
		log_tprintf(1, _T("[dw9786_coil_flux_info] y_coil_flux_coeff[%d] = 0x%04X"), i, flux_coeff);
	}
	
	for(int i=0 ; i < 6 ; i++)
	{
		RamReadA(0xB6D4+2*i,&flux_coeff);
		log_tprintf(1, _T("[dw9786_af_lin_cross_reg_info] af_coil_flux_coeff[%d] = 0x%04X"), i, flux_coeff);
	}
}

void dw9786_x_ois_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB800, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] gyro_gain_pol = 0x%04X"), r_dat);
	RamReadA(0xB806, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] gyro_gain = 0x%04X"), r_dat);
	RamReadA(0xB80C, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] gyro_offset = 0x%04X"), r_dat);
	RamReadA(0xB812, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] rot_mat_cos = 0x%04X"), r_dat);
	RamReadA(0xB818, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] rot_mat_sin = 0x%04X"), r_dat);
	RamReadA(0xB4EA, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] posture_up = 0x%04X"), r_dat);
	RamReadA(0xB4EC, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] posture_down = 0x%04X"), r_dat);
	RamReadA(0xB81E, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] acc_gain_pol = 0x%04X"), r_dat);
	RamReadA(0xB824, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] acc_gain = 0x%04X"), r_dat);
	RamReadA(0xB82A, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] acc_rot_mat_cos = 0x%04X"), r_dat);
	RamReadA(0xB830, &r_dat);
	log_tprintf(1, _T("[dw9786_x_ois_reg_info] acc_rot_mat_sin = 0x%04X"), r_dat);
}

void dw9786_y_ois_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB802, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] gyro_gain_pol = 0x%04X"), r_dat);
	RamReadA(0xB808, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] gyro_gain = 0x%04X"), r_dat);
	RamReadA(0xB80E, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] gyro_offset = 0x%04X"), r_dat);
	RamReadA(0xB814, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] rot_mat_cos = 0x%04X"), r_dat);
	RamReadA(0xB81A, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] rot_mat_sin = 0x%04X"), r_dat);
	RamReadA(0xB5EA, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] posture_up = 0x%04X"), r_dat);
	RamReadA(0xB5EC, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] posture_down = 0x%04X"), r_dat);
	RamReadA(0xB820, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] acc_gain_pol = 0x%04X"), r_dat);
	RamReadA(0xB826, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] acc_gain = 0x%04X"), r_dat);
	RamReadA(0xB82C, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] acc_rot_mat_cos = 0x%04X"), r_dat);
	RamReadA(0xB832, &r_dat);
	log_tprintf(1, _T("[dw9786_y_ois_reg_info] acc_rot_mat_sin = 0x%04X"), r_dat);
}

void dw9786_gyro_imu_reg_info(void)
{
	u16 r_dat = 0;

	RamReadA(0xB836, &r_dat);
	log_tprintf(1, _T("[dw9786_gyro_imu_reg_info] imu_select = 0x%04X"), r_dat);
}

void dw9786_comp_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB710, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] ois_lin_en = 0x%04X"), r_dat);
	RamReadA(0xB712, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] ois_Crosstalk_en = 0x%04X"), r_dat);
	RamReadA(0xB714, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] ois_Posture_en = 0x%04X"), r_dat);
	RamReadA(0xB716, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] ois_flux_en = 0x%04X"), r_dat);
	RamReadA(0xB718, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] af_lin_en = 0x%04X"), r_dat);
	RamReadA(0xB71A, &r_dat);
	log_tprintf(1, _T("[dw9786_af_comp_reg_info] af_flux_en = 0x%04X"), r_dat);
}

void dw9786_cal_status_reg_info(void)
{
	u16 r_dat;

	RamReadA(0xB700, &r_dat);
	log_tprintf(1, _T("[dw9786_cal_status_reg_info] ois_hall = 0x%04X"), r_dat);
	RamReadA(0xB702, &r_dat);
	log_tprintf(1, _T("[dw9786_cal_status_reg_info] ois_servo_loop = 0x%04X"), r_dat);
//	RamReadA(0xB704, &r_dat);
//	log_tprintf(1, _T("[dw9786_cal_status_reg_info] gyro_ofs = 0x%04X"), r_dat);
	RamReadA(0xB706, &r_dat);
	log_tprintf(1, _T("[dw9786_cal_status_reg_info] lens_ofs = 0x%04X"), r_dat);
	RamReadA(0xB70A, &r_dat);
	log_tprintf(1, _T("[dw9786_cal_status_reg_info] af_hall = 0x%04X"), r_dat);
	RamReadA(0xB70C, &r_dat);
	log_tprintf(1, _T("[dw9786_cal_status_reg_info] af_servo_loop = 0x%04X"), r_dat);

}

void dw9786_cal_chksum_reg_info(void)
{
	u16 cal_checksum_H, cal_checksum_L = 0;

	RamReadA(0xB720, &cal_checksum_H);
	log_tprintf(1, _T("[dw9786_cal_chksum_reg_info] cal_checksum_H = 0x%04X"), cal_checksum_H);
	RamReadA(0xB722, &cal_checksum_L);
	log_tprintf(1, _T("[dw9786_cal_chksum_reg_info] cal_checksum_L = 0x%04X"), cal_checksum_L);
}

void set_af_target(short target)
{
	switch (af_control_bit) 
	{
		case CTRL_10BIT:
			af_target_10bit(target);
			break;

		case CTRL_12BIT:
			af_target_12bit(target);
			break;

		case CTRL_14BIT:
			af_target_14bit(target);
			break;

		default:     
			break;    
	}	
}

void af_target_10bit(short target)
{
	unsigned char afTarget[2];

	afTarget[0] = ((target << 6) >> 8) & 0xff;
	afTarget[1] = (target << 6) & 0xff;
	
	ByteWriteA(_SLV_AF_, 0x00, afTarget[0]);
	ByteWriteA(_SLV_AF_, 0x01, afTarget[1]);
}

void af_target_12bit(short target)
{
	unsigned char afTarget[2];

	afTarget[0] = ((target << 4) >> 8) & 0xff;
	afTarget[1] = (target << 4) & 0xff;
	
	ByteWriteA(_SLV_AF_, 0x00, afTarget[0]);
	ByteWriteA(_SLV_AF_, 0x01, afTarget[1]);
}

void af_target_14bit(short target)
{
	unsigned char afTarget[2];

	afTarget[0] = ((target << 2) >> 8) & 0xff;
	afTarget[1] = (target << 2) & 0xff;
	
	ByteWriteA(_SLV_AF_, 0x00, afTarget[0]);
	ByteWriteA(_SLV_AF_, 0x01, afTarget[1]);
}

int CalcCompenPoint_9786_AF(int calpoint)
{
	int nIndexLowBound, nIndexUpBound = 0;
	int nCompenPoint;
	int sectionIndex;
	double sectionslope;
	int i;
	for ( i = 0; i < (nTargetcnt - 1); i++)
	{
		if (g_fRealTargetScale[i] < g_nTargetPoint[calpoint] && g_nTargetPoint[calpoint] <= g_fRealTargetScale[i + 1])
		{
			sectionslope = target_interval / (g_fRealTargetScale[i + 1] - g_fRealTargetScale[i]);
			nCompenPoint = (sectionslope * (g_nTargetPoint[calpoint] - g_fRealTargetScale[i])) + g_nTargetPoint[i];
		}
	}
	return nCompenPoint;
}

void InputValLoad_9786_AF(double *ldmdata, int point, int range)
{
	int i;
	nTargetcnt = point;
	Target_range = range;
	
	//nTargetcnt = 31;
	//Target_range = 16383
	target_interval = (Target_range / (nTargetcnt - 1));
	
	log_tprintf(1, _T("[InputValLoad_9786_AF] target_interval = %d"), (short)target_interval);
	
	for (i = 0; i < nTargetcnt; i++)
	{
		g_fRealStroke[i] = ldmdata[i] - ldmdata[0];
	}
	for (i = 0; i < nTargetcnt; i++)
	{
		g_fRealTargetScale[i] = g_fRealStroke[i] / g_fRealStroke[nTargetcnt - 1] * Target_range;
		
	}
}

void OutputCoeff_9786_AF(int *coeff)
{
	int i;
	for (i = 0; i < (nTargetcnt - 1); i++)
	{
		g_nTargetPoint[i] = target_interval * (i);
	}
	g_nTargetPoint[nTargetcnt - 1] = (Target_range - 1);
	for (i = 0; i < (nTargetcnt - 1); i++)
	{
		g_nCompenPoint[i] = CalcCompenPoint_9786_AF(i + 1);
	}
	for (i = 0; i < (nTargetcnt - 1); i++)
	{
		coeff[i] = g_nCompenPoint[i];
	}
}

int fra_idx_arr[1000];

int SearchKeyInArr(double* arr, int arrCapacity, double key){
	int i;
	for (i = 0; i < arrCapacity; i++){
		if (arr[i] == key){
			return 1;
		}
	}
	return 0;
}

void DeleteDuplicatedElementsInArr(double* arr, int* arrCapacity){
	double* uniqueArr = (double*)calloc(*arrCapacity, sizeof(double));
	int uniqueArrCount = 0;
	int cnt_idx = 0;
	int i;
	for (i = 0; i < *arrCapacity; i++){
		if (!SearchKeyInArr(uniqueArr, uniqueArrCount, arr[i])){
			uniqueArr[uniqueArrCount++] = arr[i];
			fra_idx_arr[cnt_idx++] = i;
		}	
	}
	memset(arr, 0, sizeof(arr[0]) * (*arrCapacity));
	for (i = 0; i < uniqueArrCount; i++){
		arr[i] = uniqueArr[i];
	}
	if (uniqueArr){
		free(uniqueArr);
		uniqueArr = NULL;
	}
	*arrCapacity = uniqueArrCount;
}

int Echo_FRA_Measurement(sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf)
{
	BYTE u08_dat1, u08_dat2;
	BYTE board_info;
	unsigned short dw9786_chipid;
	int cnt = 0;
	BYTE sts_check;
	BYTE err_list;
	//int freq;
	double gain, phase, freq;

	double* gain_temp;
	double* phase_temp;
	gain_temp = new double[fra_setting.test_point];
	phase_temp = new double[fra_setting.test_point];

	ByteReadA(FRA_BOARD_SLVID, 0x00, &board_info);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Info = 0x%2X"), board_info);

	if( board_info != FRA_BOARD_INFO)
	{
		log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Info Error!!!"));
		return FRA_BOARD_INFO_NG;
	}

	ByteReadA(FRA_BOARD_SLVID,0xF2, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0xF3, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Version(REG 0xF2): v%d.%d"), u08_dat1, u08_dat2);
	ByteReadA(FRA_BOARD_SLVID,0x32, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] VDD OUT(REG 0x32) = %d"), u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x33, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] CH1 AVDD(REG 0x33) = %d"), u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x34, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] CH1 IOVDD(REG 0x34) = %d"), u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x35, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] CH2 AVDD(REG 0x35) = %d"), u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x36, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] CH2 IOVDD(REG 0x36) = %d"), u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0xE1, &u08_dat1);
	log_tprintf(1, _T("[Echo_FRA_Measurement] AMP MODE(REG 0xE1) = %d"), u08_dat1);

	//==================================
	//		Calculate Frequency step
	//==================================
	for(int i=0; i<fra_setting.test_point; i++)
	{
		freq_buf[i] = fra_setting.start_freq * exp(log(fra_setting.end_freq / (double)fra_setting.start_freq) / (double)(fra_setting.test_point-1) * i );
		//log_tprintf(OUTPUT_LOG, _T("Index = %d -- %.2f\n"),i, freq_buf[i]);
	}

	ByteWriteA(FRA_BOARD_SLVID, 0x03, 0x01);							//FRA single point
	ByteWriteA(FRA_BOARD_SLVID, 0x31, 0x33);							//0x33: DW9786
	ByteWriteA(FRA_BOARD_SLVID, 0x30, fra_setting.ois_slave_id);		//OIS Slave address
	ByteWriteA(FRA_BOARD_SLVID, 0x6E, fra_setting.ois_mode);			//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
	ByteWriteA(FRA_BOARD_SLVID, 0x10, 0x00);	//target position
	ByteWriteA(FRA_BOARD_SLVID, 0x11, 0x00);

	ByteWriteA(FRA_BOARD_SLVID, 0x2F, fra_setting.ois_control_freq);	//0: 5KHz, 1: 10KHz, 0B: 15KHz
	ByteWriteA(FRA_BOARD_SLVID, 0x04, fra_setting.amplitude >> 8);		// Amplitude[mV] MSB 

	ByteWriteA(FRA_BOARD_SLVID, 0x05, fra_setting.amplitude & 0xFF);	// Amplitude[mV] LSB
	ByteReadA(FRA_BOARD_SLVID,0x04, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x05, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Amplitude(REG 0x04) = %d\r\n"), u08_dat1*256+u08_dat2);

	ByteWriteA(FRA_BOARD_SLVID, 0x06, fra_setting.dc_bias_ofst >> 8);		// DC Bias Offset[mV] MSB 
	ByteWriteA(FRA_BOARD_SLVID, 0x07, fra_setting.dc_bias_ofst & 0xFF);		// DC Bias Offset[mV] LSB

	ByteReadA(FRA_BOARD_SLVID,0x06, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x07, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Offset(REG 0x06) = %d\r\n"), u08_dat1*256+u08_dat2);

	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x01);	//FRA Mode ON
	m_delay(200);

	cnt = 0;
	while( cnt < TEST_LIMIT_CNT )
	{
		ByteReadA(FRA_BOARD_SLVID, 0x02, &sts_check);

		if(sts_check == 0x01)
		{
			log_tprintf(1, _T("[Echo_FRA_Measurement] Status check 0x%2X success...cnt = %d\n"), sts_check, cnt);
			break;
		}
		else
		{
			m_delay(TEST_DELAY_TIME);
			cnt++;

			if (cnt == TEST_LIMIT_CNT)
			{
				log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! Status check over time. cnt = %d\n"), cnt);

				ByteReadA(FRA_BOARD_SLVID, 0x09, &err_list);
				log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! Mode ON status check failed !!! -- Status = 0x%02X -- Error List = 0x%02X\n"), sts_check, err_list);
			
				ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);		// FRA Test Stop
				return MODE_ON_STS_CHK_ERR;
			}
		}
	}

	for(int i=0; i<fra_setting.test_point; i++)
	{
		//freq = (int)freq_buf[fra_setting.test_point-1-i]+0.5;
		
		freq = freq_buf[fra_setting.test_point-1-i];
		//log_tprintf(1, _T("[Echo_FRA_Measurement] Test frequency = %5d\n"),freq);

		ByteWriteA(FRA_BOARD_SLVID, 0x0C, (int)freq >> 8);		// Test Frequency[Hz] MSB 
		ByteWriteA(FRA_BOARD_SLVID, 0x0D, (int)freq & 0xFF);		// Test Frequency[Hz] LSB

		ByteReadA(FRA_BOARD_SLVID,0x0C, &u08_dat1);
		ByteReadA(FRA_BOARD_SLVID,0x0D, &u08_dat2);
		log_tprintf(1, _T("[Echo_FRA_Measurement] Test frequency(REG 0x0C) = %d\r\n"), u08_dat1*256+u08_dat2);

		ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x03);	// FRA Test Start
		m_delay(20);

		cnt = 0;
		while( cnt < TEST_LIMIT_CNT )
		{
			ByteReadA(FRA_BOARD_SLVID, 0x02, &sts_check);
			//log_tprintf(1, _T("[Echo_FRA_Measurement] cnt = %d -- status check: 0x%02X\n"),cnt, sts_check);

			if(sts_check == FRA_TEST_COMPLETE)	// Test complete
			{
				log_tprintf(1, _T("[Echo_FRA_Measurement] cnt = %d -- status check: 0x%02X\n"),cnt, sts_check);
				
				ByteReadA(FRA_BOARD_SLVID,0x20, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x21, &u08_dat2);
				freq = (unsigned short)(u08_dat1 * 256 + u08_dat2 ) / (double)10;

				ByteReadA(FRA_BOARD_SLVID,0x22, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x23, &u08_dat2);
				//m_delay(10);
				gain = (short)( u08_dat1 * 256 + u08_dat2 ) / (double)10;

				ByteReadA(FRA_BOARD_SLVID,0x24, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x25, &u08_dat2);
				//m_delay(10);
				phase = (short)( u08_dat1 * 256 + u08_dat2 ) / (double)10;

				log_tprintf(1, _T("Gain = %6.2f, Phase = %6.2f\n"),gain, phase);

				freq_buf[fra_setting.test_point-1-i] = freq;
				gain_buf[fra_setting.test_point-1-i] = gain;
				phase_buf[fra_setting.test_point-1- i] = phase;
				break;
			}
			else
			{
				m_delay(TEST_DELAY_TIME);
				cnt++;	
			}
		}

		if(sts_check != FRA_TEST_COMPLETE)
		{
			ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);	// FRA Test Stop
			log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! FRA Test Time's OUT !!! Test Freq. %.2f -- Measure not completed !!!\n"),freq);
			return TEST_STS_CHK_ERR;
		}
	}
	//Test Finished
	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);	// FRA Test Stop

/*
	//Print test results:
	log_tprintf(1, _T("==================================================\n\n"));
	log_tprintf(1, _T("Freq[Hz], Gain[dB], Phase[deg]\n"));

	for(int i=0; i<fra_setting.test_point; i++)
	{
		log_tprintf(1, _T("%6.1f,%6.1f,%6.1f\n"), freq_buf[i], gain_buf[i], phase_buf[i]);
	}
	log_tprintf(1, _T("==================================================\n\n"));
*/

	// deduplication
	cnt = fra_setting.test_point;
	DeleteDuplicatedElementsInArr(freq_buf, &fra_setting.test_point);

	memcpy(gain_temp, gain_buf, cnt * sizeof(double));
	memcpy(phase_temp, phase_buf, cnt * sizeof(double));

	memset(gain_buf, 0,  cnt * sizeof(double));
	memset(phase_buf, 0,  cnt * sizeof(double));

	for(int k = 0; k < fra_setting.test_point; k++)
	{
		gain_buf[k] = gain_temp[fra_idx_arr[k]];
		phase_buf[k] = phase_temp[fra_idx_arr[k]];
	}

	// After deduplication
	// Print test results:
	log_tprintf(1, _T("==================================================\n\n"));
	for(int i=0; i<fra_setting.test_point; i++)
	{
		log_tprintf(1, _T("%6.1f,%6.1f,%6.1f\n"), freq_buf[i], gain_buf[i], phase_buf[i]);
	}
	log_tprintf(1, _T("==================================================\n\n"));
	return OK;
}


int Echo_FRA_Measurement_SEMCO(sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf)
{
	BYTE u08_dat1, u08_dat2;
	BYTE board_info;
	unsigned short dw9786_chipid;
	int cnt = 0;
	BYTE sts_check;
	BYTE err_list;
	//int freq;
	double gain, phase, freq;

	double* gain_temp;
	double* phase_temp;
	gain_temp = new double[fra_setting.test_point];
	phase_temp = new double[fra_setting.test_point];

	ByteReadA(FRA_BOARD_SLVID, 0x00, &board_info);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Info = 0x%2X\r\n"), board_info);

	if( board_info != FRA_BOARD_INFO)
	{
		log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Info Error!!!\r\n"));
		return FRA_BOARD_INFO_NG;
	}

	ByteReadA(FRA_BOARD_SLVID,0xF2, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0xF3, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Board Version(REG 0xF2): v%02X%02X\r\n"), u08_dat1, u08_dat2);

	//==================================
	//		Calculate Frequency step
	//==================================
	for(int i=0; i<fra_setting.test_point; i++)
	{
		freq_buf[i] = fra_setting.start_freq - (i*fra_setting.step_freq);
		//log_tprintf(OUTPUT_LOG, _T("Index = %d -- %.2f\n"),i, freq_buf[i]);
	}

	ByteWriteA(FRA_BOARD_SLVID, 0x03, 0x01);							//FRA single point
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x31, 0x33);							//0x33: DW9786
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x30, fra_setting.ois_slave_id);		//OIS Slave address
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x6E, fra_setting.ois_mode);			//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x10, 0x00);	//target position
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x11, 0x00);
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x2F, fra_setting.ois_control_freq);	//0: 5KHz, 1: 10KHz, 0B: 15KHz
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x04, fra_setting.amplitude >> 8);		// Amplitude[mV] MSB 
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x05, fra_setting.amplitude & 0xFF);	// Amplitude[mV] LSB
	m_delay(10);

	ByteReadA(FRA_BOARD_SLVID,0x04, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x05, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Amplitude(REG 0x04) = %d\r\n"), u08_dat1*256+u08_dat2);

	ByteWriteA(FRA_BOARD_SLVID, 0x06, fra_setting.dc_bias_ofst >> 8);		// DC Bias Offset[mV] MSB 
	m_delay(10);

	ByteWriteA(FRA_BOARD_SLVID, 0x07, fra_setting.dc_bias_ofst & 0xFF);		// DC Bias Offset[mV] LSB
	m_delay(10);

	ByteReadA(FRA_BOARD_SLVID,0x06, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0x07, &u08_dat2);
	log_tprintf(1, _T("[Echo_FRA_Measurement] FRA Offset(REG 0x06) = %d\r\n"), u08_dat1*256+u08_dat2);

	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x01);	//FRA Mode ON
	m_delay(600);

	cnt = 0;
	while( cnt < TEST_LIMIT_CNT )
	{
		ByteReadA(FRA_BOARD_SLVID, 0x02, &sts_check);

		if(sts_check == 0x01)
		{
			log_tprintf(1, _T("[Echo_FRA_Measurement] Status check 0x%2X success...cnt = %d\n"), sts_check, cnt);
			break;
		}
		else
		{
			m_delay(TEST_DELAY_TIME);
			cnt++;

			if (cnt == TEST_LIMIT_CNT)
			{
				log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! Status check over time. cnt = %d\n"), cnt);

				ByteReadA(FRA_BOARD_SLVID, 0x09, &err_list);
				log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! Mode ON status check failed !!! -- Status = 0x%02X -- Error List = 0x%02X\n"), sts_check, err_list);
			
				ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);		// FRA Test Stop
				return MODE_ON_STS_CHK_ERR;
			}
		}
	}

	for(int i=0; i<fra_setting.test_point; i++)
	{
		//freq = (int)freq_buf[fra_setting.test_point-1-i]+0.5;
		
		freq = freq_buf[i];
		//log_tprintf(1, _T("[Echo_FRA_Measurement] Test frequency = %5d\n"),freq);

		ByteWriteA(FRA_BOARD_SLVID, 0x0C, (int)freq >> 8);		// Test Frequency[Hz] MSB 
		m_delay(10);

		ByteWriteA(FRA_BOARD_SLVID, 0x0D, (int)freq & 0xFF);		// Test Frequency[Hz] LSB
		m_delay(10);

		ByteReadA(FRA_BOARD_SLVID,0x0C, &u08_dat1);
		ByteReadA(FRA_BOARD_SLVID,0x0D, &u08_dat2);
		log_tprintf(1, _T("[Echo_FRA_Measurement] Test frequency(REG 0x0C) = %d\r\n"), u08_dat1*256+u08_dat2);

		ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x03);	// FRA Test Start
		m_delay(20);

		cnt = 0;
		while( cnt < TEST_LIMIT_CNT )
		{
			ByteReadA(FRA_BOARD_SLVID, 0x02, &sts_check);
			//log_tprintf(1, _T("[Echo_FRA_Measurement] cnt = %d -- status check: 0x%02X\n"),cnt, sts_check);

			if(sts_check == FRA_TEST_COMPLETE)	// Test complete
			{
				log_tprintf(1, _T("[Echo_FRA_Measurement] cnt = %d -- status check: 0x%02X\n"),cnt, sts_check);
				
				ByteReadA(FRA_BOARD_SLVID,0x20, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x21, &u08_dat2);
				//m_delay(10);
				freq = (unsigned short)( u08_dat1 * 256 + u08_dat2 ) / (double)10;

				ByteReadA(FRA_BOARD_SLVID,0x22, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x23, &u08_dat2);
				//m_delay(10);
				gain = (short)( u08_dat1 * 256 + u08_dat2 ) / (double)10;

				ByteReadA(FRA_BOARD_SLVID,0x24, &u08_dat1);
				//m_delay(10);
				ByteReadA(FRA_BOARD_SLVID,0x25, &u08_dat2);
				//m_delay(10);
				phase = (short)( u08_dat1 * 256 + u08_dat2 ) / (double)10;

				log_tprintf(1, _T("Freq = %6.2f, Gain = %6.2f, Phase = %6.2f\n"),freq, gain, phase);

				freq_buf[i] = freq;
				gain_buf[i] = gain;
				phase_buf[i] = phase;
				break;
			}
			else
			{
				m_delay(TEST_DELAY_TIME);
				cnt++;
			}
		}

		if(sts_check != FRA_TEST_COMPLETE)
		{
			ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00); // FRA Test Stop
			log_tprintf(1, _T("[Echo_FRA_Measurement] Error!!! FRA Test Time's OUT !!! Test Freq. %.2f -- Measure not completed !!!\n"),freq);
			return TEST_STS_CHK_ERR;
		}
	}
	//Test Finished
	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00); // FRA Test Stop

/*
	//Print test results:
	log_tprintf(1, _T("==================================================\n\n"));
	log_tprintf(1, _T("Freq[Hz], Gain[dB], Phase[deg]\n"));

	for(int i=0; i<fra_setting.test_point; i++)
	{
		log_tprintf(1, _T("%6.1f,%6.1f,%6.1f\n"), freq_buf[i], gain_buf[i], phase_buf[i]);
	}
	log_tprintf(1, _T("==================================================\n\n"));
*/

	// deduplication
	cnt = fra_setting.test_point;
	DeleteDuplicatedElementsInArr(freq_buf, &fra_setting.test_point);

	memcpy(gain_temp, gain_buf, cnt * sizeof(double));
	memcpy(phase_temp, phase_buf, cnt * sizeof(double));

	memset(gain_buf, 0,  cnt * sizeof(double));
	memset(phase_buf, 0,  cnt * sizeof(double));

	for(int k = 0; k < fra_setting.test_point; k++)
	{
		gain_buf[k] = gain_temp[fra_idx_arr[k]];
		phase_buf[k] = phase_temp[fra_idx_arr[k]];
	}

	// After deduplication
	// Print test results:
	log_tprintf(1, _T("==================================================\n\n"));
	for(int i=0; i<fra_setting.test_point; i++)
	{
		log_tprintf(1, _T("%6.1f,%6.1f,%6.1f\n"), freq_buf[i], gain_buf[i], phase_buf[i]);
	}
	log_tprintf(1, _T("==================================================\n\n"));
	return OK;
}

int Search_GM_PM(sFRA_Margin* fra_result, sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf)
{
	int msg = 0;
	int i = 0;
	int GM_flag = 0, PM_flag = 0;
	double gain_margin = 0, phase_margin = 0, freq_GM = 0, freq_PM = 0;

	log_tprintf(1, _T("test points = %d\n"), fra_setting.test_point);
	log_tprintf(1, _T("==================================================\n"));

	for(i=0; i<fra_setting.test_point-1; i++)
	{
		//Search Phase Margin
		if(gain_buf[i] * gain_buf[i+1] <= 0)	
		{
			if(PM_flag == 0)
			{
				phase_margin = phase_buf[i+1]+180;
				freq_PM = freq_buf[i+1];
				PM_flag = 1;
			}
			else if(phase_buf[i+1]+180 < phase_margin )
			{
				phase_margin = phase_buf[i+1]+180;
				freq_PM = freq_buf[i+1];
			}
		}

		//Search Gain Margin
		if(phase_buf[i] < -165 && phase_buf[i+1] > 165)
		{
			if( GM_flag == 0)
			{
				if(abs(phase_buf[i])> abs(phase_buf[i+1]))
				{
					gain_margin = gain_buf[i];
					freq_GM = freq_buf[i];
				}
				else
				{
					gain_margin = gain_buf[i+1];
					freq_GM = freq_buf[i+1];
				}
				GM_flag = 1;
				log_tprintf(1, _T("gain = %6.2f GM Freq[Hz] = %6.1f\n"), gain_margin, freq_GM);
			}
			else
			{
				if(abs(phase_buf[i])> abs(phase_buf[i+1]))
				{
					if(gain_buf[i] > gain_margin)
					{
						gain_margin = gain_buf[i];
						freq_GM = freq_buf[i];
					}
				}
				else
				{
					if(gain_buf[i+1] > gain_margin)
					{
						gain_margin = gain_buf[i+1];
						freq_GM = freq_buf[i+1];
					}
				}
				log_tprintf(1, _T("gain = %6.2f GM Freq[Hz] = %6.1f\n"), gain_margin, freq_GM);
			}
		}
	}

	fra_result->gain_margin_flag = GM_flag;
	fra_result->gain_margin = gain_margin;
	fra_result->gain_margin_freq = freq_GM;
	log_tprintf(1, _T("[Search_GM_PM] GM flag = %d, GM[dB] = %6.1f, GM Freq[Hz] = %6.1f\n"),GM_flag, gain_margin, freq_GM);

	fra_result->phase_margin_flag = PM_flag;
	fra_result->phase_margin = phase_margin;
	fra_result->phase_margin_freq = freq_PM;
	log_tprintf(1, _T("[Search_GM_PM] PM flag = %d, PM[deg] = %6.1f, PM Freq[Hz] = %6.1f\n"),PM_flag, phase_margin, freq_PM);
	log_tprintf(1, _T("==================================================\n"));
	return msg;
}

int Echo_Board_WhoAmI(void)
{
	BYTE board_info;

	ByteReadA(FRA_BOARD_SLVID, 0x00, &board_info);
	log_tprintf(1, _T("[Echo_Board_WhoAmI] FRA Board Info = 0x%2X"), board_info);

	return board_info;
}

int FRA_Interface_Mode(sFraInterfaceModeSetting fra_setting)
{
	BYTE sts_check = 0;
	BYTE err_list = 0;
	BYTE board_info;
	int cnt;
	BYTE u08_dat1, u08_dat2;
	
	ByteReadA(FRA_BOARD_SLVID, 0x00, &board_info);
	log_tprintf(1, _T("[FRA_Interface_Mode] FRA Board Info = 0x%2X\r\n"), board_info);
	
	if( board_info != FRA_BOARD_INFO)
	{
		log_tprintf(1, _T("[FRA_Interface_Mode] FRA Board Info Error!!!\r\n"));
		return FRA_BOARD_INFO_NG;
	}

	ByteReadA(FRA_BOARD_SLVID,0xF2, &u08_dat1);
	ByteReadA(FRA_BOARD_SLVID,0xF3, &u08_dat2);
	log_tprintf(1, _T("[FRA_Interface_Mode] FRA Board Version(REG0xF2): v%02X%02X\r\n"), u08_dat1, u08_dat2);

	ByteWriteA(FRA_BOARD_SLVID, 0x03, 0x05);							//FRA Interface Mode
	ByteWriteA(FRA_BOARD_SLVID, 0x31, 0x33);							//0x33: DW9786
	ByteWriteA(FRA_BOARD_SLVID, 0x30, fra_setting.ois_slave_id);		//OIS Slave address
	ByteWriteA(FRA_BOARD_SLVID, 0x6E, fra_setting.ois_mode);			//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
	ByteWriteA(FRA_BOARD_SLVID, 0x2F, fra_setting.ois_control_cycle);	//0x0B: 15KHz

	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x01);	//FRA Mode ON

	cnt = 0;
	while( cnt < STATUS_LIMIT_CNT )
	{
		ByteReadA(FRA_BOARD_SLVID, 0x02, &sts_check);

		if(sts_check == 0x01)
		{
			log_tprintf(1, _T("[FRA_Interface_Mode] Status check 0x%2X success...cnt = %d\r\n"), sts_check, cnt);
			break;
		}
		else
		{
			m_delay(TEST_DELAY_TIME);
			cnt++;

			if (cnt == STATUS_LIMIT_CNT)
			{
				log_tprintf(1, _T("[FRA_Interface_Mode] Status check over time. cnt = %d\r\n"), cnt);

				ByteReadA(FRA_BOARD_SLVID, 0x09, &err_list);
				log_tprintf(1, _T("[FRA_Interface_Mode] Mode ON status check failed !!! -- Status = 0x%02X -- Error List = 0x%02X\r\n"), sts_check, err_list);

				ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);	// FRA Test Stop
				return MODE_ON_STS_CHK_ERR;
			}
		}
	}

	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x03);	//FRA Test Start
	return OK;
}

void FRA_Test_Stop()
{
	log_tprintf(1, _T("[FRA_Interface_Mode] FRA Test Stop...\r\n"));
	ByteWriteA(FRA_BOARD_SLVID, 0x01, 0x00);	//FRA Test Stop
}

void dw9786_block_size_set(int block_size)
{
	log_tprintf(1, _T("[dw9786_block_size_set] original block size = %d"), MCS_PKT_SIZE);

	if(block_size % 4 == 0)
	{
		MCS_PKT_SIZE = block_size;	//block size has to be the multiple of 4
		log_tprintf(1, _T("[dw9786_block_size_set] set block size = %d"), block_size);
	}
	else
	{
		log_tprintf(1, _T("[dw9786_block_size_set] set block size is not the multiple of 4"));
	}
}
