/**
  ******************************************************************************
  * File Name          : DW9786_API.h
  * Description        : Main program header
  * DongWoon Anatech   :
  * Version            : 0.1
  ******************************************************************************
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
#ifndef _DW9786_OIS_H_
#define _DW9786_OIS_H_

//#define DJI

#ifdef DW9786_OIS_EXPORTS
#define _EXPORT __declspec(dllexport)
#else
#define _EXPORT __declspec(dllimport)
#endif

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;

/*-----------------State message define-----------------*/
#define EXTFUNC_LOAD_ERROR				16
#define DW9786_CHIP_ID					0x9786
#define AUTORD_RV_OK					0x0000
#define OK								0
#define NG								1


/*-----------------User message define-----------------*/
#define MAX_CHAR_LENGTH					100
#define round(fp)		(int)((fp) >= 0 ? (fp) + 0.5 : (fp) - 0.5)


/*-----------------Event message define-----------------*/
#ifdef DJI
#define LOOP_A							500
#define	LOOP_B							LOOP_A-1
#define WAIT_TIME						500 // for DJI Piezo
#else if
#define LOOP_A							200
#define	LOOP_B							LOOP_A-1
#define WAIT_TIME						100
#endif

#define FUNC_PASS						0
#define FUNC_FAIL						-1

#define EXECUTE_ERROR					-1
#define CHECKSUM_ERROR					4

/* [MOTORDEV PATCH 2026-05-29] User cancel via cancel-check hook */
#define DW9786_FW_CANCELLED				(-12)

#define MODE_ON							1
#define MODE_OF							0

#define OSC_48M							1
#define OSC_64M							2
#define OSC_72M							3

/*-----------------Gyro message define-----------------*/
#define GYRO_OFFSET_CAL_OK 				0
#define Y_AXIS_ABNORMAL_SIGNAL			1
#define X_AXIS_ABNORMAL_SIGNAL			2
#define Y_AXIS_NO_GYRO_SIGNAL			4
#define X_AXIS_NO_GYRO_SIGNAL			8

/*-----------------Hall message define-----------------*/
#define HALL_CAL_DONE_FAIL				0xFF

#define X_AXIS_HALL_CAL_PASS			0x1
#define X_AXIS_AMP_OFS_ERR				0x10
#define X_AXIS_RETRY_ERR				0x100
#define X_AXIS_DIRECTION_ERR			0x1000

#define Y_AXIS_HALL_CAL_PASS			0x2
#define Y_AXIS_AMP_OFS_ERR				0x20
#define Y_AXIS_RETRY_ERR				0x200
#define Y_AXIS_DIRECTION_ERR			0x2000

#define Z_AXIS_HALL_CAL_PASS			0x4
#define Z_AXIS_AMP_OFS_ERR				0x40
#define Z_AXIS_RETRY_ERR				0x400
#define Z_AXIS_DIRECTION_ERR			0x4000

#define AF_AXIS_HALL_CAL_PASS			0x4
#define AF_AXIS_AMP_OFS_ERR				0x40
#define AF_AXIS_RETRY_ERR				0x400
#define AF_AXIS_DIRECTION_ERR			0x4000

#define X_AXIS							0
#define Y_AXIS							1
#define Z_AXIS							2

/*-----------------Servo message define-----------------*/
#define SERVO_CAL_DONE_FAIL				0xFF
#define X_AXIS_SERVO_CAL_PASS			0x1
#define X_AXIS_SERVO_GAIN_MAX_ERR		0x10
#define X_AXIS_SERVO_GAIN_MIN_ERR		0x100

#define Y_AXIS_SERVO_CAL_PASS			0x2
#define Y_AXIS_SERVO_GAIN_MAX_ERR		0x20
#define Y_AXIS_SERVO_GAIN_MIN_ERR		0x200

#define Z_AXIS_SERVO_CAL_PASS			0x4
#define Z_AXIS_SERVO_GAIN_MAX_ERR		0x40
#define Z_AXIS_SERVO_GAIN_MIN_ERR		0x400

