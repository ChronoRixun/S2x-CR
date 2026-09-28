param([string]$RoslynCsc,[switch]$LiveMaster)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $root
try {
 if(!$RoslynCsc){
  $vswhere=Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
  if(Test-Path $vswhere){$RoslynCsc=(& $vswhere -latest -products '*' -find 'MSBuild\**\Roslyn\csc.exe' | Select-Object -First 1)}
 }
 if(!$RoslynCsc -or !(Test-Path $RoslynCsc)){throw 'Set -RoslynCsc to the Visual Studio Roslyn csc.exe path.'}
 & dotnet build tools/ServerManager/S2xServerManager.csproj -c Release --nologo
 if($LASTEXITCODE){throw 'Application build failed'}
 $out=Join-Path $root 'build/server-manager-tests'
 New-Item -ItemType Directory -Force $out | Out-Null
 $backend=@('tools/ServerManager/Services/MasterBrowserClient.cs','tools/ServerManager/Models/MasterServerRow.cs','tests/server-manager/MasterBrowserTests.cs')|ForEach-Object {(Resolve-Path $_).Path}
 & $RoslynCsc /nologo "/out:$out/master-tests.exe" @backend
 if($LASTEXITCODE){throw 'Backend test compile failed'}
 & "$out/master-tests.exe"
 if($LASTEXITCODE){throw 'Backend tests failed'}
 $wpf=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/WPF'
 $ui=@('tools/ServerManager/ViewModels/MasterBrowserViewModel.cs','tools/ServerManager/ViewModels/Observable.cs','tools/ServerManager/Models/GameData.cs','tools/ServerManager/Models/MasterServerRow.cs','tools/ServerManager/Services/MasterBrowserClient.cs','tests/server-manager/BrowserUiTests.cs')|ForEach-Object {(Resolve-Path $_).Path}
 & $RoslynCsc /nologo "/out:$out/browser-ui-tests.exe" "/r:$wpf/PresentationFramework.dll" "/r:$wpf/PresentationCore.dll" "/r:$wpf/WindowsBase.dll" @ui
 if($LASTEXITCODE){throw 'UI test compile failed'}
 & "$out/browser-ui-tests.exe"
 if($LASTEXITCODE){throw 'UI tests failed'}
 $ps=Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
 foreach($test in @('themes.ps1','card-order.ps1','card-drag.ps1','admin.ps1','autobalance.ps1')){
  & $ps -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot $test)
  if($LASTEXITCODE){throw "$test failed"}
 }
 $profileTest=(Resolve-Path tests/server-manager/SurvivalProfileTests.cs).Path
 & $RoslynCsc /nologo /r:System.Core.dll "/out:$out/profile-tests.exe" $profileTest
 if($LASTEXITCODE){throw 'Profile test compile failed'}
 $fixtures=@()
 foreach($spec in @(@('bodega','mp_zombie_windmill_srv'),@('olympus','mp_zombie_dnk_srv'))){
  $dir=Join-Path $out ('profile-'+$spec[0]);New-Item -ItemType Directory -Force $dir|Out-Null
  Set-Content (Join-Path $dir 'start.ps1') "throw 'Test fixture only'"
  $entries=@(foreach($bots in @(2,3)){@{key=($spec[0]+'-'+$bots);game='zombies';map=$spec[1];mode="With $bots bots";start='start.ps1';log='test.log'}})
  @{profile=('zombies-bots-'+$spec[0]);entries=$entries}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $dir 'server-manager.json')
  $fixtures+=$dir
 }
 & "$out/profile-tests.exe" (Resolve-Path tools/ServerManager/bin/Release/net48/S2xServerManager.exe).Path fixed $fixtures[0] $fixtures[1]
 if($LASTEXITCODE){throw 'Survival profile editor regression failed'}
 if($LiveMaster){& "$out/master-tests.exe" --smoke;if($LASTEXITCODE){throw 'Live master query did not complete; check network access.'}}
 Write-Output 'PASS Server Manager regression suite'
} finally {Pop-Location}
