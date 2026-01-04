$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$SigningTool = "STM32_SigningTool_CLI"
$Flash = $true
$FlashTool = "STM32_Programmer_CLI"
$BuildType = "Release"

# Function to sign a binary
function Sign-Binary {
    param(
        [string]$ProjectName,
        [string]$BuildDir,
        [string]$BinFile,
        [string]$SignedBinFile
    )
    
    Write-Host "`n=== Signing $ProjectName ===" -ForegroundColor Cyan

    # Check if binary file exists (should be created by CMake post-build)
    if (-not (Test-Path $BinFile)) {
        Write-Error "Binary file not found: $BinFile. Please build the project first."
        return $false
    }

    Write-Host "Using binary file: $BinFile" -ForegroundColor Green

    # Remove the signed binary if it exists
    if (Test-Path $SignedBinFile) {
        Remove-Item $SignedBinFile -Force
    }

    # Sign the binary
    Write-Host "Signing binary with type: $SigningType..." -ForegroundColor Yellow
    $signingArgs = @(
        "-bin", $BinFile,
        "-nk",
        "-of", "0x80000000",
        "-t", "fsbl",
        "-o", $SignedBinFile,
        "-hv", "2.3",
        "-dump", $SignedBinFile,
        "-align"
    )
    $output = & $SigningTool $signingArgs 2>&1
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        Write-Error "Failed to sign binary (exit code: $exitCode)"
        return $false
    }
    Write-Host "Signed binary created: $SignedBinFile" -ForegroundColor Green
    
    return $true
}

# Function to find external loader
function Get-ExternalLoader {
    # Try to find STM32CubeProgrammer installation
    $possiblePaths = @(
        "${env:ProgramFiles}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr",
        "${env:ProgramFiles(x86)}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr",
        "$env:LOCALAPPDATA\Programs\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    # Try to find from STM32_Programmer_CLI location
    try {
        $programmerPath = (Get-Command "STM32_Programmer_CLI" -ErrorAction Stop).Source
        $programmerDir = Split-Path $programmerPath -Parent
        $externalLoader = Join-Path $programmerDir "ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr"
        if (Test-Path $externalLoader) {
            return $externalLoader
        }
    }
    catch {
        # Ignore if command not found
    }
    
    return $null
}

# Function to flash a binary using ST-Link
function Flash-Binary {
    param(
        [string]$ProjectName,
        [string]$SignedBinFile,
        [string]$Address,
        [string]$FlashToolPath
    )
    
    Write-Host "`n=== Flashing $ProjectName ===" -ForegroundColor Cyan
    
    # Check if signed binary file exists
    if (-not (Test-Path $SignedBinFile)) {
        Write-Error "Signed binary file not found: $SignedBinFile. Please sign the binary first."
        return $false
    }
    
    Write-Host "Using signed binary: $SignedBinFile" -ForegroundColor Green
    Write-Host "Target address: $Address" -ForegroundColor Green
    
    # Get external loader for XSPI flash
    $externalLoader = Get-ExternalLoader
    if (-not $externalLoader) {
        Write-Warning "External loader not found. Attempting to flash without external loader (may fail for external flash)."
        Write-Warning "Expected location: ...\STM32CubeProgrammer\bin\ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr"
    }
    else {
        Write-Host "Using external loader: $externalLoader" -ForegroundColor Green
    }
    
    # Flash the binary using STM32_Programmer_CLI
    Write-Host "Flashing binary to external flash..." -ForegroundColor Yellow
    $flashArgs = @(
        "-c", "port=SWD mode=HOTPLUG ap=1"
    )
    
    if ($externalLoader) {
        $flashArgs += "-el", $externalLoader
    }
    
    $flashArgs += "-hardRst", "-w", $SignedBinFile, $Address
    
    try {
        $output = & $FlashToolPath $flashArgs 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        if ($exitCode -ne 0) {
            Write-Error "Failed to flash binary (exit code: $exitCode)"
            return $false
        }
        Write-Host "Binary flashed successfully at address $Address" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "Error executing flash tool: $_"
        return $false
    }
}

# Main execution
Write-Host "STM32 Binary Signing Script" -ForegroundColor Magenta
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Gray
Write-Host "Build Type: $BuildType" -ForegroundColor Gray

# Check if signing tool is available
try {
    $null = Get-Command $SigningTool -ErrorAction Stop
}
catch {
    Write-Error "Signing tool not found: $SigningTool. Please ensure it's in your PATH."
    exit 1
}

# Check if flash tool is available (if flashing is requested)
try {
    $null = Get-Command $FlashTool -ErrorAction Stop
}
catch {
    Write-Error "Flash tool not found: $FlashTool. Please ensure STM32CubeProgrammer is installed and in your PATH."
    exit 1
}
$success = $true

# Determine build directory based on build type
$buildSubDir = if ($BuildType -eq "Release") { "Release" } else { "Debug" }
# Fallback to root build directory if preset-based directory doesn't exist
$fsblBuildDir = Join-Path $ProjectRoot "FSBL\build"
$appliBuildDir = Join-Path $ProjectRoot "Appli\build"

# Try preset-based directory first, then fallback to root build directory
$fsblBinPath = Join-Path $fsblBuildDir "$buildSubDir\Firmware_FSBL.bin"
if (-not (Test-Path $fsblBinPath)) {
    $fsblBinPath = Join-Path $fsblBuildDir "Firmware_FSBL.bin"
}

$appliBinPath = Join-Path $appliBuildDir "$buildSubDir\Firmware_Appli.bin"
if (-not (Test-Path $appliBinPath)) {
    $appliBinPath = Join-Path $appliBuildDir "Firmware_Appli.bin"
}

# Sign FSBL
$fsblBin = $fsblBinPath
$fsblSigned = Join-Path $fsblBuildDir "Firmware_FSBL-trusted.bin"
    
if (Sign-Binary -ProjectName "FSBL" -BuildDir (Join-Path $ProjectRoot "FSBL\build") -BinFile $fsblBin -SignedBinFile $fsblSigned) {
    if ($Flash -and -not (Flash-Binary -ProjectName "FSBL" -SignedBinFile $fsblSigned -Address "0x70000000" -FlashToolPath $FlashTool)) {
        $success = $false
    }
}
else {
    $success = $false
}

# Sign Appli
$appliBin = $appliBinPath
$appliSigned = Join-Path $appliBuildDir "Firmware_Appli-trusted.bin"
    
if (Sign-Binary -ProjectName "Appli" -BuildDir (Join-Path $ProjectRoot "Appli\build") -BinFile $appliBin -SignedBinFile $appliSigned) {
    if ($Flash -and -not (Flash-Binary -ProjectName "Appli" -SignedBinFile $appliSigned -Address "0x70100000" -FlashToolPath $FlashTool)) {
        $success = $false
    }
}
else {
    $success = $false
}

# Check if the operation was successful
if ($success) {
    Write-Host "`n=== Operation completed successfully ===" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "`n=== Operation completed with errors ===" -ForegroundColor Red
    exit 1
}

