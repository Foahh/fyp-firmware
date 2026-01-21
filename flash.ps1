$ErrorActionPreference = "Stop"

#region Configuration
$Script:ProjectRoot = $PSScriptRoot
$Script:SigningTool = "STM32_SigningTool_CLI"
$Script:Flash = $true
$Script:FlashTool = "STM32_Programmer_CLI"
$Script:BuildType = "Release"
$Script:ProjectNamePrefix = "Firmware_"
$Script:ObjCopyTool = "arm-none-eabi-objcopy"
$Script:ExternalLoaderName = "MX66UW1G45G_STM32N6570-DK.stldr"
$Script:FlashHistoryFile = Join-Path $Script:ProjectRoot ".flash_history"

# Project configurations
$Script:Projects = @{
    FSBL  = @{
        SubDir        = "FSBL"
        FlashAddress  = "0x70000000"
        SigningType   = "fsbl"
        OffsetAddress = "0x80000000"
    }
    Appli = @{
        SubDir        = "Appli"
        FlashAddress  = "0x70100000"
        SigningType   = "fsbl"
        OffsetAddress = "0x80000000"
    }
}
#endregion

#region Helper Functions
function Test-CommandExists {
    param([string]$CommandName)
    
    try {
        $null = Get-Command $CommandName -ErrorAction Stop
        return $true
    }
    catch {
        return $false
    }
}

function Write-SectionHeader {
    param(
        [string]$Message,
        [string]$Color = "Cyan"
    )
    Write-Host "`n=== $Message ===" -ForegroundColor $Color
}

function Get-BuildDirectory {
    param([string]$ProjectSubDir)
    return Join-Path $Script:ProjectRoot "$ProjectSubDir\build\$Script:BuildType"
}

function Get-FileHashValue {
    param([string]$FilePath)
    
    if (-not (Test-Path $FilePath)) {
        return $null
    }
    
    try {
        $hash = Microsoft.PowerShell.Utility\Get-FileHash -Path $FilePath -Algorithm SHA256
        return $hash.Hash
    }
    catch {
        Write-Warning "Failed to compute hash for $FilePath : $_"
        return $null
    }
}

function Get-FlashHistory {
    if (-not (Test-Path $Script:FlashHistoryFile)) {
        return @{}
    }
    
    try {
        $content = Get-Content $Script:FlashHistoryFile -Raw -ErrorAction Stop
        if ([string]::IsNullOrWhiteSpace($content)) {
            return @{}
        }
        # Convert JSON to hashtable - handle both PowerShell 5.1 and 7+ compatibility
        $jsonObj = $content | ConvertFrom-Json -ErrorAction Stop
        $history = @{}
        if ($jsonObj -is [hashtable]) {
            # PowerShell 7+ with -AsHashtable returns hashtable directly
            $history = $jsonObj
        }
        else {
            # PowerShell 5.1 returns PSCustomObject, convert to hashtable
            $jsonObj.PSObject.Properties | ForEach-Object {
                $history[$_.Name] = $_.Value
            }
        }
        return $history
    }
    catch {
        Write-Warning "Failed to read flash history: $_"
        return @{}
    }
}

function Save-FlashHistory {
    param([hashtable]$History)
    
    try {
        # Convert hashtable to ordered dictionary to preserve key order, then to JSON
        $orderedDict = [ordered]@{}
        foreach ($key in $History.Keys) {
            $orderedDict[$key] = $History[$key]
        }
        $json = $orderedDict | ConvertTo-Json -Depth 10
        $json | Set-Content $Script:FlashHistoryFile -ErrorAction Stop
    }
    catch {
        Write-Warning "Failed to save flash history: $_"
    }
}

