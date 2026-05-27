/**
  ******************************************************************************
  * File Name          : HL9788_API_REF.h
  * Description        : Main program header
  * DongWoon Anatech   :
  * Version            : 0.0
  ******************************************************************************
  * COPYRIGHT(c) 2020 DWANATECH
  * hl9788n Setup Program for Module Company
  * Revision History
  * 2026.01.29 SJ Cho // v0.0.0.0
  * 2026.02.12 SJ Cho // v0.0.0.1 Fixed an incorrect Ready/Done status check value in AF Hall Calibration.
  * 2026.02.19 SJ Cho // v0.0.0.2 Added Echo FRA measurement support.
						 Added user memory read/write APIs.
                         Added Added hl9788n_set_startup.
  * 2026.02.26 SJ Cho // v0.0.0.3 Modified LCC point from 7 to 21.
  * 2026.02.27 SJ Cho // v0.0.0.4 Changed the function name of the AF Linearity DLL..
  * 2026.03.04 Ryan   // v0.0.0.5 Change AF linearity compensation coefficient address
  * 2026.03.05 SJ Cho // v0.0.0.6 Added 7×7 Matrix OIS LCC compensation.
                      //          Added AF coil flux compensation function.
  * 2026.03.09 Ryan   // v0.0.0.7 Add hl9788n_fixed_af_write_lin_comp() function
  * 2026.03.11 Ryan   // v0.0.0.8 Modify 7×7 Matrix OIS LCC registers
  * 2026.03.12 SJ Cho // v0.0.0.9 Add posture compensation function
					  // 		  Add SPI Master mode
  * 2006.03.17 SJ Cho // v0.0.1.0 Added OSC change API (register only)
								  Added debug counter verification function
  * 2006.03.25 SJ Cho // v0.0.1.1 Added I2C Indirect Access API (for external device control via indirect mode) 
  ******************************************************************************
**/
#ifndef _HL9788N_OIS_H_
#define _HL9788N_OIS_H_

#ifndef _STDINT_H_
#include <stdint.h>
#endif

#ifdef HL9788N_OIS_EXPORTS
#define _EXPORT __declspec(dllexport)
#else
#define _EXPORT __declspec(dllimport)
#endif

typedef unsigned char	uint8_t;
typedef unsigned short	uint16_t;
typedef unsigned int	uint32_t;

#define LOG_FUNC_START()  log_tprintf(1, _T("[%s] Start"),  _T(__FUNCTION__))
/* [MOTORDEV PATCH 2026-05-27] __VA_ARGS__ → ##__VA_ARGS__ 以兼容 GCC/MinGW
 * 当 LOG_FUNC_INFO("literal") 不带可变参数调用时，标准 __VA_ARGS__ 空展开会
 * 在前一个 comma 后留下悬空 → "expected primary-expression before ')' token"。
 * GCC 扩展 ##__VA_ARGS__ 在空参数时吃掉前面的 comma，MSVC 自然容忍。 */
#define LOG_FUNC_INFO(fmt, ...) log_tprintf(1, _T("[%s] ") _T(fmt),  _T(__FUNCTION__), ##__VA_ARGS__)
#define LOG_FUNC_ERR(fmt, ...)  log_tprintf(1, _T("[%s][ERR] ") _T(fmt), _T(__FUNCTION__), ##__VA_ARGS__)
#define LOG_FUNC_END()    log_tprintf(1, _T("[%s] End"),  _T(__FUNCTION__))

#define SET_PT		                    RamWriteA(0x603E, 0x5959)
#define MODULE_PT	                    RamWriteA(0x603E, 0xA6A6)

#define FLASH_BASE                      0x10000
#define USER_START_OFFSET               0x2000
#define USER_SIZE_BYTES                 (56U * 1024U)
#define USER_END_OFFSET                 (USER_START_OFFSET + USER_SIZE_BYTES)
#define PAGE_SIZE                       1024U

/*-----------------State message define-----------------*/
#define EXTFUNC_LOAD_ERROR				16
#define HL9788N_CHIP_ID					0x9788
#define AUTORD_RV_OK					0x0000
#define OK								0
#define NG								1

