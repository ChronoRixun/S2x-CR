param([string]$Assembly = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
$ErrorActionPreference='Stop'
# Run with Windows PowerShell -STA (the app targets desktop .NET Framework).
Add-Type -AssemblyName PresentationFramework
$asm=[Reflection.Assembly]::LoadFrom((Resolve-Path $Assembly).Path)
$flags=[Reflection.BindingFlags]'Instance,Public,NonPublic'
function Call($object,[string]$name,[object[]]$arguments=@()) { for($i=0;$i -lt $arguments.Length;$i++){if($null -ne $arguments[$i]){$arguments[$i]=$arguments[$i].PSObject.BaseObject}}; $object.GetType().GetMethod($name,$flags).Invoke($object,$arguments) }
function Read($object,[string]$name) { $prop=$object.GetType().GetProperty($name,$flags); if(-not $prop){throw ("Missing property "+$name+" on "+$object.GetType().FullName)}; ,$prop.GetValue($object,$null) }
function NewFleet([string]$game,[string]$presets) {
 $obj=[Activator]::CreateInstance($asm.GetType('S2x.ServerManager.ViewModels.FleetViewModel'),[object[]]@($game,$presets))
 $null=Call $obj 'ReloadPresetsIfChanged'; $null=Call $obj 'Recount'; return $obj
}
function Names($fleet,[string]$property='Servers') { ((Read $fleet $property) | ForEach-Object { $_.Preset.FileName }) -join ',' }
function Check($actual,$expected,[string]$label) { if($actual -ne $expected){throw "$label expected=$expected actual=$actual"}; Write-Output "PASS $label" }
function Card($fleet,[string]$name) { (Read $fleet 'Servers') | Where-Object {$_.Preset.FileName -eq $name} }
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-card-order-'+[Guid]::NewGuid().ToString('N'))
$presets=Join-Path $testRoot 'presets'
[IO.Directory]::CreateDirectory($presets)|Out-Null
try {
 foreach($name in @('a','b','c','d')) {
  @{serverName=$name;port=27016;hidden=($name -eq 'b');rotation=@()} | ConvertTo-Json | Set-Content (Join-Path $presets ($name+'.json'))
 }
 $fleet=NewFleet $testRoot $presets
 Check (Names $fleet) 'a,b,c,d' 'default stable duplicate-port identities'
 $before=@{}; Get-ChildItem $presets -File | ForEach-Object {$before[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Check (Call $fleet 'MoveCard' @((Card $fleet 'd'),(Card $fleet 'a'),$false)) $true 'drop before'
 Check (Names $fleet) 'd,a,b,c' 'hidden retained in master order'
 Check (Names $fleet 'Shown') 'd,a,c' 'visible filtered order'
 $fleet=NewFleet $testRoot $presets
 Check (Names $fleet) 'd,a,b,c' 'restart restores order'
 $fleet.GetType().GetProperty('ShowHidden').SetValue($fleet,$true,$null)
 Check (Names $fleet 'Shown') 'd,a,b,c' 'unhide reveals retained position'
 Check (Call $fleet 'MoveCardBy' @((Card $fleet 'b'),[int]-1)) $true 'keyboard move earlier'
 Check (Names $fleet) 'd,b,a,c' 'keyboard persisted master order'
 Check (Call $fleet 'MoveCardBy' @((Card $fleet 'd'),[int]-1)) $false 'first-card bound'
 Get-ChildItem $presets -File | ForEach-Object { Check (Get-FileHash $_.FullName).Hash $before[$_.FullName] "preset unchanged $($_.Name)" }
 # New files append, existing display/port edits retain their identity.
 @{serverName='New';port=1;rotation=@()} | ConvertTo-Json | Set-Content (Join-Path $presets 'e.json')
 @{serverName='Renamed display';port=30000;rotation=@()} | ConvertTo-Json | Set-Content (Join-Path $presets 'a.json')
 Call $fleet 'ReloadPresetsIfChanged'
 Check (Names $fleet) 'd,b,a,c,e' 'new append and edited-port identity'
 [IO.File]::Delete((Join-Path $presets 'c.json'))
 Call $fleet 'ReloadPresetsIfChanged'
 Check (Names $fleet) 'd,b,a,e' 'deleted preset disappears'
 [IO.File]::Move((Join-Path $presets 'a.json'),(Join-Path $presets 'renamed-file.json'))
 Call $fleet 'ReloadPresetsIfChanged'
 Check (Names $fleet) 'd,b,e,renamed-file' 'external filename rename is a new identity'
 $other=Join-Path $testRoot 'other-presets'; [IO.Directory]::CreateDirectory($other)|Out-Null
 Copy-Item "$presets/*.json" $other
 $otherFleet=NewFleet $testRoot $other
 Check (Names $otherFleet) 'e,b,d,renamed-file' 'preset directory scope isolation'
 # Exercise the app's SavePreset/SaveCopy APIs too, not just externally discovered files.
 $apiDir=Join-Path $testRoot 'api-presets'; $api=NewFleet $testRoot $apiDir
 foreach($name in @('x','y')) {
  $preset=[Activator]::CreateInstance($asm.GetType('S2x.ServerManager.Models.ServerPreset'))
  $preset.FileName=$name; $preset.FilePath=Join-Path $apiDir ($name+'.json')
  Check (Call $api 'SavePreset' @($preset,$true)) $true "SavePreset $name"
 }
 Check (Call $api 'MoveCard' @((Card $api 'x'),(Card $api 'y'),$true)) $true 'drop after last card'
 $copy=Call (Card $api 'x').Preset 'Copy'
 $copy.FileName='z'; $copy.FilePath=Join-Path $apiDir 'z.json'
 $null=Call $api 'SaveCopy' @($copy)
 Check (Names $api) 'y,x,z' 'Save As appends without moving originals'
 $api=NewFleet $testRoot $apiDir
 Check (Names $api) 'y,x,z' 'Save As order survives restart'
 $hidden=Call (Card $api 'x').Preset 'Copy'; $hidden.Hidden=$true
 Check (Call $api 'SavePreset' @($hidden,$false)) $true 'hide through save adopts identity'
 Check (Names $api 'Shown') 'y,z' 'saved hidden card filtered'
 $hidden.Hidden=$false; $null=Call $api 'SavePreset' @($hidden,$false)
 Check (Names $api 'Shown') 'y,x,z' 'saved unhide restores position'
 $originalStore=[Activator]::CreateInstance($asm.GetType('S2x.ServerManager.Services.CardOrderStore'),[object[]]@([string]$testRoot,[string]$presets)); $order=Read $originalStore 'FilePath'
 [IO.File]::Delete($order); [IO.Directory]::CreateDirectory($order)|Out-Null
 $orderBefore=Names $fleet
 Check (Call $fleet 'MoveCard' @((Card $fleet 'e'),(Card $fleet 'd'),$false)) $false 'write failure reported'
 Check (Names $fleet) $orderBefore 'write failure leaves UI unchanged'
 if(-not (Read $fleet 'ToastText').StartsWith('Could not save card order:')){throw 'Missing persistence error toast'}
 [IO.Directory]::Delete($order); [IO.File]::WriteAllText($order,'not valid json')
 $recovered=NewFleet $testRoot $presets
 Check (Names $recovered) 'e,b,d,renamed-file' 'corrupt preference safe fallback'
 Write-Output 'All actual FleetViewModel reorder and persistence checks passed.'
} finally {
 # Only the exact GUID test directory created above is removed.
 if([IO.Path]::GetFullPath($testRoot).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) -and [IO.Path]::GetFileName($testRoot).StartsWith('s2x-card-order-')) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}