function Test-FileNeedsFlash {
    param(
        [string]$FilePath,
        [string]$CacheKey
    )
    
    $currentHash = Get-FileHashValue -FilePath $FilePath
    if ($null -eq $currentHash) {
        # File doesn't exist or can't be hashed, assume needs flash
        return $true
    }
    
    $history = Get-FlashHistory
    if (-not $history.ContainsKey($CacheKey)) {
        # No cached entry, needs flash
        return $true
    }
    
    $cachedHash = $history[$CacheKey]
    if ($cachedHash -ne $currentHash) {
        # File has changed, needs flash
        return $true
    }
    
    # File hasn't changed, skip flash
    Write-Host "File unchanged since last flash, skipping: $(Split-Path $FilePath -Leaf)" -ForegroundColor Gray
    return $false
}

function Update-FlashHistory {
    param(
        [string]$FilePath,
        [string]$CacheKey
    )
    
    $currentHash = Get-FileHashValue -FilePath $FilePath
    if ($null -eq $currentHash) {
        return
    }
    
    $history = Get-FlashHistory
    $history[$CacheKey] = $currentHash
    Save-FlashHistory -History $history
}
#endregion

#region Build Functions
function Invoke-ProjectBuild {
    param(
        [string]$ProjectName,
        [hashtable]$ProjectConfig
    )
    
    $projectDir = Join-Path $Script:ProjectRoot $ProjectConfig.SubDir
    $buildDir = Get-BuildDirectory -ProjectSubDir $ProjectConfig.SubDir
    $fullProjectName = "$Script:ProjectNamePrefix$ProjectName"
    
    Write-SectionHeader "Configuring $ProjectName ($Script:BuildType)"
    Push-Location $projectDir
    try {
        # Configure
        cmake --preset $Script:BuildType
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to configure $ProjectName"
        }
        
        # Build
        Write-SectionHeader "Building $ProjectName ($Script:BuildType)"
        cmake --build --preset $Script:BuildType
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to build $ProjectName"
        }
        Write-Host "$ProjectName built successfully" -ForegroundColor Green
    }
    finally {
        Pop-Location
    }
    
    # Clean old build artifacts
    Write-SectionHeader "Cleaning old $ProjectName build artifacts"
    $trustedBin = Join-Path $buildDir "$fullProjectName-trusted.bin"
    $oldBin = Join-Path $buildDir "$fullProjectName.bin"
    
    @($trustedBin, $oldBin) | Where-Object { Test-Path $_ } | ForEach-Object {
        Remove-Item $_ -Force
        Write-Host "Removed: $_" -ForegroundColor Gray
    }
    
    # Convert ELF to binary
    Write-SectionHeader "Converting $ProjectName ELF to binary"
    $elfFile = Join-Path $buildDir "$fullProjectName.elf"
    $binFile = Join-Path $buildDir "$fullProjectName.bin"
    
    if (-not (Test-Path $elfFile)) {
        throw "ELF file not found: $elfFile"
    }
    
    & $Script:ObjCopyTool -O binary $elfFile $binFile
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert $ProjectName ELF to binary"
    }
    Write-Host "Converted ELF to binary: $binFile" -ForegroundColor Green
    
    return $true
}
#endregion

#region Signing Functions
function Invoke-BinarySign {
    param(
        [string]$ProjectName,
        [string]$BinFile,
        [string]$SignedBinFile,
        [hashtable]$ProjectConfig
    )
    
    Write-SectionHeader "Signing $ProjectName"
    
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
    Write-Host "Signing binary with type: $($ProjectConfig.SigningType)..." -ForegroundColor Yellow
    $signingArgs = @(
        "-bin", $BinFile,
        "-nk",
        "-of", $ProjectConfig.OffsetAddress,
        "-t", $ProjectConfig.SigningType,
        "-o", $SignedBinFile,
        "-hv", "2.3",
        "-dump", $SignedBinFile,
        "-align"
    )
    
    try {
        $output = & $Script:SigningTool $signingArgs 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        
        if ($exitCode -ne 0) {
            Write-Error "Failed to sign binary (exit code: $exitCode)"
            return $false
        }
        
        Write-Host "Signed binary created: $SignedBinFile" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "Error executing signing tool: $_"
        return $false
    }
}
#endregion

