# =============================================================================
# inspect_aw86100_dll.ps1
#
# Dump exports and dependent DLLs of the real vendor AW86100.dll, to diagnose
# "DLL missing export symbol" failures from QLibrary::resolve() in
#   src/services/flashstrategies/aw_sdk_strategy.cpp
#
# Usage (run on the lab machine where the real DLL is available):
#
#   1) Put the real AW86100.dll at:
#        D:\echo.qfq\26_Qt\MotorDev\ThirdParty\FlashLoad\AW86100\x64\AW86100.dll
#      OR pass -DllPath to point at any location.
#
#   2) Open PowerShell, cd to the folder containing this script, and run:
#        powershell -ExecutionPolicy Bypass -File .\inspect_aw86100_dll.ps1
#      or with explicit path:
#        powershell -ExecutionPolicy Bypass -File .\inspect_aw86100_dll.ps1 -DllPath 'C:\path\to\AW86100.dll'
#
#   3) Output is printed to the console AND saved to aw86100_inspection.txt
#      next to the script. Paste the .txt content back to Claude Code.
#
# Self-contained: no objdump / dumpbin needed; uses pure PowerShell + .NET
# to parse the PE Export/Import directories.
# =============================================================================

param(
    [string]$DllPath = (Join-Path $PSScriptRoot 'ThirdParty\FlashLoad\AW86100\x64\AW86100.dll'),
    [string]$OutFile = (Join-Path $PSScriptRoot 'aw86100_inspection.txt')
)

$ErrorActionPreference = 'Stop'

function Convert-RvaToFileOffset {
    param($Sections, [uint32]$Rva)
    foreach ($s in $Sections) {
        $size = [Math]::Max($s.VirtualSize, $s.SizeOfRawData)
        if ($Rva -ge $s.VirtualAddress -and $Rva -lt ($s.VirtualAddress + $size)) {
            return $s.PointerToRawData + ($Rva - $s.VirtualAddress)
        }
    }
    return $null
}

function Read-CString {
    param([byte[]]$Bytes, [int]$Offset)
    if ($Offset -lt 0 -or $Offset -ge $Bytes.Length) { return '<bad offset>' }
    $end = $Offset
    while ($end -lt $Bytes.Length -and $Bytes[$end] -ne 0) { $end++ }
    return [System.Text.Encoding]::ASCII.GetString($Bytes, $Offset, $end - $Offset)
}

# ---- file check ----
if (-not (Test-Path -LiteralPath $DllPath)) {
    Write-Host "ERROR: DLL not found: $DllPath" -ForegroundColor Red
    Write-Host "Usage: powershell -ExecutionPolicy Bypass -File .\inspect_aw86100_dll.ps1 -DllPath <path>" -ForegroundColor Yellow
    exit 1
}

$resolved = (Resolve-Path -LiteralPath $DllPath).Path
$bytes = [System.IO.File]::ReadAllBytes($resolved)
if ($bytes.Length -lt 0x40) {
    Write-Host "ERROR: file too small to be a PE: $resolved" -ForegroundColor Red
    exit 1
}

# ---- DOS header ----
if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
    Write-Host "ERROR: not an MZ/PE file: $resolved" -ForegroundColor Red
    exit 1
}
$peOffset = [System.BitConverter]::ToInt32($bytes, 0x3C)