/*-----------------User message define-----------------*/
#define MAX_CHAR_LENGTH					100
//#define MAX_CHAR_LENGTH					20480

#define round(fp)		(int)((fp) >= 0 ? (fp) + 0.5 : (fp) - 0.5)

/*-----------------Event message define-----------------*/
#ifdef DJI
#define LOOP_A							500
#define	LOOP_B							LOOP_A-1
#define WAIT_TIME						500 

#else if
#define LOOP_A							200
#define	LOOP_B							LOOP_A-1
#define WAIT_TIME						100
#endif

#define FUNC_PASS						0
#define FUNC_FAIL						-1

#define EXECUTE_ERROR					-1
#define CHECKSUM_ERROR					4

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

#define AF_AXIS_HALL_CAL_PASS			0x4
#define AF_AXIS_AMP_OFS_ERR				0x40
#define AF_AXIS_RETRY_ERR				0x400
#define AF_AXIS_DIRECTION_ERR			0x4000

#define ZOOM_AXIS_HALL_CAL_PASS			0x8
#define ZOOM_AXIS_AMP_OFS_ERR			0x80
#define ZOOM_AXIS_RETRY_ERR				0x800
#define ZOOM_AXIS_DIRECTION_ERR			0x8000

#define X_AXIS							0
#define Y_AXIS							1
#define AF_AXIS							2
#define ZOOM_AXIS						3

/*-----------------Servo message define-----------------*/
#define SERVO_CAL_DONE_FAIL				0xFF
#define X_AXIS_SERVO_CAL_PASS			0x1
#define X_AXIS_SERVO_GAIN_MAX_ERR		0x10
#define X_AXIS_SERVO_GAIN_MIN_ERR		0x100

#define Y_AXIS_SERVO_CAL_PASS			0x2
#define Y_AXIS_SERVO_GAIN_MAX_ERR		0x20
#define Y_AXIS_SERVO_GAIN_MIN_ERR		0x200

#define AF_AXIS_SERVO_CAL_PASS			0x4
#define AF_AXIS_SERVO_GAIN_MAX_ERR		0x40
#define AF_AXIS_SERVO_GAIN_MIN_ERR		0x400

/*---------------Lens ofs message define----------------*/
#define LENS_OFS_CAL_DONE_FAIL			0xFF
#define X_AXIS_LENS_OFS_CAL_PASS		0x1
#define X_AXIS_LENS_OFS_MAX_ERR			0x100

#define Y_AXIS_LENS_OFS_CAL_PASS		0x2
#define Y_AXIS_LENS_OFS_MAX_ERR			0x200

#define AF_AXIS_LENS_OFS_CAL_PASS		0x4
#define AF_AXIS_LENS_OFS_MAX_ERR		0x400

/*--------------Gyro ofs message define---------------*/
#define GYRO_OFS_CAL_DONE_FAIL			0xFF
#define X_AXIS_GYRO_OFS_CAL_PASS		0x1
#define X_AXIS_GYRO_OFS_MAX_ERR			0x10

#define Y_AXIS_GYRO_OFS_CAL_PASS		0x2
#define Y_AXIS_GYRO_OFS_MAX_ERR			0x20

#define AF_AXIS_GYRO_OFS_CAL_PASS		0x4
#define AF_AXIS_GYRO_OFS_MAX_ERR			0x40

#define GYRO_RAW_DATA_ERR				0x8000

// ----- AF Control Bit -----------------------------
#define CTRL_10BIT						10
#define CTRL_12BIT						12
#define CTRL_14BIT						14
//-------------------------------------------------

/*-----------------Flash message define-----------------*/
#define FMC_PAGE						20
#define MCS_CODE						1
#define MCS_LUT							2
#define MCS_USER						3
#define MCS_START_ADDRESS				0x6000

#define MCS_PKT_SIZE					64
#define FW_SIZE							0x10000     // 64KB
#define FW_HALF_SIZE					0x8000      // 32KB
//#define BLOCK_SIZE						1024        // 1KB  

#define MCS_SIZE_W						20480
#define MCS_CHECKSUM_SIZE				10240

