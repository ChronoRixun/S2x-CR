param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
# Run with Windows PowerShell -STA after building Release. Never reads real preferences.
$ErrorActionPreference='Stop'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-themes-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$assembly=[Reflection.Assembly]::LoadFrom(([IO.Path]::GetFullPath($AssemblyPath)))
$app=New-Object S2x.ServerManager.App
$app.InitializeComponent()
$path=Join-Path $testRoot 'theme-test-settings.json'
[S2x.ServerManager.Services.ThemeManager]::Initialize($path)
$ink=$app.FindResource('Ink')
foreach($mode in @('Classic','Light','HighContrast','Classic')) {
    [S2x.ServerManager.Services.ThemeManager]::Apply([S2x.ServerManager.Services.ThemeMode]::$mode)
    if($app.FindResource('Ink') -ne $ink){throw 'Theme replaced brush identity'}
    $warning=$null
    $loaded=[S2x.ServerManager.Services.ThemeSettings]::Load($path,[ref]$warning)
    if($loaded.ToString() -ne $mode){throw 'Theme persistence mismatch'}
    $window=New-Object S2x.ServerManager.Views.SettingsDialog
    $window.Measure([Windows.Size]::new(460,600));$window.Arrange([Windows.Rect]::new(0,0,460,600));$window.UpdateLayout()
    $content=$window.Content
    $content.Background=$app.FindResource('Panel')
    $content.Measure([Windows.Size]::new(460,600));$content.Arrange([Windows.Rect]::new(0,0,460,600));$content.UpdateLayout()
    Write-Output "PASS $mode live brush identity, preference roundtrip and dialog layout"
}
[IO.File]::WriteAllText($path,'{broken')
$warning=$null
$result=[S2x.ServerManager.Services.ThemeSettings]::Load($path,[ref]$warning)
if($result -ne [S2x.ServerManager.Services.ThemeMode]::Classic -or !$warning){throw 'Invalid settings did not fallback'}
Write-Output 'PASS corrupt preference fallback'
$blocked=Join-Path $testRoot 'theme-blocked-settings'
New-Item -ItemType Directory -Force $blocked | Out-Null
[S2x.ServerManager.Services.ThemeManager]::Initialize($blocked)
try{[S2x.ServerManager.Services.ThemeManager]::Apply([S2x.ServerManager.Services.ThemeMode]::Light);throw 'Write failure was not reported'}catch{if($_.Exception.Message -eq 'Write failure was not reported'){throw}}
if([S2x.ServerManager.Services.ThemeManager]::Current -ne [S2x.ServerManager.Services.ThemeMode]::Classic){throw 'Failed save changed theme'}
Remove-Item -LiteralPath $blocked
Write-Output 'PASS failed preference save preserves visible theme' 
Remove-Item -LiteralPath $path
}
finally {
    if($app){$app.Shutdown()}
    # Delete only known test files and the uniquely-created empty test directories.
    $settings=Join-Path $testRoot 'theme-test-settings.json'
    $blocked=Join-Path $testRoot 'theme-blocked-settings'
    if(Test-Path -LiteralPath $settings){Remove-Item -LiteralPath $settings}
    if(Test-Path -LiteralPath $blocked){Remove-Item -LiteralPath $blocked}
    Remove-Item -LiteralPath $testRoot
}