/*---------------Lens ofs message define----------------*/
#define LENS_OFS_CAL_DONE_FAIL			0xFF
#define X_AXIS_LENS_OFS_CAL_PASS		0x1
#define X_AXIS_LENS_OFS_MAX_ERR			0x100

#define Y_AXIS_LENS_OFS_CAL_PASS		0x2
#define Y_AXIS_LENS_OFS_MAX_ERR			0x200

#define Z_AXIS_LENS_OFS_CAL_PASS		0x4
#define Z_AXIS_LENS_OFS_MAX_ERR			0x400

/*--------------Gyro ofs message define---------------*/
#define GYRO_OFS_CAL_DONE_FAIL			0xFF
#define X_AXIS_GYRO_OFS_CAL_PASS		0x1
#define X_AXIS_GYRO_OFS_MAX_ERR			0x10

#define Y_AXIS_GYRO_OFS_CAL_PASS		0x2
#define Y_AXIS_GYRO_OFS_MAX_ERR			0x20

#define Z_AXIS_GYRO_OFS_CAL_PASS		0x4
#define Z_AXIS_GYRO_OFS_MAX_ERR			0x40

#define GYRO_RAW_DATA_ERR				0x8000


// ----- AF Control Bit -----------------------------
#define CTRL_10BIT								10
#define CTRL_12BIT								12
#define CTRL_14BIT								14
//---------------------------------------------------

/*-----------------Flash message define-----------------*/
#define FMC_PAGE						20
#define MCS_CODE						1
#define MCS_LUT							2
#define MCS_USER						3
#define MCS_START_ADDRESS				0x6000
//#define MCS_PKT_SIZE					64
#define MCS_SIZE_W						20480
#define MCS_CHECKSUM_SIZE				10240

// ----- FRA Board Setting ----------------
#define FRA_BOARD_SLVID		0x24
#define FRA_BOARD_INFO		0xAE
#define STATUS_LIMIT_CNT	100
#define TEST_DELAY_TIME		20
#define TEST_LIMIT_CNT		100
#define FRA_TEST_COMPLETE	0xDC

#define PLANT_FRA_X			0x10
#define PLANT_FRA_Y			0x30
#define PLANT_FRA_AF		0x50
#define OPEN_FRA_X			0x11
#define OPEN_FRA_Y			0x31
#define OPEN_FRA_AF			0x51


//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
#define CTRL_FREQ_5KHZ		0x00
#define CTRL_FREQ_10KHZ		0x01
#define CTRL_FREQ_15KHZ		0x0B

#define FRA_BOARD_INFO_NG		0x0100
#define MODE_ON_STS_CHK_ERR		0x0101
#define TEST_STS_CHK_ERR		0x0102

/*-----------------Struct register -----------------*/
#define COMP_POINT 						7
#define PIXEL_MIN						10

typedef struct
{
	short x_pos_margin;
	short x_neg_margin;
	short y_pos_margin;
	short y_neg_margin;
	short af_pos_margin;
	short af_neg_margin;

}stHallMargin;

typedef struct tag_croordinate
{
	double value[2][COMP_POINT];
}tCoordi;

typedef struct tag_Cross
{
	double MovePixel[COMP_POINT];
	short Coeffi[COMP_POINT];
	double zeroPixel[COMP_POINT];
	short slope[COMP_POINT];
}tCross;

typedef struct tag_Lin
{
	double zeroDist[2][COMP_POINT];
	double MeasureDist[COMP_POINT];
	double IdealDist[COMP_POINT];
	double TargetStep;				// tareget step : 512 code
	double TargetCmd[COMP_POINT];
	double StrokePerCmd;			// 1�ڵ�� distance
	double Coordinate[2][COMP_POINT];
	double NegStroke;				// negative distance 
	double PosStroke;				// positive distance
	double TargetStroke;
	double InterStroke;
	double IdealStroke[COMP_POINT];			// ideal distance
	double MeasureStroke[COMP_POINT];		// real measure distance
	short Coeffi[COMP_POINT];
	double Slope[COMP_POINT];
	short SlopeFixed[COMP_POINT];
	short PointNumber;
}tLin;