// ----- FRA Board Setting ----------------
#define FRA_BOARD_INFO		            0xAE
#define STATUS_LIMIT_CNT	            100
#define TEST_DELAY_TIME		            20
#define TEST_LIMIT_CNT		            100
#define FRA_TEST_COMPLETE	            0xDC

//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
#define PLANT_FRA_X			            0x10
#define PLANT_FRA_Y			            0x30
#define PLANT_FRA_AF		            0x50
#define PLANT_FRA_ZOOM                  0x70   /* HL9788 only */
#define OPEN_FRA_X			            0x11
#define OPEN_FRA_Y			            0x31
#define OPEN_FRA_AF			            0x51
#define OPEN_FRA_ZOOM                   0x71   /* HL9788 only */

/* Legacy DW98xx */
#define IC_DW9824                       0x04
#define IC_DW9825                       0x05
#define IC_DW9827                       0x07
#define IC_DW9828                       0x08
#define IC_DW9829                       0x09

/* DW978x Series */                 
#define IC_DW9781                       0x31
#define IC_DW9786                       0x33
#define IC_DW9784                       0x34
#define IC_DW9785                       0x35
/* DW984x Series */                 
#define IC_DW9841                       0x32

/* New Added */                 
#define IC_HL9788                       0x38

#define CTRL_FREQ_5KHZ		            0x00
#define CTRL_FREQ_10KHZ		            0x01
#define CTRL_FREQ_15KHZ		            0x0B

#define FRA_BOARD_INFO_NG		        0x0100
#define MODE_ON_STS_CHK_ERR		        0x0101
#define TEST_STS_CHK_ERR		        0x0102

// ============================================================================
// Status Bit Mask Definitions
// ============================================================================
#define CS_OK_TM                        (1 << 15)
#define CS_OK_UM                        (1 << 14)
#define CS_OK_LUT                       (1 << 13)
#define CS_OK_INS                       (1 << 12)

#define RV_ERR_TM                       (1 << 11)
#define RV_ERR_UM                       (1 << 10)
#define RV_ERR_LUT                      (1 <<  9)
#define RV_ERR_INS                      (1 <<  8)

#define AUTOLOAD_DONE                   (1 <<  4)
#define FMC_BUSY                        (1 <<  0)

// Error flag definitions (for return values)
#define ERR_TM_CS                       (1 << 0)
#define ERR_UM_CS                       (1 << 1)
#define ERR_LUT_CS                      (1 << 2)
#define ERR_INS_CS                      (1 << 3)

#define ERR_TM_RV                       (1 << 4)
#define ERR_UM_RV                       (1 << 5)
#define ERR_LUT_RV                      (1 << 6)
#define ERR_INS_RV                      (1 << 7)

// Checksum selection
#define CS_SEL_MAIN                     0x00
#define CS_SEL_LUT                      0x01
#define CS_SEL_INST                     0x03
#define CS_SEL_UM                       0x05
#define CS_SEL_TM                       0x09
#define CS_SEL_M                        0x10

// Mode
#define MODE_NVR_CONF                   (1 << 11)
#define MODE_RDN_1                      (1 << 10)
#define MODE_RDN_0                      (1 <<  9)
#define MODE_NVR                        (1 <<  8)
#define MODE_MAIN                       0x00

// ============================================================================
// Memory Size Definitions
// ============================================================================
#define FW_SIZE                         0x10000     // 64KB
#define FW_HALF_SIZE                    0x8000      // 32KB
#define LUT_SIZE                        0x2000      // 8KB
#define BLOCK_SIZE                      1024        // 1KB
#define BYTE_SIZE                       8

#define RDN_SIZE                        1024
#define NVR_TRIM_SIZE                   92
#define NVR_UM_SIZE                     68

#define DMA_CONTROL_REG                 0xA800
#define DMA_STATUS_REG                  0xA808
#define DMA_ENABLE_FLAG                 0x0001

// ============================================================================
// Firmware operation error definitions
// ============================================================================
#define HL9788N_FW_OK                   (0)
#define HL9788N_FW_TIMEOUT              (-11)
#define HL9788N_FW_CANCELLED            (-12)  // [MOTORDEV PATCH] user cancel via hook

