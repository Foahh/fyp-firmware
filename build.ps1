$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot

Write-Host "Building Release Configuration" -ForegroundColor Magenta
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Gray

# Configure and build FSBL in Release mode
Write-Host "`n=== Configuring FSBL (Release) ===" -ForegroundColor Cyan
Push-Location (Join-Path $ProjectRoot "FSBL")
try {
    cmake --preset Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to configure FSBL"
        exit 1
    }
    
    Write-Host "`n=== Building FSBL (Release) ===" -ForegroundColor Cyan
    cmake --build --preset Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to build FSBL"
        exit 1
    }
    Write-Host "FSBL built successfully" -ForegroundColor Green
}
finally {
    Pop-Location
}

# Configure and build Appli in Release mode
Write-Host "`n=== Configuring Appli (Release) ===" -ForegroundColor Cyan
Push-Location (Join-Path $ProjectRoot "Appli")
try {
    cmake --preset Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to configure Appli"
        exit 1
    }
    
    Write-Host "`n=== Building Appli (Release) ===" -ForegroundColor Cyan
    cmake --build --preset Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to build Appli"
        exit 1
    }
    Write-Host "Appli built successfully" -ForegroundColor Green
}
finally {
    Pop-Location
}

Write-Host "`n=== Release build completed successfully ===" -ForegroundColor Green
Write-Host "Binary files:" -ForegroundColor Yellow
Write-Host "  FSBL:  $ProjectRoot\FSBL\build\Release\Firmware_FSBL.bin" -ForegroundColor Gray
Write-Host "  Appli: $ProjectRoot\Appli\build\Release\Firmware_Appli.bin" -ForegroundColor Gray
Write-Host "`nTo flash the firmware, run: .\flash.ps1" -ForegroundColor Cyan