typedef struct
{
	double XonXmove[COMP_POINT];
	double YonXmove[COMP_POINT];
	double XonYmove[COMP_POINT];
	double YonYmove[COMP_POINT];
}stPixelCoordinate;

typedef struct tag_Posture
{
	double Up_POS[2];			//Up Postive X,Y Pixel 
	double Up_NEG[2];			//Up Negative X,Y Pixel 
	double REF_POS[2];			//REF Positive X,Y Pixel 
	double REF_NEG[2];			//REF Negative X,Y Pixel 
	double Down_POS[2];			//Down Positive X,Y Pixel 
	double Down_NEG[2];			//Down Negative X,Y Pixel 
}stPosture;

typedef struct
{
	short g_TargetAdc[4][512];
	short g_DIFF[2][512];
	short g_ADC_PER_MIC[2];
	int g_NG_Point;
}sHallAccuracy;

typedef struct 
{
	short x_p_cal[3];
	short x_n_cal[3];
	short y_p_cal[3];
	short y_n_cal[3];
	short af_p_cal[3];
	short af_n_cal[3];
} sCoilFlux;

typedef struct
{
	BYTE ois_slave_id;
	BYTE ois_mode;
	BYTE ois_control_cycle;
}sFraInterfaceModeSetting;

typedef struct
{
	BYTE ois_slave_id;
	BYTE ois_mode;
	BYTE ois_control_freq;

	int amplitude;
	int dc_bias_ofst;
	int test_point;
	int start_freq;
	int end_freq;
	int step_freq;
}sFRA_TestSetting;

typedef struct
{
	BYTE gain_margin_flag;
	double gain_margin;
	double gain_margin_freq;
	BYTE phase_margin_flag;
	double phase_margin;
	double phase_margin_freq;
}sFRA_Margin;

/************************************************************************/
/* Dongwoon anatech OIS �Լ� ����                                  */
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

// Extern function list
typedef int (*ptrfunc_I2CRead)(unsigned char SLVID, unsigned char addr_size, unsigned char *addr, unsigned char read_size, unsigned char *read_dat);	// SLVID: 7bit
typedef int (*ptrfunc_I2CWrite)(unsigned char SLVID, unsigned char write_size, unsigned char *write_dat);							// SLVID: 7bit
typedef int (*ptrfunc_OutputLog)(const char *string);

typedef struct _sDW978x_ExtFuncList
{
	ptrfunc_I2CRead pFunc_I2C_Read;
	ptrfunc_I2CWrite pFunc_I2C_Write;
	ptrfunc_OutputLog  pFunc_OutputLog;
} DW978x_ExtFuncList;