// ============================================================================
// Index definitions for NVM data array
// ============================================================================
#define IDX_MAIN_SLV_ID                  4
#define IDX_SUB_SLV_ID                   7
#define IDX_DATA_SIZE                    8
#define IDX_OSC                         11
#define IDX_STATUP                      12
#define IDX_CHKSUM                      16   // Index of the checksum in NVM data
#define NVM_DATA_COUNT                  17   // Total number of NVM data entries
#define NVM_START_ADDR                  0x0400 // Start address of NVM

#define I2C_SEL_MAIN                    0
#define I2C_SEL_SUB                     1

//--------------------------------------------------
// OSC Frequency Control (USER Protection)
//--------------------------------------------------
#define OSC_FREQ_64MHZ                  (0x00)  // 2'b00 : 64 MHz
#define OSC_FREQ_72MHZ                  (0x01)  // 2'b01 : 72 MHz
#define OSC_FREQ_96MHZ                  (0x02)  // 2'b10 : 96 MHz (default)
#define OSC_FREQ_128MHZ                 (0x03)  // 2'b11 : 128 MHz

/*-----------------Struct register -----------------*/
#define COMP_POINT 						21
#define PIXEL_MIN						10

/* ---------------- AF Linearity ---------------- */
#define INPUT_NUM			            83	// max. 83 points
//#define Section_NUM			            32
#define Poly_Order			            5
#define Poly_Data_Length	            8

#define GET_POSITION_COUNT	            3

//0x10: plant X, 0x11: Open X, 0x30: plant Y, 0x31: Open Y, 0x50: plant Y, 0x51: Open Y
#define CTRL_FREQ_5KHZ		            0x00
#define CTRL_FREQ_10KHZ		            0x01
#define CTRL_FREQ_15KHZ		            0x0B

typedef enum
{
    START_ACTIVE = 0,
    START_STANDBY,
    START_SLEEP,
    START_MAX
} startup_t;

typedef struct
{
    double x_pos_margin;
    double x_neg_margin;
    double y_pos_margin;
    double y_neg_margin;
    double af_pos_margin;
    double af_neg_margin;
} stHallMargin;


typedef struct
{
	double real_pos[COMP_POINT];      // measured real position
	double rel_pos[COMP_POINT];       // relative position from center
	int16_t coeff[COMP_POINT];         // crosstalk compensation coefficients
	int16_t slope[COMP_POINT];         // slope

} stCrossComp;

typedef struct
{
	double target_step;                 // LCC step size
	short  num_points;                  // number of LCC points

	double rel_dist[2][COMP_POINT];     // distance from reference (X,Y)
	double meas_dist[COMP_POINT];       // measured distance between points
	double target_cmd[COMP_POINT];      // target position commands
	double stroke_per_step;             // stroke per one step

	double coord[2][COMP_POINT];        // coordinates (X,Y)

	double neg_stroke;                  // negative stroke
	double pos_stroke;                  // positive stroke
	double target_stroke;               // target stroke
	double interp_stroke;               // interpolation stroke

	double meas_stroke[COMP_POINT];     // measured stroke
	double ideal_stroke[COMP_POINT];    // ideal stroke

	double slope[COMP_POINT];
	int16_t fixed_slope[COMP_POINT];
	int16_t coeff[COMP_POINT];           // LCC coefficients

} stLinearity;

typedef struct
{
    double posX_onMoveX[COMP_POINT];
    double posY_onMoveX[COMP_POINT];
    double posX_onMoveY[COMP_POINT];
    double posY_onMoveY[COMP_POINT];
} stPixelCoord;

/* =========================================================
 * Matrix Linearity/Crosstalk Compensation (7x7)
 *  - Ported from DW9786 reference, kept algorithm structure.
 * ========================================================= */
