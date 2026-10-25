$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$context = Join-Path $PSScriptRoot 'context.ps1'
$runner = Join-Path $PSScriptRoot 'run-logged.ps1'
$shell = (Get-Process -Id $PID).Path
$script:checks = 0
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function LogPath([object[]]$Output) {
    $line = [string]$Output[0]
    Assert ($line -match '; log=(.+)$') 'Missing log path'
    return $Matches[1]
}

$status = @(& $context)
Assert (($status -join "`n") -match 'REPO .*libultraship') 'Renderer status missing'
Assert ($status.Count -le 29) 'Status output exceeded its budget'
$headings = @(& $context -Mode headings -MaxLines 3)
Assert ($headings.Count -le 4) 'Headings output exceeded its budget'
$section = @(& $context -Mode notes -Section 'Radial Pedals' -MaxLines 3)
Assert (($section -join "`n") -match 'Radial Pedals') 'Section lookup failed'
Assert ($section.Count -le 5) 'Section output exceeded its budget'
foreach ($name in @('no-such-section-7ed36e8f', 'VR Refresh')) {
    $failed = $false
    try { $null = & $context -Mode notes -Section $name } catch { $failed = $true }
    Assert $failed "Missing/ambiguous lookup accepted: $name"
}

$success = @(& $runner -Label helper-success -Command $shell -CommandArgs @(
    '-NoProfile', '-Command', 'Write-Output "argument with spaces"; [Console]::Error.WriteLine("stderr retained"); exit 0'
))
$exitCode = $LASTEXITCODE
Assert ($exitCode -eq 0 -and $success.Count -eq 1) 'Success must emit one line and exit zero'
$successLog = Get-Content -Raw -LiteralPath (LogPath $success)
Assert ($successLog.Contains('argument with spaces') -and $successLog.Contains('stderr retained')) 'Full native output was not retained'

$failure = @(& $runner -Label helper-failure -TailLines 2 -Command $shell -CommandArgs @(
    '-NoProfile', '-Command', '1..5 | ForEach-Object { Write-Output "line-$_" }; exit 7'
))
$exitCode = $LASTEXITCODE
Assert ($exitCode -eq 7) 'Native failure code lost'
Assert ($failure.Count -eq 3 -and $failure[-1] -eq 'line-5') 'Failure tail is not bounded'
$failureLog = Get-Content -Raw -LiteralPath (LogPath $failure)
Assert ($failureLog.Contains('line-1') -and $failureLog.Contains('line-5')) 'Failure log lost output'

$missing = @(& $runner -Label helper-missing -TailLines 2 -Command 'no-such-executable-7ed36e8f')
Assert ($LASTEXITCODE -ne 0 -and $missing.Count -le 3) 'Missing executable reported success or excessive output'
Assert (Test-Path -LiteralPath (LogPath $missing)) 'Missing-command diagnostic log absent'

$fixture = Join-Path $root ('build/agent-logs/path with spaces ' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$null = New-Item -ItemType Directory -Path $fixture
$before = (Get-Location).Path
$location = @(& $runner -Label helper-cwd -WorkingDirectory $fixture -Command $shell -CommandArgs @(
    '-NoProfile', '-Command', '(Get-Location).Path'
))
$exitCode = $LASTEXITCODE
Assert ($exitCode -eq 0 -and (Get-Location).Path -eq $before) 'Caller working directory changed'
Assert ((Get-Content -Raw -LiteralPath (LogPath $location)).Trim() -eq $fixture) 'Working directory with spaces failed'

$wide = @(& $runner -Label helper-wide -TailLines 1 -Command $shell -CommandArgs @(
    '-NoProfile', '-Command', 'Write-Output ("x" * 2000); exit 2'
))
Assert ($LASTEXITCODE -eq 2 -and $wide[-1].Length -le 420) 'Wide failure output was not bounded'
Assert ((Get-Content -Raw -LiteralPath (LogPath $wide)).Trim().Length -eq 2000) 'Full wide line was not retained'
Write-Output "PASS: $script:checks helper assertions; no game or device interaction."
