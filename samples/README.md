# 固件烧录样本文件

供 FwFlashTab UI/解析路径测试使用，**非真实可烧录固件**——仅用于验证文件选择、解析、信息显示和 stub 烧录流程。

## 文件清单

| 文件 | 大小 | 用途 | 期望解析结果 |
|---|---|---|---|
| `fw_sample_simple.bin` | 256 字节 | 简单 .bin 路径 | 大小 256；格式 Binary；CRC32 `0x29058C73` |
| `fw_sample_simple.hex` | 733 字节（文本）| 简单 .hex 路径，单段 | 格式 Intel HEX；段数 1；地址 `0x00000000-0x000000FF`；有效 256；CRC32 `0x29058C73`（与 .bin 一致） |
| `fw_sample_multiseg.hex` | 148 字节（文本）| 多段 .hex，验证段合并 + 0xFF 填充 | 段数 3（`0x0000` / `0x0100` / `0x0200` 各 16 字节）；合并后 528 字节；有效 48；CRC32 `0xDDA29EA6` |

## 验证要点

选 `fw_sample_simple.bin` 后文件信息卡片应显示：
- 文件大小 `256 字节 (0.2 KB)`
- 文件格式 `Binary`
- CRC32 `0x29058C73`

选 `fw_sample_simple.hex` 后：
- 文件格式 `Intel HEX`
- CRC32 应**等于** .bin 的 CRC32（验证两路径解析后的字节流一致）
- 段数 `1`，地址范围 `0x00000000 - 0x000000FF`，有效字节 `256`

选 `fw_sample_multiseg.hex` 后：
- 段数 `3`
- 地址范围 `0x00000000 - 0x0000020F`
- 有效字节 `48`（仅三段实际数据，0xFF 填充不计）

## 重新生成

```bash
python generate_samples.py
```

CRC32 用标准 IEEE 802.3 多项式 `0xEDB88320`，与 `FirmwareParser::computeCrc32` 一致。