typedef struct Matrix_Linear {
    double ref_stroke[2][7];             // Reference(target) stroke
    int16_t linearity_coeffi[2][42];     // linearity coefficient X / Y
    int16_t linearity_slopefixed[2][42]; // linearity slope X/Y (x1024)
    int16_t crosstalk_slopefixed[2][42]; // crosstalk slope X/Y (x32768)

    double first_coeffi[2][49];          // first compensation coefficient
    double first_raw_coordi[2][49];      // first pixel coordinate
    double second_coeffi[2][49];         // second compensation coefficient
    double second_raw_coordi[2][49];     // second pixel coordinate

    double moveStep[2];
    int    PointNumber;                 // Default : 7
    int    mode;                        // 0 : 1st calibration , 1 : 2nd calibration ,  2 : verify
} tMatrix_LC;

typedef struct
{
    double pixel[2][49];                // 7x7 fixed
} stMatrixPixelCoordinate;


typedef struct
{
    double up_pos[2];     // Up positive pixel [X, Y]
    double up_neg[2];     // Up negative pixel [X, Y]
    double ref_pos[2];    // Reference positive pixel [X, Y]
    double ref_neg[2];    // Reference negative pixel [X, Y]
    double down_pos[2];   // Down positive pixel [X, Y]
    double down_neg[2];   // Down negative pixel [X, Y]
} stPosture;

typedef struct
{
    short target_adc[4][512];   // [0]X tgt, [1]X hall, [2]Y tgt, [3]Y hall
    short diff_adc[2][512];     // [X/Y][idx]
    short adc_per_um[2];        // ADC code per micron [X/Y]
    int   ng_cnt;               // Number of NG points
} stHallAccuracy;

typedef struct
{
    /* X axis */
    int16_t x_top_base;         // 0x80D4
    int16_t x_top_positive;     // 0x80D6
    int16_t x_top_negative;     // 0x80D8

    int16_t x_bot_base;         // 0x80DA
    int16_t x_bot_positive;     // 0x80DC
    int16_t x_bot_negative;     // 0x80DE

    int16_t x_cur_limit;        // 0x80D2

    /* Y axis */
    int16_t y_top_base;         // 0x81D4
    int16_t y_top_positive;     // 0x81D6
    int16_t y_top_negative;     // 0x81D8

    int16_t y_bot_base;         // 0x81DA
    int16_t y_bot_positive;     // 0x81DC
    int16_t y_bot_negative;     // 0x81DE

    int16_t y_cur_limit;        // 0x81D2

} stOisCoilFlux;

typedef struct
{
    int16_t af_top_base;         // 0x82D4
    int16_t af_top_positive;     // 0x82D6
    int16_t af_top_negative;     // 0x82D8

    int16_t af_bot_base;         // 0x82DA
    int16_t af_bot_positive;     // 0x82DC
    int16_t af_bot_negative;     // 0x82DE

    int16_t af_cur_limit;        // 0x82D2 (pattern-based)
} stAFCoilFlux;

/* =========================================================
 * FRA Test Setting
 * ========================================================= */
typedef struct
{
    int test_point;

    int start_freq;
    int end_freq;

    int amplitude;
    int dc_bias_ofst;

    int x_target;
    int y_target;
    int af_target;

    int step_freq;

    uint8_t ois_slave_id;
    uint8_t ois_mode;
    uint8_t ois_control_freq;

} sFRA_TestSetting;

/* =========================================================
 * FRA Margin Result
 * ========================================================= */
typedef struct
{
    uint8_t gain_margin_flag;
    float   gain_margin;
    float   gain_margin_freq;

    uint8_t phase_margin_flag;
    float   phase_margin;
    float   phase_margin_freq;

} sFRA_Margin;

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

typedef struct _sHL978x_ExtFuncList
{
	ptrfunc_I2CRead pFunc_I2C_Read;
	ptrfunc_I2CWrite pFunc_I2C_Write;
	ptrfunc_OutputLog  pFunc_OutputLog;
} HL978x_ExtFuncList;

/* --------------------------------------------------------------------------
 * Path / External function init
 * -------------------------------------------------------------------------- */
_EXPORT void hl9788n_file_path(char *path);
_EXPORT int  hl978x_extfuncinit(HL978x_ExtFuncList *pExtFuncList);

/* --------------------------------------------------------------------------
 * Basic / Info
 * -------------------------------------------------------------------------- */
_EXPORT int  hl9788n_id_check(void);
_EXPORT void hl9788n_fw_info(void);
_EXPORT uint16_t hl9788n_rv_status(void);

