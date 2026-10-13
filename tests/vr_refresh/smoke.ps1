param(
    [ValidateSet('DirectX11', 'OpenGL')][string]$Backend = 'OpenGL',
    [ValidateRange(5, 120)][int]$Seconds = 30
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$runtime = Join-Path $repo 'build/x64/Debug'
$destination = Join-Path $repo ("build/vr-refresh-smoke/{0}-{1}" -f $Backend, (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($archive in @('mk64.o2r', 'spaghetti.o2r')) {
    Copy-Item -LiteralPath (Join-Path $runtime $archive) -Destination $destination
}
$backendId = if ($Backend -eq 'DirectX11') { 1 } else { 2 }
$settings = @{
    Window = @{
        Backend = @{ Id = $backendId; Name = $Backend }
        Fullscreen = @{ Enabled = $false }
        Width = 960
        Height = 720
    }
    CVars = @{
        gSkipIntro = 2
        gAllowBackgroundInputs = 0
        gGameMasterVolume = [double]0.0
        gMainMusicVolume = [double]0.0
        gSFXMusicVolume = [double]0.0
        gEnvironmentVolume = [double]0.0
    }
}
$settings | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $destination 'spaghettify.cfg.json')
$process = Start-Process -FilePath (Join-Path $runtime 'Spaghettify.exe') -ArgumentList '--novr' `
    -WorkingDirectory $destination -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $destination 'stdout.log') `
    -RedirectStandardError (Join-Path $destination 'stderr.log')
try {
    if ($process.WaitForExit($Seconds * 1000)) {
        throw "Game exited during $Backend startup test, exit code $($process.ExitCode). Logs: $destination"
    }
    $process.Refresh()
    $log = Get-Content -LiteralPath (Join-Path $destination 'logs/Spaghetti Kart.log') -Raw
    if ($log -notmatch 'Loaded TrackInfo!' -or $log -match '\[(error|critical)\]') {
        throw "Startup log did not pass for $Backend. Logs: $destination"
    }
    [pscustomobject]@{
        Backend = $Backend
        AliveAfterSeconds = $Seconds
        Responding = $process.Responding
        Muted = $true
        TrackInfoLoaded = $true
        WindowTitle = $process.MainWindowTitle
        Logs = $destination
    }
} finally {
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(5000)) {
            Stop-Process -Id $process.Id
            $process.WaitForExit()
        }
    }
}
