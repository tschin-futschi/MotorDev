$qtRoot = "D:\Qt\6.11.0\mingw_64"
$qtToolsRoot = "D:\Qt\Tools"
$mingwBin = Join-Path $qtToolsRoot "mingw1310_64\bin"
$cmakeBin = Join-Path $qtToolsRoot "CMake_64\bin"
$ninjaBin = Join-Path $qtToolsRoot "Ninja"
$qtBin = Join-Path $qtRoot "bin"
$qt6Dir = Join-Path $qtRoot "lib\cmake\Qt6"

$prependPaths = @($cmakeBin, $ninjaBin, $mingwBin, $qtBin)
$existingPaths = ($env:Path -split ';') | Where-Object { $_ }
$filteredPaths = $existingPaths | Where-Object { $prependPaths -notcontains $_ }
$env:Path = (($prependPaths + $filteredPaths) -join ';')

$env:CMAKE_PREFIX_PATH = $qtRoot
$env:Qt6_DIR = $qt6Dir

foreach ($name in @('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS')) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}

Write-Host "Qt environment configured."
Write-Host "Qt root: $qtRoot"
Write-Host "Qt6_DIR: $env:Qt6_DIR"
Write-Host "CMAKE_PREFIX_PATH: $env:CMAKE_PREFIX_PATH"
Write-Host "Compiler: $mingwBin"