/* --------------------------------------------------------------------------
 * Calibration store / erase
 * -------------------------------------------------------------------------- */
_EXPORT int  hl9788n_module_cal_store(void);
_EXPORT int  hl9788n_set_cal_store(void);
_EXPORT int  hl9788n_module_cal_erase(void);
_EXPORT int  hl9788n_set_cal_erase(void);

/* --------------------------------------------------------------------------
 * Slave ID / Utility
 * -------------------------------------------------------------------------- */
_EXPORT void hl978x_ois_slvid(BYTE hl978xn_slvid_8BIT);
_EXPORT void echo_board_slvid(BYTE board_slvid_8BIT);
_EXPORT int  wait_check_register(uint16_t reg, uint16_t ref);
_EXPORT void m_delay(unsigned int delay);

/* --------------------------------------------------------------------------
 * PT control
 * -------------------------------------------------------------------------- */
_EXPORT void hl9788n_user_pt_off(void);
_EXPORT void hl9788n_user_pt_on(void);
_EXPORT void hl9788n_all_pt_off(void);
_EXPORT void hl9788n_all_pt_on(void);

_EXPORT void hl9788n_lut_pt_on(void);
_EXPORT void hl9788n_lut_pt_off(void);

_EXPORT void hl9788n_nvm_pt_on(void);
_EXPORT void hl9788n_nvm_pt_off(void);

_EXPORT void hl9788n_inst_pt_on(void);
_EXPORT void hl9788n_inst_pt_off(void);

_EXPORT void hl9788n_check_pt(void);
_EXPORT void hl9788n_set_pt_on(void);

/* --------------------------------------------------------------------------
 * MCU control
 * -------------------------------------------------------------------------- */
_EXPORT void hl9788n_ic_reset(void);
_EXPORT void hl9788n_ois_reset(void);

_EXPORT void hl9788n_mcu_on(void);
_EXPORT void hl9788n_mcu_off(void);
_EXPORT void hl9788n_mcu_on_forced(void);

_EXPORT void hl9788n_chip_enable(void);
_EXPORT void hl9788n_chip_disable(void);

_EXPORT int  hl9788n_fw_update_dma(void);

/* ----------------------------------------------------------------------------
 * [MOTORDEV PATCH 2026-05-27] FW update with external buffer + progress/cancel hook
 *
 * Sibling function of hl9788n_fw_update_dma(). Accepts an already-parsed
 * uint16_t buffer of length FW_SIZE/2 = 32768 words (64KB firmware) instead of
 * reading from FW_File_Path. Allows the MotorDev host application to skip
 * vendor-side file IO and supply the firmware directly.
 *
 * Progress / cancel hooks (set via hl9788n_set_progress_cb / _cancel_check)
 * are invoked at key points inside this function. They are NULL-safe; when
 * unset the function behaves identically to a normal long-running call.
 *
 * Returns FUNC_PASS on success, EXECUTE_ERROR on DMA/verify failure,
 * HL9788N_FW_CANCELLED if the cancel hook returns non-zero, or FUNC_FAIL
 * if final verification did not pass.
 *
 * NOTE: When upgrading vendor source, re-apply this addition or call the
 * original hl9788n_fw_update_dma() with a temporary hex file as fallback.
 * -------------------------------------------------------------------------- */
typedef void (*hl9788n_progress_cb_t)(int pct);
typedef int  (*hl9788n_cancel_check_t)(void);  /* return non-zero to cancel */

_EXPORT void hl9788n_set_progress_cb(hl9788n_progress_cb_t cb);
_EXPORT void hl9788n_set_cancel_check(hl9788n_cancel_check_t cb);

_EXPORT int  hl9788n_fw_update_dma_with_buffer(const uint16_t *fw_data,
                                                uint32_t word_count);

/* --------------------------------------------------------------------------
 * Hall accuracy test (your newly added API)
 * -------------------------------------------------------------------------- */
_EXPORT void get_hall_accuracy_data(stHallAccuracy *pst_hall_acc);

