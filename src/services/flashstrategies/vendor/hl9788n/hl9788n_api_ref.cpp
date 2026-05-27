

#include "stdafx.h"
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "hl9788n_api_ref.h"
#include "Func.h"
#include "string.h"
#include "math.h"
#include <tchar.h>
#include "mmsystem.h"
#include "stdint.h"


HL978x_ExtFuncList* g_stExtFnList = NULL;

extern	ptrWriteI2CDev WriteI2CDev;
extern	ptrReadI2CDev ReadI2CDev;
extern	ptrOutputLog  OutputLog;

extern uint8_t _SLV_OIS_;
uint8_t _SLV_AF_ = 0x18;
int af_control_bit = 12;
uint8_t FRA_BOARD_SLVID = 0x28;

stLinearity LCC[2];
stCrossComp Cross[2];
stPosture PostureCoor;
stHallAccuracy HallAcc;
float CoeffiX;
float CoeffiY;

char FW_File_Path[MAX_PATH];

extern TCHAR modulePath[_MAX_DIR];
extern TCHAR moduleFileName[_MAX_PATH];

int ref_stroke = 120;
int ref_gyro_result = 4000;

double ldm_input[INPUT_NUM] = { 0 };				// LDM data
double ldm_target[INPUT_NUM] = { 0 };				// LDM target
double ldm_ideal[INPUT_NUM] = { 0 };				// ideal data

double g_fFit_RealStroke[INPUT_NUM] = { 0 };
double g_fFit_RealStroke_temp[INPUT_NUM] = { 0 };

double g_fRealTargetScale[INPUT_NUM] = { 0 };		// LDM data to Target Scale
int g_nTargetPoint[INPUT_NUM];									// target capture
//double output_debug[Section_NUM+1];
int debug;

int target_interval = 512;
int target_range = 16383;
int section_num = target_range / (target_interval - 1);
int coeff_bit = 0;
int count = 33;
double full_stroke = 1000.0;
double af_sens = 0.0625;	// sensitivity [um/code]

int poly_sweep_data_num = Poly_Data_Length;
int poly_order = Poly_Order;
double rough_section[INPUT_NUM] = { 0.0 };
int test_buffer_init = 0;
int test_buffer_end = 0;
int test_buffer_center = 0;
double test_buffer[Poly_Data_Length] = { 0.0 };
double poly_x[Poly_Data_Length] = { 0.0 };
double fine_poly_result[20000] = { 0.0 }; //250114 update
double fine_section[INPUT_NUM] = { 0.0 };
double final_fitted_x_axis[INPUT_NUM] = { 0.0 };

double a[10 + 1] = { 0.0 };
double X[2 * 10 + 1] = { 0.0 };
double Y[10 + 1] = { 0.0 };
double B[10 + 1][10 + 2];

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

int wait_check_register(uint16_t reg, uint16_t ref)
{
	int ret = 0;
	uint16_t r_data;
	int i = 0;

	for(i = 0; i < LOOP_A; i++) {
		RamReadA(reg, &r_data); //Read status
		if(r_data == ref) {
			ret = FUNC_PASS;
			break;
		}
		else {
			if (i >= LOOP_B) {
				log_tprintf(1, _T("[wait_check_register]fail: 0x%04X"), r_data);
				return FUNC_FAIL;
			}
		}
		m_delay(WAIT_TIME);
	}	
	return ret;
}

