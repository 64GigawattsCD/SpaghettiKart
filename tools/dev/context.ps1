[CmdletBinding()]
param(
    [ValidateSet('status', 'headings', 'notes')][string]$Mode = 'status',
    [string]$Section,
    [ValidateRange(1, 120)][int]$MaxLines = 35
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path

function Show-Bounded([object[]]$Lines, [int]$Limit) {
    foreach ($line in @($Lines | Select-Object -First $Limit)) {
        $text = [string]$line
        if ($text.Length -gt 300) { $text = $text.Substring(0, 300) + ' [line truncated]' }
        Write-Output $text
    }
    if ($Lines.Count -gt $Limit) {
        Write-Output "[$($Lines.Count - $Limit) more lines; narrow the query or read the named file]"
    }
}

if ($Mode -eq 'status') {
    foreach ($repo in @($root, (Join-Path $root 'libultraship'))) {
        Write-Output "REPO $repo"
        if (-not (Test-Path -LiteralPath (Join-Path $repo '.git'))) {
            Write-Output 'Not initialized here.'
            continue
        }
        $status = @(& git.exe -C $repo status --short --branch)
        if ($LASTEXITCODE -ne 0) { throw "Git status failed: $repo" }
        Show-Bounded $status 12
        & git.exe -C $repo log -1 '--format=HEAD %h %s'
        if ($LASTEXITCODE -ne 0) { throw "Git log failed: $repo" }
    }
    Write-Output 'Tracking comparisons use local refs; no fetch, game launch or device access occurred.'
    return
}

$notes = Join-Path $root 'docs/codex-session-notes.md'
$lines = @(Get-Content -LiteralPath $notes)
$headings = @(for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i].StartsWith('## ')) {
        [pscustomobject]@{ Index = $i; Title = $lines[$i].Substring(3) }
    }
})
if ($Mode -eq 'headings') {
    Show-Bounded @($headings | ForEach-Object { "$($_.Index + 1): $($_.Title)" }) $MaxLines
    return
}
if ([string]::IsNullOrWhiteSpace($Section)) { throw 'Use -Section with a heading name or unique fragment.' }
$matchesFound = @($headings | Where-Object {
    $_.Title.IndexOf($Section, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
})
if ($matchesFound.Count -ne 1) {
    Show-Bounded @($matchesFound | ForEach-Object { $_.Title }) 10
    throw "Expected one matching section, found $($matchesFound.Count). Use -Mode headings to choose."
}
$start = $matchesFound[0].Index
$next = @($headings | Where-Object { $_.Index -gt $start } | Select-Object -First 1)
$end = if ($next.Count) { $next[0].Index - 1 } else { $lines.Count - 1 }
Write-Output "$notes : lines $($start + 1)-$($end + 1)"
Show-Bounded @($lines[$start..$end]) $MaxLines