_EXPORT int  hall_accuracy_test(unsigned short radius_um,
                                float accuracy_um,
                                unsigned short deg_step,
                                unsigned short wait_t0,
                                unsigned short wait_t1,
                                unsigned short wait_t2);

_EXPORT void set_ref_stroke(int stroke, int gyro_result);

/* --------------------------------------------------------------------------
 * AF target write
 * -------------------------------------------------------------------------- */
_EXPORT void set_af_target(short target);
_EXPORT void af_target_10bit(short target);
_EXPORT void af_target_12bit(short target);
_EXPORT void af_target_14bit(short target);

/* --------------------------------------------------------------------------
 * Servo / OIS
 * -------------------------------------------------------------------------- */
_EXPORT int hl9788n_servo_on(void);
_EXPORT int hl9788n_servo_off(void);
_EXPORT int hl9788n_ois_on(void);

/* --------------------------------------------------------------------------
 * Calibration
 * -------------------------------------------------------------------------- */
_EXPORT int  hl9788n_hall_cal(int num_channels);
_EXPORT int  hl9788n_x_hall_cal(void);
_EXPORT int  hl9788n_y_hall_cal(void);
_EXPORT int  hl9788n_af_hall_cal(void);
_EXPORT int  hl9788n_zoom_hall_cal(void);

_EXPORT void hl9788n_x_hall_cal_info(void);
_EXPORT void hl9788n_y_hall_cal_info(void);
_EXPORT void hl9788n_af_hall_cal_info(void);

_EXPORT int  hl9788n_servo_cal(int num_channels);
_EXPORT int  hl9788n_lens_ofs_cal(int num_channels);
_EXPORT int  hl9788n_gyro_ofs_cal(int num_channels);

/* --------------------------------------------------------------------------
 * Margin / sensor selection
 * (NOTE: hl9788n_select_gyro()는 이 cpp에 없음)
 * -------------------------------------------------------------------------- */
_EXPORT int hl9788n_xy_hall_margin_adjust(stHallMargin *hall_epa);
_EXPORT int hl9788n_af_hall_margin_adjust(stHallMargin *hall_epa);

/* --------------------------------------------------------------------------
 * Pixel / Compensation
 * -------------------------------------------------------------------------- */
_EXPORT void hl9788n_get_pixel_coord(stPixelCoord *p_coor, short p_num);

/* =========================================================
 * Matrix LCC (7x7) APIs
 * ========================================================= */
_EXPORT void hl9788n_mat_get_coord_1st(stMatrixPixelCoordinate *p_coor, short p_num);
_EXPORT void hl9788n_mat_get_coord_2nd(stMatrixPixelCoordinate *p_coor);

_EXPORT void hl9788n_mat_comp_1st(uint16_t xtarget_step, uint16_t ytarget_step);
_EXPORT void hl9788n_mat_comp_2nd(uint16_t xtarget_step, uint16_t ytarget_step);

_EXPORT void hl9788n_mat_write_comp(void);

_EXPORT void hl9788n_mat_comp_disable(void);
_EXPORT void hl9788n_mat_comp_enable(void);


_EXPORT void hl9788n_af_write_lin_comp(int *lin_coeff);
_EXPORT void hl9788n_write_ois_lin_comp(void);
_EXPORT void hl9788n_write_ois_cross_comp(void);
_EXPORT void hl9788n_ois_cross_comp(void);
_EXPORT void hl9788n_ois_lin_comp(uint16_t x_target_step, uint16_t y_target_step);
_EXPORT void hl9788n_fixed_ois_lin_comp(uint16_t x_target_step, uint16_t y_target_step,
	uint16_t x_target_stroke, uint16_t y_target_stroke);
_EXPORT int hl9788n_ois_coilflux_comp(const stOisCoilFlux *pst_ois_flux);
_EXPORT int hl9788n_af_coilflux_comp(const stAFCoilFlux *pst_af_flux);
/* --------------------------------------------------------------------------
 * Enable / Disable
 * -------------------------------------------------------------------------- */
_EXPORT void hl9788n_ois_lin_cross_enable(void);
_EXPORT void hl9788n_ois_lin_cross_disable(void);
_EXPORT void hl9788n_af_lin_enable(void);
_EXPORT void hl9788n_af_lin_disable(void);

