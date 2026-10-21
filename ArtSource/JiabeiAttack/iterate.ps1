<#
调参一轮：重新生成配方 → 重建该段特效 → 截图。
  .\iterate.ps1 [-Role changli] -Stage Attack01 -Shots 0.02,0.06,0.15 -Views Back,Front
截图在 Saved\Diagnostics\EffectScreenshots\<蒙太奇>_<View>_<ms>.png（嘉贝莉娜 AM_Jiabei_<段>，长离 AM_<段>）。UE 编辑器要先关掉。
#>
param(
    [string]$Stage = 'Attack01',
    [string]$Shots = '0.02,0.06,0.15',
    [string]$Views = 'Back,Front',
    [float]$Pitch = -20,
    [ValidateSet('jiabei', 'changli')][string]$Role = 'jiabei',
    [float]$CameraDistance = 0
)
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$montage = if ($Role -eq 'changli') { "AM_$Stage" } else { "AM_Jiabei_$Stage" }
python -I "$here\prepare_attack.py" --character $Role $Stage | Select-Object -Last 1
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\UEProjects\Wuwa\Wuwa.uproject' -run=WuwaChangliAttackEffectSetup `
    "-Character=$Role" "-Attack=$Stage" -Apply -unattended -nop4 -nosplash 2>&1 |
    Where-Object { $_ -match 'LogWuwaAttackEffectSetup: (Error|Display: Rebuilt)|LogWuwaReference\w*: Error|error' -and $_ -notmatch 'LogInit' } |
    ForEach-Object { $_ -replace '^.*(LogWuwa)', '$1' }
& "$here\shoot.ps1" -Role $Role -Montage $montage -Shots $Shots -Views $Views -Pitch $Pitch -CameraDistance $CameraDistance | Select-String 'Saved|Error'
