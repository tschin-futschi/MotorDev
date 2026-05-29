// =============================================================================
// @file    dw9786_symbol_rename.h
// @brief   MotorDev PATCH: rename all dw9786 vendor internal symbols
//
// HL9788N 与 DW9786 vendor 来自同一厂商，共享大量同名全局符号（函数 + 数据）。
// 静态链接到同一 exe 时这些符号冲突，原方案用 link flag --allow-multiple-definition
// 让 link 通过，但实际运行时 dw9786 vendor 各文件对 WriteI2CDev 等全局的引用在
// link 合并后行为不一致（dw9786 内部读到 NULL 而 hl9788n 内部能用），导致 SEGV。
//
// 本头文件通过预处理器 #define 把 dw9786 vendor 内部冲突符号全部 rename 为
// `dw9786_v_*` 前缀，使两个 vendor 完全独立，去除 link 冲突，可移除
// --allow-multiple-definition。
//
// 使用：
//   - vendor/dw9786/stdafx.h 顶部 #include 它（vendor TU 编译时生效）
//   - dw9786_vendor_include.h 顶部 #include 它（bridge/strategy 编译时一致看到）
// =============================================================================
#pragma once

// --- Data globals ---
#define _SLV_OIS_              dw9786_v_SLV_OIS_
#define _SLV_AF_               dw9786_v_SLV_AF_
#define WriteI2CDev            dw9786_v_WriteI2CDev
#define ReadI2CDev             dw9786_v_ReadI2CDev
#define OutputLog              dw9786_v_OutputLog
#define af_control_bit         dw9786_v_af_control_bit
#define g_stExtFnList          dw9786_v_g_stExtFnList
#define Cross                  dw9786_v_Cross
#define PostureCoor            dw9786_v_PostureCoor
#define CoeffiX                dw9786_v_CoeffiX
#define CoeffiY                dw9786_v_CoeffiY
#define FW_File_Path           dw9786_v_FW_File_Path
#define g_fRealTargetScale     dw9786_v_g_fRealTargetScale
#define target_interval        dw9786_v_target_interval
#define g_nTargetPoint         dw9786_v_g_nTargetPoint

// --- Functions ---
#define RamWriteA              dw9786_v_RamWriteA
#define RamReadA               dw9786_v_RamReadA
#define CntWrt                 dw9786_v_CntWrt
#define CnRd                   dw9786_v_CnRd
#define ByteWriteA             dw9786_v_ByteWriteA
#define ByteReadA              dw9786_v_ByteReadA
#define UserByteReadA          dw9786_v_UserByteReadA
#define UserByteWriteA         dw9786_v_UserByteWriteA
#define i2c_read_byte          dw9786_v_i2c_read_byte
#define i2c_write_byte         dw9786_v_i2c_write_byte
#define i2c_block_read         dw9786_v_i2c_block_read
#define i2c_block_write        dw9786_v_i2c_block_write
#define log_tprintf            dw9786_v_log_tprintf
#define H2D                    dw9786_v_H2D
#define D2H                    dw9786_v_D2H
#define m_delay                dw9786_v_m_delay
#define wait_check_register    dw9786_v_wait_check_register