# ---- PE signature 'PE\0\0' ----
if ([System.BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) {
    Write-Host "ERROR: invalid PE signature at offset 0x$($peOffset.ToString('X'))" -ForegroundColor Red
    exit 1
}

# ---- COFF header (IMAGE_FILE_HEADER, 20 bytes after 'PE\0\0') ----
$coffOff        = $peOffset + 4
$machine        = [System.BitConverter]::ToUInt16($bytes, $coffOff)
$numSections    = [System.BitConverter]::ToUInt16($bytes, $coffOff + 2)
$sizeOfOptional = [System.BitConverter]::ToUInt16($bytes, $coffOff + 16)

# ---- Optional header ----
$optOff = $coffOff + 20
$magic  = [System.BitConverter]::ToUInt16($bytes, $optOff)
$is64   = ($magic -eq 0x20B)

# Data Directories live at the end of the optional header
# PE32:  optOff + 96   (28 standard + 68 windows-specific)
# PE32+: optOff + 112  (24 standard + 88 windows-specific)
$ddOff = if ($is64) { $optOff + 112 } else { $optOff + 96 }

# DataDir[0] = Export Table, DataDir[1] = Import Table; each = 8 bytes (RVA + Size)
$exportRva  = [System.BitConverter]::ToUInt32($bytes, $ddOff + 0 * 8 + 0)
$exportSize = [System.BitConverter]::ToUInt32($bytes, $ddOff + 0 * 8 + 4)
$importRva  = [System.BitConverter]::ToUInt32($bytes, $ddOff + 1 * 8 + 0)
$importSize = [System.BitConverter]::ToUInt32($bytes, $ddOff + 1 * 8 + 4)
$delayRva   = [System.BitConverter]::ToUInt32($bytes, $ddOff + 13 * 8 + 0)  # Delay-Load Import
$delaySize  = [System.BitConverter]::ToUInt32($bytes, $ddOff + 13 * 8 + 4)

# ---- Section table ----
$sectionsOff = $optOff + $sizeOfOptional
$sections = @()
for ($i = 0; $i -lt $numSections; $i++) {
    $base = $sectionsOff + $i * 40
    $sections += [pscustomobject]@{
        VirtualSize      = [System.BitConverter]::ToUInt32($bytes, $base + 8)
        VirtualAddress   = [System.BitConverter]::ToUInt32($bytes, $base + 12)
        SizeOfRawData    = [System.BitConverter]::ToUInt32($bytes, $base + 16)
        PointerToRawData = [System.BitConverter]::ToUInt32($bytes, $base + 20)
    }
}

# ---- Parse Export Directory ----
# IMAGE_EXPORT_DIRECTORY layout:
#   +0  Characteristics, +4 TimeDateStamp, +8 MajorVer, +10 MinorVer,
#   +12 Name (RVA), +16 Base, +20 NumberOfFunctions, +24 NumberOfNames,
#   +28 AddressOfFunctions (RVA), +32 AddressOfNames (RVA),
#   +36 AddressOfNameOrdinals (RVA)
$exports = @()
if ($exportRva -ne 0) {
    $eOff = Convert-RvaToFileOffset $sections $exportRva
    if ($null -ne $eOff) {
        $numberOfNames     = [System.BitConverter]::ToUInt32($bytes, $eOff + 24)
        $addressOfNamesRva = [System.BitConverter]::ToUInt32($bytes, $eOff + 32)
        $namesOff = Convert-RvaToFileOffset $sections $addressOfNamesRva
        if ($null -ne $namesOff) {
            for ($i = 0; $i -lt $numberOfNames; $i++) {
                $nameRva = [System.BitConverter]::ToUInt32($bytes, $namesOff + $i * 4)
                $nameOff = Convert-RvaToFileOffset $sections $nameRva
                if ($null -ne $nameOff) {
                    $exports += Read-CString $bytes $nameOff
                }
            }
        }
    }
}

# ---- Parse Import Directory ----
function Read-ImportDescriptors {
    param([byte[]]$Bytes, $Sections, [uint32]$DirRva)
    $result = @()
    if ($DirRva -eq 0) { return $result }
    $iOff = Convert-RvaToFileOffset $Sections $DirRva
    if ($null -eq $iOff) { return $result }
    while ($true) {
        # IMAGE_IMPORT_DESCRIPTOR (20 bytes):
        #   +0 OriginalFirstThunk, +4 TimeDateStamp, +8 ForwarderChain,
        #   +12 Name (RVA to ASCII), +16 FirstThunk
        $oft     = [System.BitConverter]::ToUInt32($Bytes, $iOff + 0)
        $nameRva = [System.BitConverter]::ToUInt32($Bytes, $iOff + 12)
        $ft      = [System.BitConverter]::ToUInt32($Bytes, $iOff + 16)
        if ($oft -eq 0 -and $nameRva -eq 0 -and $ft -eq 0) { break }
        if ($nameRva -ne 0) {
            $nameOff = Convert-RvaToFileOffset $Sections $nameRva
            if ($null -ne $nameOff) {
                $result += (Read-CString $Bytes $nameOff)
            }
        }
        $iOff += 20
    }
    return $result
}

$imports      = Read-ImportDescriptors $bytes $sections $importRva
$delayImports = @()
if ($delayRva -ne 0) {
    # IMAGE_DELAYLOAD_DESCRIPTOR (32 bytes):
    #   +0 Attributes, +4 DllNameRVA, +8 ModuleHandle,
    #   +12 IAT, +16 INT, +20 BoundIAT, +24 UnloadIAT, +28 TimeDateStamp
    $dOff = Convert-RvaToFileOffset $sections $delayRva
    while ($null -ne $dOff) {
        $attr    = [System.BitConverter]::ToUInt32($bytes, $dOff + 0)
        $nameRva = [System.BitConverter]::ToUInt32($bytes, $dOff + 4)
        if ($attr -eq 0 -and $nameRva -eq 0) { break }
        if ($nameRva -ne 0) {
            $nameOff = Convert-RvaToFileOffset $sections $nameRva
            if ($null -ne $nameOff) {
                $delayImports += (Read-CString $bytes $nameOff)
            }
        }
        $dOff += 32
    }
}

# ---- Build report ----
$lines = @()
$lines += '===== AW DLL Inspection ====='
$lines += "File         : $resolved"
$lines += "Size         : $($bytes.Length) bytes"
$lines += "Architecture : $(if ($is64) { 'x86-64 (PE32+)' } else { 'x86 (PE32)' })"
$lines += ("Machine code : 0x{0:X4}" -f $machine)
$lines += ("FileTimeUTC  : {0}" -f ([System.IO.File]::GetLastWriteTimeUtc($resolved).ToString('s')))
$lines += ''
$lines += '----- Imports (Static, statically linked DLLs) -----'
if ($imports.Count -eq 0) {
    $lines += '  (none)'
} else {
    $imports | Sort-Object -Unique | ForEach-Object { $lines += "  $_" }
}
$lines += ''
$lines += '----- Imports (Delay-load) -----'
if ($delayImports.Count -eq 0) {
    $lines += '  (none)'
} else {
    $delayImports | Sort-Object -Unique | ForEach-Object { $lines += "  $_" }
}
$lines += ''
$lines += "----- Exports (exported symbols, count = $($exports.Count)) -----"
if ($exports.Count -eq 0) {
    $lines += '  (none)'
} else {
    $exports | Sort-Object | ForEach-Object { $lines += "  $_" }
}
$lines += ''
$lines += '===== End ====='

$report = ($lines -join [Environment]::NewLine)
Write-Host $report

if ($OutFile) {
    [System.IO.File]::WriteAllText($OutFile, $report, [System.Text.UTF8Encoding]::new($false))
    Write-Host ''
    Write-Host "Report saved to: $OutFile" -ForegroundColor Green
    Write-Host 'Paste the content of this .txt back to Claude Code.' -ForegroundColor Green
}
