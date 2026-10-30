# Wordt door de installer aangeroepen: registreert of verwijdert de autostart-taak.
param(
    [Parameter(Mandatory)][ValidateSet('Install', 'Uninstall')][string]$Action,
    [string]$ExePath
)

$taskName = 'TempMonitor'

# Lopende instantie altijd eerst stoppen
Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
Get-Process -Name TempMonitor, PresentMon -ErrorAction SilentlyContinue | Stop-Process -Force

if ($Action -eq 'Uninstall') {
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    return
}

# Start bij aanmelden van elke gebruiker (BUILTIN\Users), met de hoogste rechten
# zodat de CPU-sensoren leesbaar zijn.
$action    = New-ScheduledTaskAction -Execute $ExePath -WorkingDirectory (Split-Path $ExePath)
$trigger   = New-ScheduledTaskTrigger -AtLogOn
$principal = New-ScheduledTaskPrincipal -GroupId 'S-1-5-32-545' -RunLevel Highest
$settings  = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
                -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) `
                -MultipleInstances IgnoreNew

Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal `
    -Settings $settings -Description 'Stuurt CPU/GPU-temperatuur naar het ronde USB-scherm' -Force | Out-Null
Start-ScheduledTask -TaskName $taskName