#region Flash Functions
function Get-ExternalLoader {
    # Try common installation paths
    $possiblePaths = @(
        "${env:ProgramFiles}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\$Script:ExternalLoaderName",
        "${env:ProgramFiles(x86)}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\$Script:ExternalLoaderName",
        "$env:LOCALAPPDATA\Programs\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\$Script:ExternalLoaderName"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    # Try to find from STM32_Programmer_CLI location
    try {
        $programmerPath = (Get-Command $Script:FlashTool -ErrorAction Stop).Source
        $programmerDir = Split-Path $programmerPath -Parent
        $externalLoader = Join-Path $programmerDir "ExternalLoader\$Script:ExternalLoaderName"
        if (Test-Path $externalLoader) {
            return $externalLoader
        }
    }
    catch {
        # Ignore if command not found
    }
    
    return $null
}

function Invoke-BinaryFlash {
    param(
        [string]$ProjectName,
        [string]$SignedBinFile,
        [string]$Address,
        [string]$SourceBinFile
    )
    
    Write-SectionHeader "Flashing $ProjectName"
    
    if (-not (Test-Path $SignedBinFile)) {
        Write-Error "Image file not found: $SignedBinFile. Please sign/convert the binary first."
        return $false
    }

    # Enforce HEX images for flashing, which already embed absolute addresses.
    if (-not $SignedBinFile.ToLower().EndsWith(".hex")) {
        Write-Error "Only Intel HEX images are supported for flashing. Got: $SignedBinFile"
        return $false
    }
    
    # Check if file needs flashing based on cache - hash the source binary, not the signed HEX
    $cacheKey = "$ProjectName|$Address"
    $fileToHash = if ($SourceBinFile -and (Test-Path $SourceBinFile)) { $SourceBinFile } else { $SignedBinFile }
    if (-not (Test-FileNeedsFlash -FilePath $fileToHash -CacheKey $cacheKey)) {
        return $true  # Skip flash, file unchanged
    }
    
    Write-Host "Using image file: $SignedBinFile" -ForegroundColor Green

    # Get external loader for XSPI flash
    $externalLoader = Get-ExternalLoader
    if (-not $externalLoader) {
        Write-Warning "External loader not found. Attempting to flash without external loader (may fail for external flash)."
        Write-Warning "Expected location: ...\STM32CubeProgrammer\bin\ExternalLoader\$Script:ExternalLoaderName"
    }
    else {
        Write-Host "Using external loader: $externalLoader" -ForegroundColor Green
    }
    
    # Flash the image using STM32_Programmer_CLI
    Write-Host "Flashing HEX image to external flash..." -ForegroundColor Yellow
    $flashArgs = @(
        "-c", "port=SWD mode=HOTPLUG ap=1"
    )
    
    if ($externalLoader) {
        $flashArgs += "-el", $externalLoader
    }
    
    $flashArgs += "-hardRst", "-w", $SignedBinFile
    
    try {
        $output = & $Script:FlashTool $flashArgs 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        
        if ($exitCode -ne 0) {
            Write-Error "Failed to flash binary (exit code: $exitCode)"
            return $false
        }
        
        Write-Host "Binary flashed successfully at address $Address" -ForegroundColor Green
        
        # Update cache after successful flash - use source binary for hash
        $fileToHash = if ($SourceBinFile -and (Test-Path $SourceBinFile)) { $SourceBinFile } else { $SignedBinFile }
        Update-FlashHistory -FilePath $fileToHash -CacheKey $cacheKey
        
        return $true
    }
    catch {
        Write-Error "Error executing flash tool: $_"
        return $false
    }
}
#endregion

#region Main Processing
function Invoke-ProjectProcessing {
    param(
        [string]$ProjectName,
        [hashtable]$ProjectConfig
    )
    
    $buildDir = Get-BuildDirectory -ProjectSubDir $ProjectConfig.SubDir
    $fullProjectName = "$Script:ProjectNamePrefix$ProjectName"
    $binFile = Join-Path $buildDir "$fullProjectName.bin"
    $signedBinFile = Join-Path $buildDir "$fullProjectName-trusted.bin"
    $signedHexFile = Join-Path $buildDir "$fullProjectName-trusted.hex"
    
    # Build
    try {
        Invoke-ProjectBuild -ProjectName $ProjectName -ProjectConfig $ProjectConfig
    }
    catch {
        Write-Error "Failed to build $ProjectName : $_"
        return $false
    }
    
    # Sign
    if (-not (Invoke-BinarySign -ProjectName $ProjectName -BinFile $binFile -SignedBinFile $signedBinFile -ProjectConfig $ProjectConfig)) {
        return $false
    }

    # Convert the signed binary to an Intel HEX file with the correct base address
    Write-SectionHeader "Converting signed binary to HEX (base address $($ProjectConfig.FlashAddress))"

    # Remove existing HEX if present
    if (Test-Path $signedHexFile) {
        Remove-Item $signedHexFile -Force
    }

    # arm-none-eabi-objcopy -I binary -O ihex --change-addresses <flash_addr> in.bin out.hex
    & $Script:ObjCopyTool -I binary -O ihex --change-addresses $ProjectConfig.FlashAddress $signedBinFile $signedHexFile
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $signedHexFile)) {
        Write-Error "Failed to convert signed binary to HEX for $ProjectName"
        return $false
    }

    Write-Host "HEX image created: $signedHexFile" -ForegroundColor Green
    $imageForFlashing = $signedHexFile

    if ($Script:Flash) {
        if (-not (Invoke-BinaryFlash -ProjectName $ProjectName -SignedBinFile $imageForFlashing -Address $ProjectConfig.FlashAddress -SourceBinFile $binFile)) {
            return $false
        }
    }
    
    return $true
}