int hl978x_extfuncinit(HL978x_ExtFuncList* pExtFuncList)
{
	g_stExtFnList = pExtFuncList;

	LOG_FUNC_START();
	
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

void hl9788n_wait_for_busy_clear(void)
{
	uint16_t busy_status = 0;
	uint16_t retry = 0;
	const uint16_t max_retry = 200;   // 200 * 5ms = 1s

	do {
		RamReadA(0xC000, &busy_status);
		if (!(busy_status & 0x0001))
			break;

		m_delay(5);
		retry++;

		if (retry >= max_retry) {
			LOG_FUNC_ERR("Busy flag timeout (status=0x%04X)", busy_status);
			break;
		}
	} while (1);
}

int hl9788n_id_check(void)
{
    uint16_t chip_id;
	
	LOG_FUNC_START();

    RamReadA(0xA108, &chip_id); // ID read
	LOG_FUNC_INFO("Chip ID: %04X", chip_id);

    if (chip_id != HL9788N_CHIP_ID) {
		LOG_FUNC_ERR("Invalid chip ID (Read: 0x%04X, Expected: 0x%04X)", chip_id, HL9788N_CHIP_ID);
        LOG_FUNC_END();
		return EXECUTE_ERROR;
    }

	LOG_FUNC_END();
    return chip_id;
}

void hl9788n_file_path(char *path)
{
	 memcpy(FW_File_Path,path, MAX_PATH);
	 LOG_FUNC_INFO("fw file path: %s", FW_File_Path);
}

void hl9788n_fw_info(void)
{
	uint16_t r_data;

	LOG_FUNC_START();

	RamReadA(0x7000, &r_data);
	LOG_FUNC_INFO("v%04X, Set version: 0x%02X [MSB], Project No.: 0x%02X [LSB]", r_data, r_data >> 8, r_data & 0xFF);

	RamReadA(0x7002, &r_data);
	LOG_FUNC_INFO("v%04X, FW version: 0x%02X [MSB], PID version: 0x%02X [LSB]", r_data, r_data >> 8, r_data & 0xFF);

	RamReadA(0x7004, &r_data);
	LOG_FUNC_INFO("FW date: %04X", r_data);

	LOG_FUNC_END();
}

int hl9788n_module_cal_store(void)
{
	uint16_t status;
	uint16_t exec_check = 0xA000;
	uint16_t done_check = 0xA001;

	LOG_FUNC_START();

	RamWriteA(0x6026, 0x000A);    // Store execute

	if (wait_check_register(0x6020, exec_check) == FUNC_PASS) {
		MODULE_PT;
		RamWriteA(0x6022, 0x0001); // Store execute
		m_delay(500);
	}
	else {
		LOG_FUNC_ERR("Execute fail at 0x6020 (expected 0x%04X)", exec_check);
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	if (wait_check_register(0x6020, done_check) == FUNC_PASS) {
		LOG_FUNC_INFO("Store complete (0x6020 = 0x%04X)", done_check);
	}
	else {
		LOG_FUNC_ERR("Store fail at 0x6020 (expected 0x%04X)", done_check);
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	LOG_FUNC_END();
	return FUNC_PASS;
}

int hl9788n_set_cal_store(void)
{
	// Keep address constants, only variable for check values
	uint16_t val_status_exec = 0xA000;
	uint16_t val_status_done = 0xA001;

	LOG_FUNC_START();

	// 1) Start SET store
	RamWriteA(0x6026, 0x000A);  // Store execute

	// 2) Wait for execute status
	if (wait_check_register(0x6020, val_status_exec) == FUNC_PASS) {
		SET_PT;
		RamWriteA(0x6022, 0x0001); // Store execute
		m_delay(100);
	}
	else {
		LOG_FUNC_ERR("Execute stage fail @0x6020");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	// 3) Wait for done status
	if (wait_check_register(0x6020, val_status_done) == FUNC_PASS) {
		LOG_FUNC_INFO("SET cal store complete");
	}
	else {
		LOG_FUNC_ERR("SET cal store fail @0x6020");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	LOG_FUNC_END();
	return FUNC_PASS;
}

int hl9788n_module_cal_erase(void)
{
	uint16_t val_status_exec = 0xB000;
	uint16_t val_status_done = 0xB001;

	LOG_FUNC_START();

	// 1) Start erase command
	RamWriteA(0x6026, 0x000B);   // Erase execute trigger
	m_delay(1);

	// 2) Wait until erase process starts (status = 0xB000)
	if (wait_check_register(0x6020, val_status_exec) == FUNC_PASS) {
		MODULE_PT;                       // Module protection / preparation
		RamWriteA(0x6022, 0x0001);       // Execute erase
		m_delay(100);
	}
	else {
		LOG_FUNC_ERR("Erase execute failed");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	// 3) Wait until erase process completes (status = 0xB001)
	if (wait_check_register(0x6020, val_status_done) == FUNC_PASS) {
		LOG_FUNC_INFO("Module cal erase complete");
	}
	else {
		LOG_FUNC_ERR("Module cal erase failed @0x6020");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	LOG_FUNC_END();
	return FUNC_PASS;
}

int hl9788n_set_cal_erase(void)
{
	/*
	Error code definition
	0  : No Error
	-1 : EXECUTE_ERROR
	*/
	// Keep register addresses fixed, use variables for status values
	uint16_t val_status_exec = 0xB000;
	uint16_t val_status_done = 0xB001;

	LOG_FUNC_START();

	// 1) Start erase command
	RamWriteA(0x6026, 0x000B);   // Erase execute trigger
	m_delay(1);

	// 2) Wait until erase process starts (status = 0xB000)
	if (wait_check_register(0x6020, val_status_exec) == FUNC_PASS) {
		SET_PT;                        // SET protection / preparation
		RamWriteA(0x6022, 0x0001);     // Execute erase
		m_delay(100);
	}
	else {
		LOG_FUNC_ERR("Erase execute failed");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	// 3) Wait until erase process completes (status = 0xB001)
	if (wait_check_register(0x6020, val_status_done) == FUNC_PASS) {
		LOG_FUNC_INFO("SET cal erase complete");
	}
	else {
		LOG_FUNC_ERR("SET cal erase failed");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	LOG_FUNC_END();
	return FUNC_PASS;
}


void hl978x_ois_slvid(BYTE hl978xn_slvid_8BIT)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("Before change, HL9788N_SLVID = 0x%02X", _SLV_OIS_);

	_SLV_OIS_ = hl978xn_slvid_8BIT;

	LOG_FUNC_INFO("After change, HL9788N_SLVID  = 0x%02X", _SLV_OIS_);

	LOG_FUNC_END();
}

void hl9788n_user_pt_off(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set USER protection OFF (disabled)");
	RamWriteA(0xA1F4, 0x8F48);
	RamWriteA(0xA1F6, 0xA8DC);
	LOG_FUNC_END();
}

void hl9788n_user_pt_on(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set USER protection ON (enabled)");
	RamWriteA(0xA1F4, 0x0000);
	RamWriteA(0xA1F6, 0x0000);
	LOG_FUNC_END();
}

void hl9788n_all_pt_off(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set ALL protection OFF (disabled)");
	RamWriteA(0xA3FC, 0x27D9);
	RamWriteA(0xA3FE, 0x1BEC);
	LOG_FUNC_END();
}

void hl9788n_all_pt_on(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set ALL protection ON (enabled)");
	RamWriteA(0xA3FC, 0x0000);
	RamWriteA(0xA3FE, 0x0000);
	LOG_FUNC_END();
}

void hl9788n_lut_pt_off(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set LUT protection OFF (disabled)");
	RamWriteA(0xA1F8, 0x8B40);
	RamWriteA(0xA1FA, 0x6AB3);
	LOG_FUNC_END();
}

void hl9788n_lut_pt_on(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set LUT protection ON (enabled)");
	RamWriteA(0xA1F8, 0x0000);
	RamWriteA(0xA1FA, 0x0000);
	LOG_FUNC_END();
}

void hl9788n_nvm_pt_off(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set NVM protection OFF (disabled)");
	RamWriteA(0xA1FC, 0x3C0E);
	RamWriteA(0xA1FE, 0x52A5);
	LOG_FUNC_END();
}

void hl9788n_nvm_pt_on(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set NVM protection ON (enabled)");
	RamWriteA(0xA1FC, 0x0000);
	RamWriteA(0xA1FE, 0x0000);
	LOG_FUNC_END();
}

void hl9788n_inst_pt_off(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set Instruction protection OFF (disabled)");
	RamWriteA(0xA3F8, 0xDEC0);
	RamWriteA(0xA3FA, 0x4C9D);	
	LOG_FUNC_END();
}

void hl9788n_inst_pt_on(void)
{
	LOG_FUNC_START();
	LOG_FUNC_INFO("Set Instruction protection ON (enabled)");
	RamWriteA(0xA3F8, 0x0000);
	RamWriteA(0xA3FA, 0x0000);		
	LOG_FUNC_END();
}

void hl9788n_check_pt(void)
{
	uint16_t chk_pt;

	LOG_FUNC_START();
	LOG_FUNC_INFO("Checking IC protection status");

	// Read protection register
	RamReadA(0xA136, &chk_pt);
	LOG_FUNC_INFO("IC protection status: 0x%04X", chk_pt);

	// Bit 4: All protection
	if (chk_pt & (1 << 4))
		LOG_FUNC_INFO("All protection is OFF (disabled)");
	else
		LOG_FUNC_INFO("All protection is ON (enabled)");

	// Bit 3: Instruction protection
	if (chk_pt & (1 << 3))
		LOG_FUNC_INFO("Instruction protection is OFF (disabled)");
	else
		LOG_FUNC_INFO("Instruction protection is ON (enabled)");

	// Bit 2: User protection
	if (chk_pt & (1 << 2))
		LOG_FUNC_INFO("User protection is OFF (disabled)");
	else
		LOG_FUNC_INFO("User protection is ON (enabled)");

	// Bit 1: LUT protection
	if (chk_pt & (1 << 1))
		LOG_FUNC_INFO("LUT protection is OFF (disabled)");
	else
		LOG_FUNC_INFO("LUT protection is ON (enabled)");

	// Bit 0: NVM protection
	if (chk_pt & (1 << 0))
		LOG_FUNC_INFO("NVM protection is OFF (disabled)");
	else
		LOG_FUNC_INFO("NVM protection is ON (enabled)");

	LOG_FUNC_END();
}

void hl9788n_set_pt_on(void)
{
	uint16_t chk_pt;

	LOG_FUNC_START();
	LOG_FUNC_INFO("Starting IC protection ON configuration");

	// Read current protection status
	RamReadA(0xA136, &chk_pt);
	LOG_FUNC_INFO("Current protection register: 0x%04X", chk_pt);

	// Bit 4: All protection
	if (chk_pt & (1 << 4)) {
		LOG_FUNC_INFO("All protection is OFF → Turning ON");
		hl9788n_all_pt_on();
	}
	
	// Bit 3: Instruction protection
	if (chk_pt & (1 << 3)) {
		LOG_FUNC_INFO("Instruction protection is OFF → Turning ON");
		hl9788n_inst_pt_on();
	}

	// Bit 2: User protection
	if (chk_pt & (1 << 2)) {
		LOG_FUNC_INFO("User protection is OFF → Turning ON");
		hl9788n_user_pt_on();
	}

	// Bit 1: LUT protection
	if (chk_pt & (1 << 1)) {
		LOG_FUNC_INFO("LUT protection is OFF → Turning ON");
		hl9788n_lut_pt_on();
	}

	// Bit 0: NVM protection
	if (chk_pt & (1 << 0)) {
		LOG_FUNC_INFO("NVM protection is OFF → Turning ON");
		hl9788n_nvm_pt_on();
	}

	LOG_FUNC_INFO("All required protections have been enabled");
	LOG_FUNC_END();
}


void hl9788n_ic_reset(void)
{
	LOG_FUNC_START();

	RamWriteA(0xA50C, 0x0010);   // MCU off
	hl9788n_all_pt_off();
	RamWriteA(0xA50C, 0x0100);   // sys_rst_cmd
	m_delay(11);
	//Automatically enters Active mode after a system reset
	LOG_FUNC_END();
}

void hl9788n_ois_reset(void)
{
	LOG_FUNC_START();

	hl9788n_user_pt_off();               // Disable protection
	RamWriteA(0xA50C, 0x0010);           // MCU off
	RamWriteA(0xA500, 0x0010);           // FMC reset
	hl9788n_user_pt_on();                // Enable protection
	m_delay(6);							 // auto load time
	RamWriteA(0xA50C, 0x0000);			 // MCU on
	m_delay(5); // It depends on the firmware
	LOG_FUNC_END();
}

void hl9788n_mcu_on_forced(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("Forced MCU ON");
	hl9788n_user_pt_off();
	RamWriteA(0xA500, 0x0030);   		 // Auto-Read Bypass On, FMC Reset
	RamWriteA(0xA50C, 0x0000);   		 // MCU On
	m_delay(5); 						 // It depends on the firmware
	hl9788n_user_pt_on();
	LOG_FUNC_END();
}

void hl9788n_mcu_on(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("MCU ON");
	RamWriteA(0xA50C, 0x0000);
	m_delay(5);

	LOG_FUNC_END();
}

void hl9788n_mcu_off(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("MCU OFF");
	RamWriteA(0xA50C, 0x0010);

	LOG_FUNC_END();
}

void hl9788n_chip_enable(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("Chip enabled (IC EN)");
	RamWriteA(0xA100, 0x0100);

	LOG_FUNC_END();
}

void hl9788n_chip_disable(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("Chip disabled (IC DIS, MCU OFF)");
	RamWriteA(0xA50C, 0x0010);
	RamWriteA(0xA100, 0x0000);

	LOG_FUNC_END();
}

void hl9788n_inst_erase(void)
{
	LOG_FUNC_START();

	LOG_FUNC_INFO("Instruction erase start");
	RamWriteA(0xC008, 0x0004);   // Erase command
	m_delay(1200);
	hl9788n_wait_for_busy_clear();
	LOG_FUNC_INFO("Instruction erase complete");

	LOG_FUNC_END();
}

void hl9786n_file_path(char *path)
{
	memcpy(FW_File_Path, path, MAX_PATH);
	LOG_FUNC_START();
	LOG_FUNC_INFO("FW file path: %s", FW_File_Path);
	LOG_FUNC_END();
}

void configure_dma(uint32_t mem_addr_val)
{
	uint32_t mem_mode = 0x00000000;
	uint32_t dma_ctrl = 0x02010200;
	uint32_t dma_saddr = 0x70000000;
	uint32_t dma_daddr = 0x7000C010;
	uint32_t dma_cnt = 0x00008000;
	uint32_t dma_clk = 0x00002000;

	uint16_t data_buf0;
	uint16_t data_buf1;
	
	LOG_FUNC_START();

	// 1. DMA Clock (0xA514)
	data_buf0 = dma_clk & 0xFFFF;
	data_buf1 = (dma_clk >> 16) & 0xFFFF;
	RamWriteA(0xA514, data_buf0);
	RamWriteA(0xA516, data_buf1);


	// 2. Memory Address (0xC00C)
	data_buf0 = mem_addr_val & 0xFFFF;
	data_buf1 = (mem_addr_val >> 16) & 0xFFFF;
	RamWriteA(0xC00C, data_buf0);
	RamWriteA(0xC00E, data_buf1);

	// 3. Memory Mode (0xC004)
	data_buf0 = mem_mode & 0xFFFF;
	data_buf1 = (mem_mode >> 16) & 0xFFFF;
	RamWriteA(0xC004, data_buf0);
	RamWriteA(0xC006, data_buf1);

	// 4. DMA Control (0xA810)
	data_buf0 = dma_ctrl & 0xFFFF;
	data_buf1 = (dma_ctrl >> 16) & 0xFFFF;
	RamWriteA(0xA810, data_buf0);
	RamWriteA(0xA812, data_buf1);

	// 5. DMA Source Address (0xA814)
	data_buf0 = dma_saddr & 0xFFFF;
	data_buf1 = (dma_saddr >> 16) & 0xFFFF;
	RamWriteA(0xA814, data_buf0);
	RamWriteA(0xA816, data_buf1);
	
	// 6. DMA Destination Address (0xA818)
	data_buf0 = dma_daddr & 0xFFFF;
	data_buf1 = (dma_daddr >> 16) & 0xFFFF;
	RamWriteA(0xA818, data_buf0);
	RamWriteA(0xA81A, data_buf1);

	// 7. DMA Count (0xA81C)
	data_buf0 = dma_cnt & 0xFFFF;
	data_buf1 = (dma_cnt >> 16) & 0xFFFF;
	RamWriteA(0xA81C, data_buf0);
	RamWriteA(0xA81E, data_buf1);

	LOG_FUNC_END();
}

/* ----------------------------------------------------------------------------
 * [MOTORDEV PATCH 2026-05-27] Progress / cancel hook globals.
 * Set via hl9788n_set_progress_cb / _cancel_check; NULL when unused.
 * -------------------------------------------------------------------------- */
static hl9788n_progress_cb_t   g_hl9788n_progress_cb   = NULL;
static hl9788n_cancel_check_t  g_hl9788n_cancel_check  = NULL;

void hl9788n_set_progress_cb(hl9788n_progress_cb_t cb)   { g_hl9788n_progress_cb = cb; }
void hl9788n_set_cancel_check(hl9788n_cancel_check_t cb) { g_hl9788n_cancel_check = cb; }

#define HL9788N_PATCH_REPORT(pct) do { if (g_hl9788n_progress_cb) g_hl9788n_progress_cb(pct); } while (0)
#define HL9788N_PATCH_CANCELLED() (g_hl9788n_cancel_check && g_hl9788n_cancel_check())

/* ----------------------------------------------------------------------------
 * [MOTORDEV PATCH 2026-05-27] FW update accepting external buffer.
 *
 * Body migrated from hl9788n_fw_update_dma(); the file-loading section is
 * gone (caller supplies fw_data). 5 hook points inserted for progress and
 * cancel: 2 burst-loop tops, 2 DMA-wait-loop tops, 1 verify-complete.
 *
 * @param fw_data     externally parsed firmware buffer (vendor layout)
 * @param word_count  number of uint16_t entries; must equal FW_SIZE (0x10000)
 * @return FUNC_PASS, FUNC_FAIL, EXECUTE_ERROR, or HL9788N_FW_CANCELLED
 * -------------------------------------------------------------------------- */
int hl9788n_fw_update_dma_with_buffer(const uint16_t *fw_data, uint32_t word_count)
{
	uint16_t addr;
	uint32_t dma_en = 0x00000001;
	uint16_t dma_en0 = dma_en & 0xFFFF;
	uint16_t dma_en1 = (dma_en >> 16) & 0xFFFF;
	uint32_t mem_addr = 0;
	uint16_t r_status;
	uint16_t timeout;

	LOG_FUNC_START();
	LOG_FUNC_INFO("DMA-based firmware update start (with_buffer, words=%u)", word_count);

	/* [MOTORDEV PATCH 2026-05-27] Expected word count = FW_SIZE / 2 (32768 words = 64KB);
	 * FW_SIZE 在 vendor 头里注释为 "64KB"（字节），故 word 数 = FW_SIZE / sizeof(uint16) = FW_SIZE/2。
	 * vendor flash 主体也只读 FW_DATA[0..FW_SIZE/2-1]（见原函数 for 循环）。 */
	const uint32_t kExpectedWords = (uint32_t)FW_SIZE / 2;
	if (fw_data == NULL || word_count != kExpectedWords) {
		LOG_FUNC_ERR("Invalid fw buffer (ptr=%p, words=%u, expected=%u)",
		             (const void *)fw_data, word_count, (unsigned)kExpectedWords);
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	/* [MOTORDEV PATCH v3 2026-05-27] 进度上报按"已烧入字节 / 总字节"线性计算。
	 * 总固件 = FW_SIZE = 65536 字节；每个 burst 写 MCS_PKT_SIZE*2 = 128 字节；
	 * 共 512 bursts，进度精度 = 100/512 ≈ 0.2% 每步。
	 * erase / DMA wait / verify 不传输数据，故进度条自然停顿在真实边界：
	 *   0%（erase 1.2s）→ 0→50% 平滑（第一段 burst 4.5s）→ 50% 停顿（DMA wait ~500ms）
	 *   → 50→100% 平滑（第二段 burst 4.5s）→ 100% 停顿（DMA wait + verify ~600ms） */
	HL9788N_PATCH_REPORT(0);

	hl9788n_mcu_off();
	RamWriteA(0xA518, 0x0717);      // CLK_PWM enabled to update the busy bit
	RamWriteA(0xC004, 0x0000);      // Select instruction memory

	hl9788n_inst_pt_off();
	hl9788n_inst_erase();

	LOG_FUNC_INFO("Starting write of the first 32KB firmware block");
	// First 32KB — 进度 0% → 50%（已烧 0 → FW_HALF_SIZE 字节）
	for (addr = 0; addr < FW_HALF_SIZE; addr += MCS_PKT_SIZE * 2) {
		if (HL9788N_PATCH_CANCELLED()) {
			LOG_FUNC_INFO("Cancelled during first 32KB burst at addr=0x%04X", addr);
			LOG_FUNC_END();
			return HL9788N_FW_CANCELLED;
		}
		RamWriteA_Burst(addr, fw_data + (addr / 2), MCS_PKT_SIZE);
		/* 已烧字节 = addr + 128；pct = 已烧 * 100 / FW_SIZE */
		HL9788N_PATCH_REPORT((int)(((unsigned long)(addr + MCS_PKT_SIZE * 2) * 100UL) / FW_SIZE));
	}
	configure_dma(mem_addr);

	RamWriteA(0xA800, dma_en0);
	RamWriteA(0xA802, dma_en1);

	m_delay(200);
	timeout = 1;
	for (int i = 0; i < 200; i++) {     // 200 ms max
		if (HL9788N_PATCH_CANCELLED()) {
			LOG_FUNC_INFO("Cancelled during first DMA wait at i=%d", i);
			LOG_FUNC_END();
			return HL9788N_FW_CANCELLED;
		}
		RamReadA(0xD804, &r_status);    // Check DMA Busy flag (PWM_flag)
		if (r_status == 0x0000) {
			timeout = 0;
			break;
		}
		m_delay(2);
	}

	if (timeout) {
		LOG_FUNC_INFO("DMA timeout on first 32KB (r_status != 0x0000)");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}
	LOG_FUNC_INFO("First 32KB firmware block written");

	LOG_FUNC_INFO("Starting write of the second 32KB firmware block");
	// Second 32KB — 进度 50% → 100%（已烧 FW_HALF_SIZE → FW_SIZE 字节）
	mem_addr = FW_HALF_SIZE;
	for (addr = 0; addr < FW_HALF_SIZE; addr += MCS_PKT_SIZE * 2) {
		if (HL9788N_PATCH_CANCELLED()) {
			LOG_FUNC_INFO("Cancelled during second 32KB burst at addr=0x%04X", addr);
			LOG_FUNC_END();
			return HL9788N_FW_CANCELLED;
		}
		RamWriteA_Burst(addr,
			fw_data + (FW_HALF_SIZE / 2) + (addr / 2),
			MCS_PKT_SIZE);
		/* 已烧字节 = FW_HALF_SIZE + addr + 128 */
		HL9788N_PATCH_REPORT((int)(((unsigned long)(FW_HALF_SIZE + addr + MCS_PKT_SIZE * 2) * 100UL) / FW_SIZE));
	}
	configure_dma(mem_addr);

	RamWriteA(0xA800, dma_en0);
	RamWriteA(0xA802, dma_en1);

	m_delay(200);
	timeout = 1;
	for (int i = 0; i < 200; i++) {     // 200 ms max
		if (HL9788N_PATCH_CANCELLED()) {
			LOG_FUNC_INFO("Cancelled during second DMA wait at i=%d", i);
			LOG_FUNC_END();
			return HL9788N_FW_CANCELLED;
		}
		RamReadA(0xD804, &r_status);    // Check DMA Busy flag (PWM_flag)
		if (r_status == 0x0000) {
			timeout = 0;
			break;
		}
		m_delay(2);
	}
	if (timeout) {
		LOG_FUNC_INFO("DMA timeout on second 32KB (r_status != 0x0000)");
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}
	LOG_FUNC_INFO("Second 32KB firmware block written");

	// ----------------- Reset & Verify -----------------
	// 100% 已在第二段最后一个 burst 上报，此处 verify 阶段无数据传输，进度条停留 100%
	hl9788n_user_pt_off();
	RamWriteA(0xA500, 0x0010); // fmc reset
	hl9788n_user_pt_on();
	hl9788n_inst_pt_on();
	m_delay(6); //auto load time

	if (hl9788n_rv_status() & CS_OK_INS) {
		LOG_FUNC_INFO("Firmware update completed successfully");
		LOG_FUNC_END();
		return FUNC_PASS;
	}
	else {
		LOG_FUNC_ERR("Firmware update verify failed");
		LOG_FUNC_END();
		return FUNC_FAIL;
	}
}

/* ----------------------------------------------------------------------------
 * Original hl9788n_fw_update_dma(): loads FW_File_Path into a static buffer,
 * then forwards to hl9788n_fw_update_dma_with_buffer().
 * Retained for backward compatibility with any vendor-style caller.
 * -------------------------------------------------------------------------- */
int hl9788n_fw_update_dma(void)
{
	static uint16_t FW_DATA_HL9788N[FW_SIZE] = { 0 };
	FILE *p_file = NULL;
	int idx = 0;
	char buffer[MAX_CHAR_LENGTH];

	LOG_FUNC_START();

	// Open FW file and load into buffer (text hex -> 32b per line -> split to 2x16b)
	if (fopen_s(&p_file, FW_File_Path, "rt") == 0) {
		while (fgets(buffer, MAX_CHAR_LENGTH, p_file) != NULL) {
			unsigned long val32 = strtoul(buffer, NULL, 16);
			if (idx + 1 >= FW_SIZE) {
				LOG_FUNC_ERR("FW buffer overflow at index %d", idx);
				fclose(p_file);
				LOG_FUNC_END();
				return EXECUTE_ERROR;
			}
			FW_DATA_HL9788N[idx++] = (uint16_t)(val32 & 0xFFFF);
			FW_DATA_HL9788N[idx++] = (uint16_t)((val32 >> 16) & 0xFFFF);
		}
		fclose(p_file);
		LOG_FUNC_INFO("FW file open & parse complete (words=%d)", idx);
	}
	else {
		LOG_FUNC_ERR("FW file open failed: %s", FW_File_Path);
		LOG_FUNC_END();
		return EXECUTE_ERROR;
	}

	return hl9788n_fw_update_dma_with_buffer(FW_DATA_HL9788N, (uint32_t)idx);
}

uint16_t hl9788n_rv_status(void)
{
	/*
        #define CS_OK_TM             (1 << 15)
        #define CS_OK_UM             (1 << 14)
        #define CS_OK_LUT            (1 << 13)
        #define CS_OK_INS            (1 << 12)

        #define RV_ERR_TM            (1 << 11)
        #define RV_ERR_UM            (1 << 10)
        #define RV_ERR_LUT           (1 <<  9)
        #define RV_ERR_INS           (1 <<  8)

        #define AUTOLOAD_DONE        (1 <<  4)
        #define FMC_BUSY             (1 <<  0)
    */
	uint16_t status = 0;

	LOG_FUNC_START();
	
	RamReadA(0xC000, &status);
	LOG_FUNC_INFO("Status = 0x%04X", status);

	if ((status & CS_OK_TM) == 0) LOG_FUNC_ERR("TM_REG Check-sum NOT OK");
	if ((status & CS_OK_UM) == 0) LOG_FUNC_ERR("UM_REG Check-sum NOT OK");
	if ((status & CS_OK_LUT) == 0) LOG_FUNC_ERR("LUT Check-sum NOT OK");
	if ((status & CS_OK_INS) == 0) LOG_FUNC_ERR("INS Check-sum NOT OK");

	if (status & RV_ERR_TM)   LOG_FUNC_ERR("TM_REG Verification Error");
	if (status & RV_ERR_UM)   LOG_FUNC_ERR("UM_REG Verification Error");
	if (status & RV_ERR_LUT)  LOG_FUNC_ERR("LUT Verification Error");
	if (status & RV_ERR_INS)  LOG_FUNC_ERR("INS Verification Error");

	if ((status & AUTOLOAD_DONE) == 0) LOG_FUNC_INFO("Auto-load NOT completed");
	if (status & FMC_BUSY)             LOG_FUNC_INFO("FMC is currently busy");

	LOG_FUNC_END();
	return status;
}
