[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Command,
    [string[]]$CommandArgs = @(),
    [string]$WorkingDirectory = (Join-Path $PSScriptRoot '../..'),
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Label = 'command',
    [ValidateRange(1, 100)][int]$TailLines = 20
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$directory = Join-Path $root 'build/agent-logs'
$null = New-Item -ItemType Directory -Path $directory -Force
$log = Join-Path $directory ("{0}-{1}-{2}.log" -f $Label, (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8))
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$code = 1
Push-Location -LiteralPath $WorkingDirectory
try {
    $native = Get-Command $Command -CommandType Application -ErrorAction Stop | Select-Object -First 1
    # Stderr belongs in the log; the native exit code determines success.
    $ErrorActionPreference = 'Continue'
    $PSNativeCommandUseErrorActionPreference = $false
    $global:LASTEXITCODE = $null
    & $native.Source @CommandArgs *> $log
    $code = if ($null -eq $global:LASTEXITCODE) { 1 } else { $global:LASTEXITCODE }
} catch {
    $_ | Out-String | Add-Content -LiteralPath $log
    $code = 1
} finally {
    Pop-Location
}
$timer.Stop()
Write-Output ("{0}: exit={1}; seconds={2:N1}; log={3}" -f $Label, $code, $timer.Elapsed.TotalSeconds, $log)
if ($code -ne 0) {
    Get-Content -LiteralPath $log -Tail $TailLines | ForEach-Object {
        $line = [string]$_
        if ($line.Length -gt 400) { $line = $line.Substring(0, 400) + ' [line truncated]' }
        Write-Output $line
    }
}
exit $code
