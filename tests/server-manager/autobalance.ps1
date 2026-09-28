param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
# Run with Windows PowerShell -STA after building Release. Temporary folders only: no game
# folder, no real preferences or presets, and no server process is started.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$asm=[Reflection.Assembly]::LoadFrom([IO.Path]::GetFullPath($AssemblyPath))
$flags=[Reflection.BindingFlags]'Instance,Static,Public,NonPublic'
function Raw($value) { if($null -eq $value){return $null}; if($value -is [psobject]){$value=$value.PSObject.BaseObject}; return ,$value }
function TypeOf([string]$name) { $asm.GetType($name,$true) }
function New([string]$name,[object[]]$arguments=@()) { for($i=0;$i -lt $arguments.Count;$i++){$arguments[$i]=Raw $arguments[$i]}; [Activator]::CreateInstance((TypeOf $name),[object[]]$arguments) }
function Get($object,[string]$name) {
 $type=$object.GetType(); $prop=$type.GetProperty($name,$flags); if($prop){return ,$prop.GetValue($object,$null)}
 $field=$type.GetField($name,$flags); if($field){return ,$field.GetValue($object)}
 throw "Missing member $name on $($type.FullName)"
}
function Put($object,[string]$name,$value) {
 $type=$object.GetType(); $prop=$type.GetProperty($name,$flags); if($prop){$prop.SetValue($object,(Raw $value),$null);return}
 $type.GetField($name,$flags).SetValue($object,(Raw $value))
}
function Call($object,[string]$name,[object[]]$arguments=@()) { for($i=0;$i -lt $arguments.Count;$i++){$arguments[$i]=Raw $arguments[$i]}; $object.GetType().GetMethod($name,$flags).Invoke($object,$arguments) }
function CallStatic($type,[string]$name,[object[]]$arguments=@()) { for($i=0;$i -lt $arguments.Count;$i++){$arguments[$i]=Raw $arguments[$i]}; $type.GetMethod($name,$flags).Invoke($null,$arguments) }
function Require([bool]$ok,[string]$label) { if(-not $ok){throw "FAIL $label"}; Write-Output "PASS $label" }
function Same([byte[]]$a,[byte[]]$b) { [Convert]::ToBase64String($a) -eq [Convert]::ToBase64String($b) }
function Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Lines([string]$cfg) { ,($cfg -split "`n") }
function Entry([string]$map,[string]$gametype) { $entry=New 'S2x.ServerManager.Models.RotationEntry'; Put $entry 'Map' $map; Put $entry 'Gametype' $gametype; $entry }
function Preset([string]$mode,[bool]$auto,[int]$fill=12) {
 $preset=New 'S2x.ServerManager.Models.ServerPreset'
 Put $preset 'ServerName' 'Cfg test'; Put $preset 'FileName' 'cfg-test'; Put $preset 'FilePath' '(test)\cfg-test.json'
 Put $preset 'Mode' $mode; Put $preset 'BotFill' $fill; Put $preset 'AutoBalance' $auto
 if($mode -eq 'zombies'){(Get $preset 'Rotation').Add((Entry 'mp_zombie_house' 'zombies'))}else{(Get $preset 'Rotation').Add((Entry 'mp_house' 'war'))}
 $preset
}
# Everything a test run writes stays under here, and nothing else is ever deleted.
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-autobalance-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$locked=$null
try {
 $storeType=TypeOf 'S2x.ServerManager.Services.PresetStore'
 $controllerType=TypeOf 'S2x.ServerManager.Services.ServerController'
 $installerType=TypeOf 'S2x.ServerManager.Services.ServerScriptInstaller'

 # -- the preset key -----------------------------------------------------------
 $presets=Join-Path $testRoot 'presets'; New-Item -ItemType Directory -Path $presets | Out-Null
 $plainPath=Join-Path $presets 'plain.json'
 [IO.File]::WriteAllText($plainPath,'{"serverName":"Plain","mode":"mp","port":27016,"botFill":12,"rotation":[{"map":"mp_house","gametype":"war"}]}')
 $plain=CallStatic $storeType 'Read' @($plainPath)
 Require (-not (Get $plain 'AutoBalance')) 'a preset without autoBalance reads as off'
 $balancedPath=Join-Path $presets 'balanced.json'
 [IO.File]::WriteAllText($balancedPath,'{"serverName":"Balanced","mode":"mp","port":27017,"botFill":12,"autoBalance":true,"futureKey":{"nested":[1,2]},"rotation":[{"map":"mp_house","gametype":"war","futureLine":"kept"}]}')
 $balanced=CallStatic $storeType 'Read' @($balancedPath)
 Require (Get $balanced 'AutoBalance') 'autoBalance true reads as on'
 $copy=Call $balanced 'Copy'
 Require (Get $copy 'AutoBalance') 'Copy keeps auto-balance on'
 Put $copy 'AutoBalance' $false
 Require (Get $balanced 'AutoBalance') 'Copy shares nothing with the original'
 $store=New 'S2x.ServerManager.Services.PresetStore' @([string]$testRoot,[string]$presets)
 $null=Call $store 'Save' @($balanced,$false)
 $json=[IO.File]::ReadAllText($balancedPath) | ConvertFrom-Json
 Require ($json.autoBalance -eq $true) 'Save writes autoBalance true'
 Require (($json.futureKey.nested -join ',') -eq '1,2' -and $json.rotation[0].futureLine -eq 'kept') 'unknown keys survive a save, on the preset and the rotation line'
 $null=Call $store 'Save' @($copy,$false)
 $json=[IO.File]::ReadAllText($balancedPath) | ConvertFrom-Json
 Require ($json.autoBalance -eq $false -and ($json.futureKey.nested -join ',') -eq '1,2') 'Save writes autoBalance false and still keeps unknown keys'
 Require (-not (Get (CallStatic $storeType 'Read' @($balancedPath)) 'AutoBalance')) 'off survives the round trip'

 # -- the cfg -------------------------------------------------------------------
 $off=Preset 'mp' $false
 (Get $off 'ExtraLines').Add('set g_speed 190')
 $expected=@('set sv_hostname "Cfg test"','set scr_war_scorelimit 75','set scr_dom_halftime 0','set scr_dom_roundlimit 1',
  'set bot_fill 12','set bot_names nostalgia','set sv_maprotation "gametype war map mp_house"','set bot_DifficultyDefault regular',
  'set party_maxplayers 18','set party_minplayers 1','set party_matchStartDelay 60','set master_server_enable 1','set sv_lanOnly 0',
  'set s2x_autobalance 0','set g_speed 190') -join "`n"
 Require ((CallStatic $controllerType 'BuildServerCfg' @($off)) -eq $expected) 'off: bot_fill as before, s2x_autobalance 0 after the editor lines, advanced block last'
 $on=Preset 'mp' $true
 (Get $on 'ExtraLines').Add('set g_speed 190')
 $lines=Lines (CallStatic $controllerType 'BuildServerCfg' @($on))
 Require ($lines -contains 'set bot_fill 0' -and -not ($lines -contains 'set bot_fill 12')) 'on: the native bot fill is 0'
 $at=@{}; for($i=0;$i -lt $lines.Count;$i++){$at[$lines[$i]]=$i}
 Require ($at['set bot_fill 0'] -lt $at['set bot_names nostalgia'] -and $at['set bot_names nostalgia'] -lt $at['set sv_maprotation "gametype war map mp_house"']) 'on: bot_fill 0 keeps the launcher line order'
 Require ($at['set sv_lanOnly 0'] -lt $at['set s2x_autobalance 1'] -and $at['set s2x_autobalance 1'] -lt $at['set s2x_autobalance_target 12'] -and
  $at['set s2x_autobalance_target 12'] -lt $at['set scr_teambalance 0'] -and $lines[-1] -eq 'set g_speed 190') 'on: script dvars after the editor lines, advanced block still last'
 $zombies=Preset 'zombies' $true
 $text=CallStatic $controllerType 'BuildServerCfg' @($zombies)
 Require ($text -notmatch 'bot_fill|s2x_autobalance|scr_teambalance') 'Zombies: no bot fill and nothing for auto-balance'
 $profileCfg=Preset 'mp' $true
 Put $profileCfg 'LaunchProfileId' 'fixture-profile'; Put $profileCfg 'LaunchEntryKey' 'fixture-entry'
 $lines=Lines (CallStatic $controllerType 'BuildServerCfg' @($profileCfg))
 Require ($lines -contains 'set bot_fill 12' -and $lines -contains 'set s2x_autobalance 0' -and -not ($lines -contains 'set s2x_autobalance 1')) 'profile preset: auto-balance never applies'

 # -- the installer ------------------------------------------------------------
 $records=Join-Path $testRoot 'record'
 $installer=New 'S2x.ServerManager.Services.ServerScriptInstaller' @([string]$records)
 $recordPath=Get $installer 'RecordPath'
 $targets=[string[]]$installerType.GetField('AutoBalanceTargets').GetValue($null)
 $script=[byte[]](CallStatic $installerType 'AutoBalanceScript')
 $source=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot '../../tools/ServerManager/ServerScripts/s2x_autobalance.gsc'))
 Require (Same $script $source) 'the exe carries ServerScripts\s2x_autobalance.gsc byte for byte'
 $scriptHash=(New-Object Security.Cryptography.SHA256Managed).ComputeHash($script) | ForEach-Object {$_.ToString('x2')}
 $scriptHash=$scriptHash -join ''
 function Stray([string]$root,[string[]]$allowed) {
  # Anything besides the scripts themselves is a temporary file left behind.
  $keep=@($allowed | ForEach-Object {[IO.Path]::GetFullPath((Join-Path $root $_))})
  @(Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$keep -notcontains $_.FullName})
 }
 function States($result) { ((Get $result 'Targets') | ForEach-Object {(Get $_ 'State').ToString()}) -join ',' }
 function Message($result) { Get $result 'Message' }

 $fresh=Join-Path $testRoot 'game-fresh'
 $result=Call $installer 'InstallAutoBalance' @($fresh)
 Require ((States $result) -eq ((@('Installed')*$targets.Count) -join ',') -and $null -eq (Message $result)) 'fresh install writes every target and says nothing'
 foreach($target in $targets){ Require (Same ([IO.File]::ReadAllBytes((Join-Path $fresh $target))) $script) "fresh copy matches the exe: $target" }
 $record=[IO.File]::ReadAllText($recordPath) | ConvertFrom-Json
 foreach($target in $targets){ Require (@($record.PSObject.Properties[[IO.Path]::GetFullPath((Join-Path $fresh $target))].Value) -contains $scriptHash) "the record holds the written hash: $target" }
 Require ((Stray $fresh $targets).Count -eq 0 -and @(Get-ChildItem -LiteralPath $records -File).Count -eq 1) 'no temporary file left beside the scripts or the record'

 $stamps=@($targets | ForEach-Object {[IO.File]::GetLastWriteTimeUtc((Join-Path $fresh $_)).Ticks})
 $recordText=[IO.File]::ReadAllText($recordPath)
 $result=Call $installer 'InstallAutoBalance' @($fresh)
 Require ((States $result) -eq ((@('Current')*$targets.Count) -join ',') -and $null -eq (Message $result)) 'second run finds the current copy'
 Require ((($targets | ForEach-Object {[IO.File]::GetLastWriteTimeUtc((Join-Path $fresh $_)).Ticks}) -join ',') -eq ($stamps -join ',') -and [IO.File]::ReadAllText($recordPath) -eq $recordText) 'second run writes nothing'

 # An older build's copy: written through the same installer, so its hash is on record.
 $old=[Text.Encoding]::ASCII.GetBytes("// an older s2x_autobalance.gsc`r`ninit()`r`n{`r`n}`r`n")
 $upgrade=Join-Path $testRoot 'game-upgrade'
 $result=Call $installer 'Install' @($upgrade,$old,$targets)
 Require ((States $result) -eq ((@('Installed')*$targets.Count) -join ',')) 'an older copy is installed first'
 $result=Call $installer 'InstallAutoBalance' @($upgrade)
 Require ((States $result) -eq ((@('Updated')*$targets.Count) -join ',') -and $null -eq (Message $result)) 'a recorded older copy is upgraded'
 foreach($target in $targets){ Require (Same ([IO.File]::ReadAllBytes((Join-Path $upgrade $target))) $script) "upgraded copy matches the exe: $target" }
 Require ((Stray $upgrade $targets).Count -eq 0) 'upgrade leaves no temporary file'

 $custom=Join-Path $testRoot 'game-custom'
 $mine=[Text.Encoding]::ASCII.GetBytes("// edited by hand`r`n")
 foreach($target in $targets){ $path=Join-Path $custom $target; New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null; [IO.File]::WriteAllBytes($path,$mine) }
 $result=Call $installer 'InstallAutoBalance' @($custom)
 Require ((States $result) -eq ((@('Custom')*$targets.Count) -join ',')) 'an unknown copy is left alone'
 Require ((Message $result) -eq 'a custom copy of s2x_autobalance.gsc is in use') 'and it is reported as a custom copy'
 foreach($target in $targets){ Require (Same ([IO.File]::ReadAllBytes((Join-Path $custom $target))) $mine) "hand-edited copy unchanged: $target" }
 # A hash on record for one target does not make another target's copy ours.
 foreach($target in $targets){ [IO.File]::WriteAllBytes((Join-Path $custom $target),$old) }
 $result=Call $installer 'InstallAutoBalance' @($custom)
 Require ((States $result) -eq ((@('Custom')*$targets.Count) -join ',') -and (Same ([IO.File]::ReadAllBytes((Join-Path $custom $targets[0]))) $old)) 'hashes are recorded per target'
 Require ((Stray $custom $targets).Count -eq 0) 'refusal leaves no temporary file'

 # A copy identical to the exe's, with no record of it, is taken as ours.
 $adoptRecords=Join-Path $testRoot 'record-adopt'
 $adopter=New 'S2x.ServerManager.Services.ServerScriptInstaller' @([string]$adoptRecords)
 $result=Call $adopter 'InstallAutoBalance' @($fresh)
 $adopted=[IO.File]::ReadAllText((Get $adopter 'RecordPath')) | ConvertFrom-Json
 Require ((States $result) -eq ((@('Current')*$targets.Count) -join ',') -and @($adopted.PSObject.Properties).Count -eq $targets.Count) 'an identical copy with no record is current, and recorded'

 # Held open by someone else: a few retries, then the launch is told, and nothing is left behind.
 $busy=Join-Path $testRoot 'game-busy'
 $null=Call $installer 'Install' @($busy,$old,$targets)
 $busyPath=Join-Path $busy $targets[0]
 $locked=New-Object IO.FileStream($busyPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 $result=Call $installer 'InstallAutoBalance' @($busy)
 Require ((Get (Get $result 'Targets')[0] 'State').ToString() -eq 'Failed' -and (Message $result).StartsWith('could not write ' + $targets[0] + ': ')) 'a script held open is reported, not waited on forever'
 $locked.Dispose(); $locked=$null
 Require ((Same ([IO.File]::ReadAllBytes($busyPath)) $old) -and (Stray $busy $targets).Count -eq 0) 'a failed replace keeps the old copy and no temporary file'
 $result=Call $installer 'InstallAutoBalance' @($busy)
 Require ((States $result) -eq ((@('Updated')*$targets.Count) -join ',')) 'once released it is upgraded'

 # Not writable: a file where the scripts folder should be, and a folder where the script should be.
 $blocked=Join-Path $testRoot 'game-blocked'
 New-Item -ItemType Directory -Path (Join-Path $blocked 's2x') | Out-Null
 [IO.File]::WriteAllText((Join-Path $blocked 's2x\scripts'),'not a folder')
 $result=Call $installer 'InstallAutoBalance' @($blocked)
 Require ((Get $result 'Failed') -and (Message $result).StartsWith('could not write ')) 'a folder that cannot be made is reported'
 $inTheWay=Join-Path $testRoot 'game-folder'
 foreach($target in $targets){ New-Item -ItemType Directory -Force (Join-Path $inTheWay $target) | Out-Null }
 $result=Call $installer 'InstallAutoBalance' @($inTheWay)
 Require ((Get $result 'Failed') -and (Message $result) -match 'in the way') 'a folder in the way is reported'
 Require (@(Get-ChildItem -LiteralPath $inTheWay -Recurse -File).Count -eq 0) 'nothing is left behind when the write cannot happen'

 # Several targets, one of them somebody else's.
 $many=[string[]]@('s2x\scripts\mp\war\s2x_autobalance.gsc','s2x\scripts\mp\dom\s2x_autobalance.gsc','s2x\scripts\mp\hp\s2x_autobalance.gsc')
 $mixed=Join-Path $testRoot 'game-many'
 $domPath=Join-Path $mixed $many[1]; New-Item -ItemType Directory -Force (Split-Path $domPath) | Out-Null; [IO.File]::WriteAllBytes($domPath,$mine)
 $result=Call $installer 'Install' @($mixed,$script,$many)
 Require ((States $result) -eq 'Installed,Custom,Installed' -and (Message $result) -eq 'a custom copy of s2x_autobalance.gsc is in use') 'several targets: each is decided on its own'
 $result=Call $installer 'Install' @($mixed,$script,$many)
 Require ((States $result) -eq 'Current,Custom,Current' -and (Same ([IO.File]::ReadAllBytes($domPath)) $mine)) 'several targets: second run writes nothing'
 Require ((Stray $mixed $many).Count -eq 0) 'several targets: no temporary file'

 # -- the start path, short of launching anything -----------------------------
 $owners=New-Object S2x.ServerManager.Services.ManagedServerOwnershipStore((Join-Path $testRoot 'owners'))
 $startGame=Join-Path $testRoot 'game-start'; New-Item -ItemType Directory -Path $startGame | Out-Null
 $controller=New 'S2x.ServerManager.Services.ServerController' @([string]$startGame,$owners,$installer)
 foreach($case in @(@('mp',$false,$null,'off'),@('zombies',$true,$null,'Zombies'),@('mp',$true,'fixture-profile','a profile'))) {
  $snapshot=Preset $case[0] $case[1]
  if($case[2]){Put $snapshot 'LaunchProfileId' $case[2]; Put $snapshot 'LaunchEntryKey' 'fixture-entry'}
  Require ($null -eq (Call $controller 'PrepareAutoBalance' @($snapshot)) -and @(Get-ChildItem -LiteralPath $startGame -Recurse).Count -eq 0) "nothing is installed for $($case[3])"
 }
 $snapshot=Preset 'mp' $true
 Require ($null -eq (Call $controller 'PrepareAutoBalance' @($snapshot)) -and (Get $snapshot 'AutoBalance') -and (Test-Path -LiteralPath (Join-Path $startGame $targets[0]))) 'on: the script is in place before the cfg'
 $customStart=New 'S2x.ServerManager.Services.ServerController' @([string]$custom,$owners,$installer)
 $snapshot=Preset 'mp' $true
 Require ((Call $customStart 'PrepareAutoBalance' @($snapshot)) -eq 'A custom copy of s2x_autobalance.gsc is in use.' -and (Get $snapshot 'AutoBalance')) 'custom copy: the start goes ahead with auto-balance and says so'
 $failStart=New 'S2x.ServerManager.Services.ServerController' @([string]$inTheWay,$owners,$installer)
 $snapshot=Preset 'mp' $true
 $note=Call $failStart 'PrepareAutoBalance' @($snapshot)
 $lines=Lines (CallStatic $controllerType 'BuildServerCfg' @($snapshot))
 Require ($note.StartsWith('Auto-balance is not active (could not write ') -and -not (Get $snapshot 'AutoBalance')) 'failed install: the start goes ahead and says auto-balance is not active'
 Require ($lines -contains 'set bot_fill 12' -and $lines -contains 'set s2x_autobalance 0') 'failed install: that launch uses the native bot fill'

 # -- the editor ---------------------------------------------------------------
 $app=New-Object S2x.ServerManager.App
 $app.InitializeComponent()
 [S2x.ServerManager.Services.ThemeManager]::Initialize((Join-Path $testRoot 'unused-theme.json'))
 function View($editor) {
  $view=New-Object S2x.ServerManager.Views.EditorView
  $view.DataContext=$editor
  $view.Measure([Windows.Size]::new(1160,740)); $view.Arrange([Windows.Rect]::new(0,0,1160,740)); $view.UpdateLayout()
  $view
 }
 function Apply($editor) { $target=New 'S2x.ServerManager.Models.ServerPreset'; $null=Call $editor 'Apply' @($target); $target }
 $fleet=CallStatic (TypeOf 'S2x.ServerManager.ViewModels.FleetViewModel') 'DemoEditor' @('mp')
 $preset=Preset 'mp' $false 12
 $editor=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,$preset,$false)
 $view=View $editor
 $switch=$view.FindName('tglAutoBalance')
 Require ((Get $editor 'AutoBalanceEnabled') -and $switch.IsEnabled -and -not $switch.IsChecked -and -not (Get $editor 'IsDirty')) 'multiplayer: the switch is there, off and enabled'
 $switch.IsChecked=$true
 Require ((Get $editor 'AutoBalance') -and (Get $editor 'IsDirty') -and (Get (Apply $editor) 'AutoBalance')) 'switching it on marks the editor unsaved and saves on'
 Require ((Get $editor 'BotSummary').StartsWith('auto-balance 12 ') -and (Get $editor 'AutoBalanceNote') -eq 'Bots fill to the bot fill size and leave as people join; teams are kept even.') 'summary and one-line note'
 Put $editor 'BotFill' 11
 Require ((Get $editor 'AutoBalanceNote') -like '*An odd size puts one extra player on one side.') 'odd size note'
 Put $editor 'PickedGametype' ((Get $editor 'GametypeOptions') | Where-Object {(Get $_ 'Key') -eq 'dm'})
 $null=Call $editor 'Add'
 Require ((Get $editor 'AutoBalanceNote') -like '*On free-for-all maps it only keeps the player count at the bot fill size.') 'free-for-all note'
 Put $editor 'CapText' '16'
 $note=Get $editor 'AutoBalanceNote'
 Require ($note -like '*The player cap limits how many people can join; 18 lets the match grow past the bot fill size.' -and ($note -split "`n").Count -eq 2) 'player cap note, and never more than two lines'
 $card=(Get $fleet 'Servers')[0]; Put (Get $card 'Preset') 'AutoBalance' $true
 Require ((Get $card 'RotationSummary') -like '*auto-balance 12*') 'the stopped card says auto-balance'
 Put $editor 'AutoBalance' $false
 Put $editor 'BotFill' 12; Put $editor 'CapText' '18'; $null=Call $editor 'Remove' @((Get $editor 'Rotation')[1])
 Require (-not (Get $editor 'IsDirty')) 'switching it back off is no change'

 # Zombies: the section is hidden, the switch is disabled, and Save writes off.
 Put $editor 'AutoBalance' $true
 Call (Get $editor 'SetZombiesCommand') 'Execute' @($null)
 Require (-not (Get $editor 'AutoBalanceEnabled') -and -not $switch.IsEnabled -and -not (Get (Apply $editor) 'AutoBalance')) 'switching to Zombies disables it and saves off'
 $zombieFleet=CallStatic (TypeOf 'S2x.ServerManager.ViewModels.FleetViewModel') 'DemoEditor' @('zombies')
 $zombieEditor=Get $zombieFleet 'Editor'
 Put $zombieEditor 'AutoBalance' $true
 $zombieSwitch=(View $zombieEditor).FindName('tglAutoBalance')
 Require (-not $zombieSwitch.IsEnabled -and -not (Get $zombieEditor 'AutoBalance') -and -not (Get (Apply $zombieEditor) 'AutoBalance')) 'Zombies preset: disabled, and Save writes off'

 # A launch profile's server: its package writes the cfg, so the switch cannot apply.
 $profilePreset=Preset 'mp' $true
 Put $profilePreset 'LaunchProfileId' 'fixture-profile'; Put $profilePreset 'LaunchEntryKey' 'fixture-entry'
 $profileEditor=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,$profilePreset,$false)
 $profileSwitch=(View $profileEditor).FindName('tglAutoBalance')
 Put $profileEditor 'AutoBalance' $true
 Require (-not (Get $profileEditor 'AutoBalanceEnabled') -and -not $profileSwitch.IsEnabled -and -not $profileSwitch.IsChecked -and -not (Get (Apply $profileEditor) 'AutoBalance')) 'profile preset: disabled, and Save writes off'
 Write-Output 'All auto-balance checks passed.'
}
finally {
 if($locked){$locked.Dispose()}
 if($app){$app.Shutdown()}
 # Only the exact GUID test directory created above is removed.
 if([IO.Path]::GetFullPath($testRoot).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) -and [IO.Path]::GetFileName($testRoot).StartsWith('s2x-autobalance-')) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