_EXPORT void dw9786_file_path(char *path);
_EXPORT int dw978x_extfuncinit(DW978x_ExtFuncList* pExtFuncList);
_EXPORT int dw9786_id_check(void);
_EXPORT int dw9786_auto_read_check(void);
_EXPORT int dw9786_fw_download(void);
_EXPORT void dw9786_module_init_set(void);
_EXPORT int dw9786_checksum_func(int type);
_EXPORT int dw9786_hall_calibration(int axis_cnt);
_EXPORT int dw9786_x_hall_calibration(void);
_EXPORT int dw9786_y_hall_calibration(void);
_EXPORT int dw9786_z_hall_calibration(void);
_EXPORT int dw9786_af_hall_calibration(void);
_EXPORT int dw9786_servo_loop_calibration(int axis_cnt);
_EXPORT int dw9786_lens_ofs_calibration(int axis_cnt);
_EXPORT int dw9786_gyro_ofs_calibration(int axis_cnt);
_EXPORT int dw9786_hall_margin_adjustment(stHallMargin *hall_epa);
_EXPORT int dw9786_sel_gyro_sensor(int sel_gyro);
_EXPORT void dw9786_chip_info(void);
_EXPORT void dw9786_fw_info(void);
_EXPORT int dw9786_module_cal_store(void);
_EXPORT int dw9786_set_cal_store(void);
_EXPORT int dw9786_module_cal_erase(void);
_EXPORT int dw9786_set_cal_erase(void);
_EXPORT int dw9786_load(void);
_EXPORT int dw9786_reset(void);
_EXPORT int dw9786_mcu_active(unsigned short mode);
_EXPORT int dw9786_chip_enable(unsigned short en);
_EXPORT int dw9786_user_protection(void);
_EXPORT int dw9786_servo_off(void);
_EXPORT int dw9786_servo_on(void);
_EXPORT int dw9786_ois_on(void);
_EXPORT int dw9786_3_axis_servo_off(void);
_EXPORT int dw9786_3_axis_servo_on(void);
_EXPORT int dw9786_3_axis_ois_on(void);
_EXPORT int OpenLoopAging(int loop_cnt, int ms_delay);
_EXPORT int dw9786_spi_master_mode(void);
_EXPORT int dw9786_spi_intercept_mode(void);
_EXPORT int dw9786_slave_id_change(unsigned char ois_id, unsigned char mem_id);
_EXPORT int dw9786_osc_red_mem_change(int OSC);
_EXPORT int dw9786_osc_param_change(int OSC);
_EXPORT int dw9786_read_osc_param(void);
_EXPORT int dw9786_read_week_info(void);
_EXPORT int dw9786_osc_param_change_80MHz(void);
_EXPORT unsigned short make_80MHZ_osc(u16 osc_72m);
_EXPORT int dw9786_osc_param_change_78MHz(void);
_EXPORT unsigned short make_78MHZ_osc(u16 osc_72m);
_EXPORT int dw9786_osc_param_change_75MHz(void);
_EXPORT unsigned short make_75MHZ_osc(u16 osc_72m);
_EXPORT int dw9786_ldo_change(int ldo_cnt);
_EXPORT void dw978x_ois_slvid(BYTE DW978x_SLVID_8BIT);
_EXPORT void dw978x_mem_slvid(BYTE dw978x_slvid_8BIT);
_EXPORT void set_af_ic_setting(BYTE AF_SLVID_8BIT, int ctrl_bit);
_EXPORT double get_pixel_semco_4point(double left, double right, double top, double bottom);
_EXPORT void dw9786_getpixel_coordinate(stPixelCoordinate *p_coor, short p_num);
_EXPORT void dw9786_crosstalk_compensation(void);
_EXPORT void dw9786_linearity_compensation(u16 xtarget_step, u16 ytarget_step);
_EXPORT void dw9786_fixed_linearity_compensation(unsigned short X_TargetStep, unsigned short Y_TargetStep, unsigned short X_TargetStroke, unsigned short Y_TargetStroke);
_EXPORT int dw9786_ois_coilflux_compensation(sCoilFlux* flux);
_EXPORT int dw9786_af_coilflux_compensation(sCoilFlux* flux);
_EXPORT void dw9786_ois_lin_cross_comp_enable(void);
_EXPORT void dw9786_ois_lin_cross_comp_disable(void);
_EXPORT void dw9786_af_lin_comp_enable(void);
_EXPORT void dw9786_af_lin_comp_disable(void);
_EXPORT unsigned int checksum_func(u16 *data, int leng);
_EXPORT unsigned int cal_checksum2_func(u32 checksum, u16 data);
_EXPORT int dw9786_cal_checksum(void);
_EXPORT int wait_check_register(u16 reg, u16 ref);
_EXPORT void dw9786_af_write_linearity_compensation(int *LinCoeff);
_EXPORT void dw9786_write_linearity_compensation(void);
_EXPORT void dw9786_write_crosstalk_compensation(void);
_EXPORT void m_delay(unsigned int delay);
_EXPORT void dw9786_flash_acess(int type);
_EXPORT void dw9786_block_size_set(int block_size);
_EXPORT int dw9786_x_hall_calibration(void);
_EXPORT int dw9786_y_hall_calibration(void);
_EXPORT int dw9786_z_hall_calibration(void);