function Test-Prerequisites {
    $allValid = $true
    
    if (-not (Test-CommandExists -CommandName $Script:SigningTool)) {
        Write-Error "Signing tool not found: $Script:SigningTool. Please ensure it's in your PATH."
        $allValid = $false
    }
    
    if ($Script:Flash -and -not (Test-CommandExists -CommandName $Script:FlashTool)) {
        Write-Error "Flash tool not found: $Script:FlashTool. Please ensure STM32CubeProgrammer is installed and in your PATH."
        $allValid = $false
    }
    
    if (-not (Test-CommandExists -CommandName $Script:ObjCopyTool)) {
        Write-Error "Objcopy tool not found: $Script:ObjCopyTool. Please ensure ARM toolchain is in your PATH."
        $allValid = $false
    }
    
    return $allValid
}

function Invoke-FlashNetworkModel {
    param(
        [string]$NetworkHexFile
    )
    
    $fileName = Split-Path $NetworkHexFile -Leaf
    Write-SectionHeader "Flashing Network Model: $fileName"
    
    if (-not (Test-Path $NetworkHexFile)) {
        Write-Error "Network HEX file not found: $NetworkHexFile"
        return $false
    }
    
    # Check if file needs flashing based on cache
    $cacheKey = "Network|$fileName"
    if (-not (Test-FileNeedsFlash -FilePath $NetworkHexFile -CacheKey $cacheKey)) {
        return $true  # Skip flash, file unchanged
    }
    
    Write-Host "Using network HEX file: $NetworkHexFile" -ForegroundColor Green
    
    # Get external loader for XSPI flash
    $externalLoader = Get-ExternalLoader
    if (-not $externalLoader) {
        Write-Warning "External loader not found. Attempting to flash without external loader (may fail for external flash)."
        Write-Warning "Expected location: ...\STM32CubeProgrammer\bin\ExternalLoader\$Script:ExternalLoaderName"
    }
    else {
        Write-Host "Using external loader: $externalLoader" -ForegroundColor Green
    }
    
    # Flash the network HEX image using STM32_Programmer_CLI
    # Note: HEX files embed addresses, so we don't need to specify address explicitly
    Write-Host "Flashing network HEX image to external flash..." -ForegroundColor Yellow
    $flashArgs = @(
        "-c", "port=SWD mode=HOTPLUG ap=1"
    )
    
    if ($externalLoader) {
        $flashArgs += "-el", $externalLoader
    }
    
    $flashArgs += "-hardRst", "-w", $NetworkHexFile
    
    try {
        $output = & $Script:FlashTool $flashArgs 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        
        if ($exitCode -ne 0) {
            Write-Error "Failed to flash network model (exit code: $exitCode)"
            return $false
        }
        
        Write-Host "Network model flashed successfully: $fileName" -ForegroundColor Green
        
        # Update cache after successful flash
        Update-FlashHistory -FilePath $NetworkHexFile -CacheKey $cacheKey
        
        return $true
    }
    catch {
        Write-Error "Error executing flash tool: $_"
        return $false
    }
}

