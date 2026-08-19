param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [string]$DllDirectory,
    [string]$DllName
)

Write-Host "Post-build configuration: $Configuration"
Write-Host "DLL directory: $DllDirectory"
Write-Host "DLL name:      $DllName"

if ([string]::IsNullOrWhiteSpace($DllName))
{
    Write-Error "TargetName/DLL name was not provided."
    exit 1
}

# Test DLL is present.
$DllPath = Join-Path $DllDirectory "${DllName}.dll"

if (-not (Test-Path $DllPath -PathType Leaf)) {
    Write-Error "DLL `"${DllPath}`" not found."
    exit 1
}

Write-Host "Plugin DLL found at `"${DllPath}`"" -ForegroundColor Green

# Resolve a plugin name without "_mm".
$PluginName = $DllName -replace "_mm$", ""
Write-Host "Plugin name defined as `"${PluginName}`"" -ForegroundColor green

# HLDS paths.
$HldsPath = "D:\steamcmd\steamapps\common\Half-Life"
$HldsExecutablePath = Join-Path $HldsPath "hlds.exe"

# Check hlds executable exists.
if (-not (Test-Path $HldsExecutablePath -PathType Leaf)) {
    Write-Error "HLDS executable not found: `"${HldsExecutablePath}`"."
    exit 1
}

# Get local IPv4 address used by the default route.
$HldsIP = (
    Get-NetRoute -DestinationPrefix "0.0.0.0/0" |
    Sort-Object RouteMetric |
    Select-Object -First 1 |
    ForEach-Object {
        Get-NetIPAddress `
            -AddressFamily IPv4 `
            -InterfaceIndex $_.InterfaceIndex |
            Select-Object -First 1 -ExpandProperty IPAddress
    }
)

if ([string]::IsNullOrWhiteSpace($HldsIP)) {
    Write-Error "Could not determine local IPv4 address."
    exit 1
}

# Find HLDS process and kill it if it exists.
$HldsProcesses = Get-CimInstance Win32_Process -Filter "Name = 'hlds.exe'" |
    Where-Object {
        $_.ExecutablePath -and
        $_.ExecutablePath.Equals(
            $HldsExecutablePath,
            [StringComparison]::OrdinalIgnoreCase
        )
    }

foreach ($HldsProcess in $HldsProcesses) {
    Write-Host "Stopping existing HLDS process (PID $($HldsProcess.ProcessId))..."

    Stop-Process `
        -Id $HldsProcess.ProcessId `
        -Force `
        -ErrorAction SilentlyContinue
}

if ($HldsProcesses) {
    Start-Sleep -Seconds 1
}

# Find an available HLDS UDP port.
$HldsPort = 27015

while ($true) {
    $InUse = Get-NetUDPEndpoint `
        -LocalPort $HldsPort `
        -ErrorAction SilentlyContinue

    if (-not $InUse) {
        break
    }

    $NextPort = $HldsPort + 1

    Write-Host "Port `"${HldsPort}`" is already in use. Trying port `"${NextPort}`"..."

    $HldsPort = $NextPort
}

# Executable parameters.
$HldsArgs = @(
    "-dev"
    "-console"
    "-game", "cstrike"
    "-port", $HldsPort
    "-pingboost", "3"
    "-steam"
    "-master"
    "-secure"
    "-bots"
    "-timeout", "3"
    "+ip", $HldsIP
    "+map", "de_nuke"
    "+maxplayers", "32"
    "+sys_ticrate", "1000"
    "+log", "on"
)

# DLL destination.
$DllDestinationDir = Join-Path $HldsPath "cstrike\addons\$PluginName"

# Create the destination directory if needed.
New-Item -ItemType Directory -Path $DllDestinationDir -Force | Out-Null
Copy-Item -Path $DllPath -Destination $DllDestinationDir -Force

# Get project plugin config.
$ConfigPath = Join-Path $PSScriptRoot "config"

$ConfigFiles = Get-ChildItem -Path $ConfigPath -Filter "*.cfg" -File

if ($ConfigFiles.Count -ne 1) {
    Write-Error "Expected exactly one *.cfg file in `"${ConfigPath}`". Found $($ConfigFiles.Count)."
    exit 1
}

# AMXX config directory.
$ConfigDestinationDir = Join-Path $HldsPath "cstrike\addons\amxmodx\configs\plugins"

# Create the destination directory if needed.
New-Item -ItemType Directory -Path $ConfigDestinationDir -Force | Out-Null

# Copy config to destination
Copy-Item -Path $ConfigFiles[0].FullName -Destination $ConfigDestinationDir -Force

# Start HLDS.
Start-Process `
    -FilePath $HldsExecutablePath `
    -ArgumentList $HldsArgs `
    -WorkingDirectory $HldsPath `
    -WindowStyle Minimized

Write-Host `
    "Running HLDS at `"$HldsIP`:$HldsPort`" with arguments `"$($HldsArgs -join ' ')`"" `
    -ForegroundColor Green
