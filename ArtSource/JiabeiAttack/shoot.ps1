<#
在真实地图里切到嘉贝莉娜，播放指定段的蒙太奇并截图（EffectVisual.ChangliAttackScreenshots 测试，需要真实 RHI）。
  .\shoot.ps1 [-Role changli] -Montage AM_Jiabei_Attack01 -Shots 0.05,0.15,0.3 -Views Back,Front -Pitch -20 [-DarkFloor 脚下铺深色地面，对照夜景视频的颜色]
截图在 Saved\Diagnostics\EffectScreenshots\<Tag>_<View>_<ms>.png。
#>
param(
    [string]$Montage = 'AM_Jiabei_Attack01',
    [string]$Shots = '0.05,0.15,0.3,0.5',
    [string]$Views = 'Back,Front',
    [float]$Pitch = -20,
    [switch]$DarkFloor,
    [switch]$NoPlay,
    [switch]$RotateCharacter,
    [float]$CameraHeight = 50,
    [float]$CameraDistance = 0,
    [ValidateSet('jiabei', 'changli')][string]$Role = 'jiabei'
)
$ErrorActionPreference = 'Continue'
$path = "/Game/Characters/Role/$Role/AnimMontage/$Montage.$Montage"
$log = "C:\UEProjects\Wuwa\Saved\Logs\JiabeiShot_$Montage.log"
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' 'C:\UEProjects\Wuwa\Wuwa.uproject' `
    '-ExecCmds=Automation RunTests EffectVisual.ChangliAttackScreenshots; Quit' -unattended -nosplash -RenderOffscreen -NoLiveCoding `
    '-TestExit=Automation Test Queue Empty' "-abslog=$log" `
    "-EffectRole=$Role" "-EffectMontagePath=$path" "-EffectMontage=$Montage" "-EffectShots=$Shots" "-EffectViews=$Views" "-EffectPitch=$Pitch" $(if ($DarkFloor) { '-EffectDarkFloor' }) $(if ($NoPlay) { '-EffectNoPlay' }) $(if ($RotateCharacter) { '-EffectRotateCharacter' }) "-EffectCameraHeight=$CameraHeight" "-EffectCameraDistance=$CameraDistance" | Out-Null
Select-String -Path $log -Pattern 'Saved .*\.png|Displacement|Result=\{|Error:.*(Effect|switch|missing|Screenshot)' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[[^\]]+\]', '' }