function Invoke-FlashAllNetworkModels {
    Write-SectionHeader "Flashing Network Models"
    
    $networkBinDir = Join-Path $Script:ProjectRoot "Networks\Bin"
    
    if (-not (Test-Path $networkBinDir)) {
        Write-Warning "Network Bin directory not found: $networkBinDir"
        Write-Warning "Skipping network model flash. Please generate the network models first."
        return $false
    }
    
    # Find all .hex files in the Networks/Bin directory
    $networkHexFiles = Get-ChildItem -Path $networkBinDir -Filter "*.hex" -File
    
    if ($networkHexFiles.Count -eq 0) {
        Write-Warning "No network HEX files found in: $networkBinDir"
        Write-Warning "Skipping network model flash. Please generate the network models first."
        return $false
    }
    
    Write-Host "Found $($networkHexFiles.Count) network HEX file(s) to flash" -ForegroundColor Green
    
    $allSucceeded = $true
    foreach ($hexFile in $networkHexFiles) {
        if (-not (Invoke-FlashNetworkModel -NetworkHexFile $hexFile.FullName)) {
            $allSucceeded = $false
        }
    }
    
    return $allSucceeded
}

#endregion

#region Main Execution
Write-Host "STM32 Binary Signing and Flashing Script" -ForegroundColor Magenta
Write-Host "Project Root: $Script:ProjectRoot" -ForegroundColor Gray
Write-Host "Build Type: $Script:BuildType" -ForegroundColor Gray
Write-Host "Flash Enabled: $Script:Flash" -ForegroundColor Gray

# Validate prerequisites
if (-not (Test-Prerequisites)) {
    exit 1
}

$allSucceeded = $true

# Process FSBL first
if ($Script:Projects.ContainsKey("FSBL")) {
    Write-SectionHeader "Processing FSBL" -Color Magenta
    if (-not (Invoke-ProjectProcessing -ProjectName "FSBL" -ProjectConfig $Script:Projects["FSBL"])) {
        $allSucceeded = $false
    }
}

# Flash network models after FSBL, before Appli (as per README documentation)
if ($Script:Flash) {
    if (-not (Invoke-FlashAllNetworkModels)) {
        Write-Warning "Network model flash failed or was skipped, but continuing..."
        # Don't fail the entire script if network flash fails
    }
}

# Process Appli after network models
if ($Script:Projects.ContainsKey("Appli")) {
    Write-SectionHeader "Processing Appli" -Color Magenta
    if (-not (Invoke-ProjectProcessing -ProjectName "Appli" -ProjectConfig $Script:Projects["Appli"])) {
        $allSucceeded = $false
    }
}

# Final status
if ($allSucceeded) {
    Write-SectionHeader "Operation completed successfully" -Color Green
    exit 0
}
else {
    Write-SectionHeader "Operation completed with errors" -Color Red
    exit 1
}
#endregion