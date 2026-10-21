<#
不放角色，把 Niagara 系统并排摆在固定镜头前播放并截图（EffectVisual.NiagaraGallery，需要真实 RHI）。
  .\gallery.ps1 -Systems NS_Reference_Jiabei_R1a_Dg_02_1,NS_Reference_Jiabei_R1a_Kaiqiang_01 -Tag A01 [-Pitch -89 -Distance 400]
名字不带路径时按 -Role（jiabei / changli）的重建目录补全；也可以直接给 /Game/... 对象路径。
#>
param(
    [string[]]$Systems,
    [string]$Shots = '0.05,0.15,0.3,0.6',
    [string]$Tag = 'Gallery',
    [float]$Scale = 1,
    [float]$Pitch = -32,
    [float]$Distance = 830,
    [switch]$DarkFloor,
    [int]$Columns = 0,
    [float]$Spacing = 400,
    [ValidateSet('jiabei', 'changli')][string]$Role = 'jiabei'
)
$title = if ($Role -eq 'changli') { 'Changli' } else { 'Jiabei' }
$ErrorActionPreference = 'Continue'
$paths = foreach ($s in $Systems) {
    if ($s.StartsWith('/Game/')) { $s } else { "/Game/Effects/$title/Reference/Niagara/$s.$s" }
}
$log = "C:\UEProjects\Wuwa\Saved\Logs\Gallery_$Tag.log"
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' 'C:\UEProjects\Wuwa\Wuwa.uproject' `
    '-ExecCmds=Automation RunTests EffectVisual.NiagaraGallery; Quit' -unattended -nosplash -RenderOffscreen -NoLiveCoding `
    '-TestExit=Automation Test Queue Empty' "-abslog=$log" `
    "-GallerySystems=$($paths -join ',')" "-GalleryShots=$Shots" "-GalleryTag=$Tag" "-GalleryScale=$Scale" "-GalleryPitch=$Pitch" "-GalleryDistance=$Distance" "-GalleryColumns=$Columns" "-GallerySpacing=$Spacing" $(if ($DarkFloor) { '-GalleryDarkFloor' }) | Out-Null
Select-String -Path $log -Pattern 'Saved .*\.png|Result=\{|Error:.*(Missing|Screenshot|PIE)' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[[^\]]+\]', '' }