/* --------------------------------------------------------------------------
 * HW config
 * -------------------------------------------------------------------------- */
_EXPORT int  hl9788n_change_slave_id(uint8_t new_slave, uint16_t i2c_sel);
//_EXPORT int  hl9788n_change_osc_flash(uint8_t osc_sel);
_EXPORT int  hl9788n_change_osc_reg_only(uint8_t osc_sel);
_EXPORT int  hl9788n_set_half_size(void);
_EXPORT int  hl9788n_set_startup(startup_t startup_sel);

/* --------------------------------------------------------------------------
 * AF Linearity Compensator calculation
 * -------------------------------------------------------------------------- */
_EXPORT double Round(double input);
_EXPORT void hl9788n_Curve_Fitting_sec(void);
_EXPORT void hl9788n_InputValLoad(int step, int range, double *target, double *ldmdata, int bit);
_EXPORT void hl9788n_OutputCoeff(int *coeff);
_EXPORT void hl9788n_InputValLoadFixedAF(double *ldmdata, int point, int range, double sens);
_EXPORT int hl9788n_CalcCompenPointFixedAF(int calpoint);
_EXPORT void hl9788n_OutputCoeffFixedAF(int *coeff);
_EXPORT void hl9788n_fixed_af_write_lin_comp(int *lin_coeff);

_EXPORT void hl9788n_get_posture_pixel(const stPosture *pst_posture);

_EXPORT void hl9788n_posture_comp_enable(void);
_EXPORT void hl9788n_posture_comp_disable(void);
_EXPORT void hl9788n_posture_reg_info(void);
_EXPORT int  hl9788n_posture_comp(unsigned short gyro_top_bottom, int posture_spec);

_EXPORT int hl9788n_user_mem_write(uint16_t start_addr, uint8_t *data, uint32_t data_size);
_EXPORT int hl9788n_user_mem_read(uint16_t start_addr, uint8_t *data, uint32_t data_size);

_EXPORT int hl9788n_spi_master_mode(void);
_EXPORT int hl9788n_spi_intercept_mode(void);


/* =========================================================
 * FRA Measurement API
 * ========================================================= */

/* Single frequency sweep (Host controlled) */
_EXPORT int hl9788n_fra_run_single_point(
                                const sFRA_TestSetting *setting,
                                float *freq_buf,
                                float *gain_buf,
                                float *phase_buf);

/* Board internal sweep */
_EXPORT int hl9788n_fra_run_fullscan_pro(
                                const sFRA_TestSetting *setting,
                                float *freq_buf,
                                float *gain_buf,
                                float *phase_buf);

/* Gain Margin / Phase Margin calculation */
_EXPORT int hl9788n_fra_calc_gm_pm(
                                sFRA_Margin *result,
                                const sFRA_TestSetting *setting,
                                const float *freq_buf,
                                const float *gain_buf,
                                const float *phase_buf);

/* Board WhoAmI */
_EXPORT int hl9788n_fra_board_whoami(void);

/* Debug Counter Check */
_EXPORT int hl9788n_debug_count(void);

/* HL9788N I2C Indirect Access API (for external device control via indirect mode) */
_EXPORT int hl9788n_i2c_indirect_mode(unsigned char u08_device_slvid);

_EXPORT int hl9788n_i2c_indirect_single_write(unsigned char u08_addr, unsigned char u08_data);
_EXPORT int hl9788n_i2c_indirect_single_read(unsigned char u08_addr, unsigned char *pu08_data);

_EXPORT int hl9788n_i2c_indirect_double_write(unsigned char u08_addr, unsigned short u16_data);
_EXPORT int hl9788n_i2c_indirect_double_read(unsigned char u08_addr, unsigned short *pu16_data);

_EXPORT int hl9788n_i2c_indirect_4byte_write(unsigned char u08_addr, unsigned char *pu08_data);
_EXPORT int hl9788n_i2c_indirect_4byte_read(unsigned char u08_addr, unsigned char *pu08_data);

#ifdef __cplusplus
}
#endif

#undef	_EXPORT
#endif



