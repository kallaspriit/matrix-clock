<#
.SYNOPSIS
    Registers clock_host.py as a scheduled task that starts at logon.

.DESCRIPTION
    Runs the host under pythonw.exe so no console window appears, in --tray mode so it does not
    need stdin, reconnects on its own, and offers a tray menu. A logon trigger is used rather than a
    service because the task needs the user's desktop session for the tray icon, not admin rights.

    The tray menu's "Release port" is the intended way to free the COM port for a firmware upload;
    it reconnects by itself afterwards, so the task never needs stopping.

    The task restarts itself if it dies, and the daemon retries internally when the board is absent,
    so it survives both the clock being unplugged and the COM port not having enumerated yet.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\install_autostart.ps1
    powershell -ExecutionPolicy Bypass -File .\install_autostart.ps1 -Uninstall
#>

[CmdletBinding()]
param(
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$taskName = "MatrixClockHost"

if ($Uninstall) {
    if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
        Write-Host "Removed scheduled task '$taskName'."
    } else {
        Write-Host "No scheduled task '$taskName' to remove."
    }

    return
}

$scriptPath = Join-Path $PSScriptRoot "clock_host.py"

if (-not (Test-Path $scriptPath)) {
    throw "clock_host.py was not found next to this script ($scriptPath)"
}

# pythonw rather than python: no console window, which also means no stdin, which is exactly why
# the host needs --tray (or --daemon) here rather than its interactive mode
$pythonw = (Get-Command pythonw.exe -ErrorAction SilentlyContinue).Source

if (-not $pythonw) {
    throw "pythonw.exe is not on PATH. Install Python or add it to PATH, then re-run."
}

& $pythonw -c "import serial" 2>$null
if (-not $?) {
    throw "pyserial is not installed for $pythonw. Run: pip install pyserial"
}

# The tray is optional, the host falls back to headless without it, so warn rather than fail
& $pythonw -c "import pystray, PIL" 2>$null
if (-not $?) {
    Write-Warning "pystray/pillow are not installed, so there will be no tray icon."
    Write-Warning "Run 'pip install pystray pillow' and restart the task to get one."
}

$logPath = Join-Path $env:LOCALAPPDATA "matrix-clock\host.log"
New-Item -ItemType Directory -Force -Path (Split-Path $logPath) | Out-Null

$argument = '"{0}" --tray --log "{1}"' -f $scriptPath, $logPath

$action = New-ScheduledTaskAction -Execute $pythonw -Argument $argument
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME

$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -StartWhenAvailable `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -RestartCount 3

Register-ScheduledTask `
    -TaskName $taskName `
    -Action $action `
    -Trigger $trigger `
    -Settings $settings `
    -Description "Keeps the matrix-clock panel time synced over USB serial." `
    -Force | Out-Null

Write-Host "Registered scheduled task '$taskName'."
Write-Host "  runs:  $pythonw $argument"
Write-Host "  log:   $logPath"
Write-Host ""
Write-Host "Start it now without logging out:  Start-ScheduledTask -TaskName $taskName"
Write-Host "Remove it:                         .\install_autostart.ps1 -Uninstall"
