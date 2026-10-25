#Requires -RunAsAdministrator
[CmdletBinding()]
param([string]$OutputDirectory = 'E:\SpaghettiKart-VR-refresh\build\windows-lan-repair-20261016')
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$receipt = [ordered]@{ Started = (Get-Date).ToString('o'); Completed = $false; Changes = @(); Error = $null }
try {
    $computer = Get-CimInstance Win32_ComputerSystem
    if ($computer.PartOfDomain) { throw 'Domain membership changed since inspection; no changes applied.' }
    Get-NetFirewallProfile -PolicyStore ActiveStore |
        Select-Object Name,Enabled,AllowInboundRules,AllowLocalFirewallRules,NotifyOnListen |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'profiles-before.json')
    & netsh.exe advfirewall export (Join-Path $OutputDirectory 'firewall-before.wfw') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Firewall backup failed.' }
    $key = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\Network Connections'
    $entry = Get-ItemProperty -LiteralPath $key -Name NC_PersonalFirewallConfig -ErrorAction SilentlyContinue
    if ($null -ne $entry) {
        & reg.exe export 'HKLM\SOFTWARE\Policies\Microsoft\Windows\Network Connections' (Join-Path $OutputDirectory 'network-connections-before.reg') /y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Registry backup failed.' }
        # The installed ADMX maps zero to the enabled legacy prohibition. No other entries are touched.
        if ($entry.NC_PersonalFirewallConfig -eq 0) {
            Remove-ItemProperty -LiteralPath $key -Name NC_PersonalFirewallConfig
            $receipt.Changes += 'Removed legacy NC_PersonalFirewallConfig=0.'
        }
    }
    $program = 'E:\SpaghettiKart-VR-refresh\build\x64\Debug\Spaghettify.exe'
    if (-not (Test-Path -LiteralPath $program -PathType Leaf)) { throw 'Game executable missing.' }
    $ruleName = 'SpaghettiKart-LAN-UDP-54777'
    $existing = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    if (-not $existing) {
        New-NetFirewallRule -Name $ruleName -DisplayName 'SpaghettiKart LAN race (UDP 54777)' `
            -Description 'Phone/PC race on the private local subnet. Added 2026-10-16.' `
            -Program $program -Direction Inbound -Action Allow -Enabled True -Profile Private `
            -Protocol UDP -LocalPort 54777 -RemoteAddress LocalSubnet -EdgeTraversalPolicy Block | Out-Null
        $receipt.Changes += 'Added private-subnet UDP54777 exception for the exact game executable.'
    }
    # Existing public-network block rules, account memberships, Windows components,
    # management providers, firewall profiles and unrelated settings are unchanged.
    & gpresult.exe /scope computer /z 2>&1 | Out-File -LiteralPath (Join-Path $OutputDirectory 'computer-settings.txt') -Encoding utf8
    $receipt.Completed = $true
} catch {
    $receipt.Error = $_.Exception.Message
} finally {
    $receipt.Finished = (Get-Date).ToString('o')
    $receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
}
if (-not $receipt.Completed) { exit 1 }