_EXPORT void dw9786_x_hall_reg_info(void);
_EXPORT void dw9786_y_hall_reg_info(void);
_EXPORT void dw9786_af_hall_reg_info(void);
_EXPORT void dw9786_hall_margin_adj_reg_info(void);
_EXPORT void dw9786_lens_offs_reg_info(void);
_EXPORT void dw9786_servo_loop_gain_reg_info(void);

_EXPORT void dw9786_x_linearity_reg_info(void);
_EXPORT void dw9786_x_crosstalk_reg_info(void);
_EXPORT void dw9786_y_linearity_reg_info(void);
_EXPORT void dw9786_y_crosstalk_reg_info(void);
_EXPORT void dw9786_af_lin_cross_reg_info(void);

_EXPORT void dw9786_coil_flux_info(void);
_EXPORT void dw9786_x_ois_reg_info(void);
_EXPORT void dw9786_y_ois_reg_info(void);
_EXPORT void dw9786_gyro_imu_reg_info(void);
_EXPORT void dw9786_af_comp_reg_info(void);
_EXPORT void dw9786_cal_status_reg_info(void);
_EXPORT void dw9786_cal_chksum_reg_info(void);

_EXPORT void get_hall_accuracy_data(sHallAccuracy *getHallAccuracy);
_EXPORT int circle_motion_test (unsigned short RADIUS, float ACCURACY, unsigned short DEG_STEP, unsigned short WAIT_TIME0, unsigned short WAIT_TIME1, unsigned short WAIT_TIME2);
_EXPORT void set_ref_stroke(int ref_stroke, int ref_gyro_filter_out);

_EXPORT int FRA_Interface_Mode(sFraInterfaceModeSetting fra_setting);
_EXPORT void FRA_Test_Stop(void);
_EXPORT int Echo_FRA_Measurement(sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf);
_EXPORT int Echo_FRA_Measurement_SEMCO(sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf);
_EXPORT int Search_GM_PM(sFRA_Margin* fra_result, sFRA_TestSetting fra_setting, double *freq_buf, double *gain_buf, double *phase_buf);
_EXPORT int Echo_Board_WhoAmI(void);

_EXPORT void InputValLoad_9786_AF(double *ldmdata, int point, int range);
_EXPORT int CalcCompenPoint_9786_AF(int calpoint, int point);
_EXPORT void OutputCoeff_9786_AF(int *coeff);

_EXPORT void set_af_target(short target);
_EXPORT void af_target_10bit(short target);
_EXPORT void af_target_12bit(short target);
_EXPORT void af_target_14bit(short target);

// Apply different functions depending on the project
_EXPORT void dw9786_module_DJI_init_set(void);

/* ============================================================
 * [MOTORDEV PATCH 2026-05-29] FW update with external buffer + progress/cancel hook
 * 让上位机直接传 firmware buffer，跳过 vendor 内部 fopen + 解析；并在 erase / write
 * 循环关键点注入 progress 上报 + cancel 检查（hook 实体在 dw9786_bridge 注入）。
 * 与 hl9788n_fw_update_dma_with_buffer 模式完全对称。
 * ============================================================ */
typedef void (*dw9786_progress_cb_t)(int pct);
typedef int  (*dw9786_cancel_check_t)(void);  /* return non-zero to cancel */

_EXPORT void dw9786_set_progress_cb(dw9786_progress_cb_t cb);
_EXPORT void dw9786_set_cancel_check(dw9786_cancel_check_t cb);

/* word_count must equal MCS_SIZE_W (20480); fw_data points to host buffer. */
_EXPORT int  dw9786_fw_download_with_buffer(const u16 *fw_data, u32 word_count);

#ifdef __cplusplus
}
#endif

#undef	_EXPORT
#endif



