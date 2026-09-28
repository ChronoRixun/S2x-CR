param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
# Run with Windows PowerShell -STA after building Release. Chat commands and the map vote: the
# preset groups, their cfg lines and the text sanitiser, s2x_servercmds.gsc's installer and start
# path, the script's name table against GameData and a lint of it, and the editor sections.
# Temporary folders only: no game folder, no real preferences or presets, no server process, no
# network. The editor is hosted in a plain WPF Application with the Manager's Theme.xaml, never
# the Manager's App, whose startup would run on the real game folder and settings.
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
function StaticValue($type,[string]$name) { $field=$type.GetField($name,$flags); if($field){return ,$field.GetValue($null)}; return ,$type.GetProperty($name,$flags).GetValue($null,$null) }
function Require([bool]$ok,[string]$label) { if(-not $ok){throw "FAIL $label"}; Write-Output "PASS $label" }
function Same([byte[]]$a,[byte[]]$b) { [Convert]::ToBase64String($a) -eq [Convert]::ToBase64String($b) }
function Lines([string]$cfg) { ,($cfg -split "`n") }
function Entry([string]$map,[string]$gametype) { $entry=New 'S2x.ServerManager.Models.RotationEntry'; Put $entry 'Map' $map; Put $entry 'Gametype' $gametype; $entry }
function Preset([string]$mode='mp') {
 $preset=New 'S2x.ServerManager.Models.ServerPreset'
 Put $preset 'ServerName' 'Cfg test'; Put $preset 'FileName' 'cfg-test'; Put $preset 'FilePath' '(test)\cfg-test.json'
 Put $preset 'Mode' $mode
 if($mode -eq 'zombies'){(Get $preset 'Rotation').Add((Entry 'mp_zombie_house' 'zombies'))}else{(Get $preset 'Rotation').Add((Entry 'mp_house' 'war'))}
 $preset
}
function Rules($preset,[string[]]$rules) { $list=Get $preset 'Rules'; $list.Clear(); foreach($rule in $rules){$list.Add($rule)} }
function Profiled($preset) { Put $preset 'LaunchProfileId' 'fixture-profile'; Put $preset 'LaunchEntryKey' 'fixture-entry'; $preset }
$dot=[string][char]0x00B7
# Everything a test run writes stays under here, and nothing else is ever deleted.
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-servercmds-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$app=$null
try {
 $storeType=TypeOf 'S2x.ServerManager.Services.PresetStore'
 $controllerType=TypeOf 'S2x.ServerManager.Services.ServerController'
 $installerType=TypeOf 'S2x.ServerManager.Services.ServerScriptInstaller'
 $cfgTextType=TypeOf 'S2x.ServerManager.Services.CfgText'
 $gameDataType=TypeOf 'S2x.ServerManager.Models.GameData'
 $presetType=TypeOf 'S2x.ServerManager.Models.ServerPreset'

 # -- the preset groups ----------------------------------------------------------
 $presets=Join-Path $testRoot 'presets'; New-Item -ItemType Directory -Path $presets | Out-Null
 $store=New 'S2x.ServerManager.Services.PresetStore' @([string]$testRoot,[string]$presets)
 $plainPath=Join-Path $presets 'plain.json'
 [IO.File]::WriteAllText($plainPath,'{"serverName":"Plain","mode":"mp","port":27016,"botFill":12,"rotation":[{"map":"mp_house","gametype":"war"}]}')
 $plain=CallStatic $storeType 'Read' @($plainPath)
 Require (-not (Get $plain 'ChatCommands') -and (Get $plain 'Rules').Count -eq 0 -and (Get $plain 'Discord') -eq '' -and
  -not (Get $plain 'MapVote') -and (Get $plain 'VoteChoices') -eq 3 -and (Get $plain 'VoteSeconds') -eq 15) 'a preset without the groups reads as off, no rules, no Discord, 3 choices, 15 s'
 $null=Call $store 'Save' @($plain,$false)
 $json=[IO.File]::ReadAllText($plainPath) | ConvertFrom-Json
 Require ($json.chatCommands.enabled -eq $false -and @($json.chatCommands.rules).Count -eq 0 -and $json.chatCommands.discord -eq '' -and
  $json.mapVote.enabled -eq $false -and $json.mapVote.choices -eq 3 -and $json.mapVote.seconds -eq 15) 'Save always writes both groups, off'

 $groupsPath=Join-Path $presets 'groups.json'
 [IO.File]::WriteAllText($groupsPath,'{"serverName":"Groups","mode":"mp","port":27017,"rotation":[{"map":"mp_house","gametype":"war"}],' +
  '"chatCommands":{"enabled":true,"rules":["  first  ","","second","   ","third\r\nline",5,"fifth","sixth","seventh"],"discord":"  discord.gg/abc ","futureChat":{"nested":[1,2]}},' +
  '"mapVote":{"enabled":true,"choices":9,"seconds":3,"futureVote":"kept"},"futureRoot":true}')
 $groups=CallStatic $storeType 'Read' @($groupsPath)
 Require ((Get $groups 'ChatCommands') -and ((Get $groups 'Rules') -join '|') -eq 'first|second|third line|5|fifth' -and (Get $groups 'Discord') -eq 'discord.gg/abc') 'rules: the first five that are not blank, trimmed, a line break inside one made a space'
 Require ((Get $groups 'MapVote') -and (Get $groups 'VoteChoices') -eq 5 -and (Get $groups 'VoteSeconds') -eq 10) 'choices over 5 and seconds under 10 clamp to 5 and 10'
 foreach($case in @(@('1','99',2,30),@('"4"','"20"',4,20),@('"x"','null',3,15),@('3.5','25',3,25))) {
  $path=Join-Path $presets 'clamp.json'
  [IO.File]::WriteAllText($path,'{"serverName":"Clamp","mode":"mp","port":27018,"rotation":[],"mapVote":{"enabled":true,"choices":' + $case[0] + ',"seconds":' + $case[1] + '}}')
  $read=CallStatic $storeType 'Read' @($path)
  Require ((Get $read 'VoteChoices') -eq $case[2] -and (Get $read 'VoteSeconds') -eq $case[3]) "choices $($case[0]) and seconds $($case[1]) read as $($case[2]) and $($case[3])"
 }
 $oddPath=Join-Path $presets 'odd.json'
 [IO.File]::WriteAllText($oddPath,'{"serverName":"Odd","mode":"mp","port":27019,"rotation":[],"chatCommands":true,"mapVote":"yes"}')
 $odd=CallStatic $storeType 'Read' @($oddPath)
 Require (-not (Get $odd 'ChatCommands') -and -not (Get $odd 'MapVote') -and (Get $odd 'VoteChoices') -eq 3) 'a group that is not an object reads as the defaults'
 $null=Call $store 'Save' @($odd,$false)
 $json=[IO.File]::ReadAllText($oddPath) | ConvertFrom-Json
 Require ($json.chatCommands.enabled -eq $false -and $json.mapVote.seconds -eq 15) 'and Save puts an object in its place'

 $copy=Call $groups 'Copy'
 Require ((Get $copy 'ChatCommands') -and ((Get $copy 'Rules') -join '|') -eq 'first|second|third line|5|fifth' -and (Get $copy 'Discord') -eq 'discord.gg/abc' -and
  (Get $copy 'MapVote') -and (Get $copy 'VoteChoices') -eq 5 -and (Get $copy 'VoteSeconds') -eq 10) 'Copy carries every field'
 (Get $copy 'Rules').Add('added'); (Get $copy 'Rules')[0]='changed'; Put $copy 'ChatCommands' $false; Put $copy 'MapVote' $false
 ((Get $copy 'Raw')['chatCommands'])['futureChat']=$null
 Require (((Get $groups 'Rules') -join '|') -eq 'first|second|third line|5|fifth' -and (Get $groups 'ChatCommands') -and (Get $groups 'MapVote') -and
  $null -ne ((Get $groups 'Raw')['chatCommands'])['futureChat']) 'Copy shares nothing: rules and the groups in Raw are its own'

 Rules $groups @('No camping','Be nice')
 Put $groups 'Discord' 'discord.gg/xyz'; Put $groups 'VoteChoices' 4; Put $groups 'VoteSeconds' 20
 $null=Call $store 'Save' @($groups,$false)
 $json=[IO.File]::ReadAllText($groupsPath) | ConvertFrom-Json
 Require ($json.chatCommands.enabled -eq $true -and ($json.chatCommands.rules -join '|') -eq 'No camping|Be nice' -and $json.chatCommands.discord -eq 'discord.gg/xyz' -and
  $json.mapVote.enabled -eq $true -and $json.mapVote.choices -eq 4 -and $json.mapVote.seconds -eq 20) 'Save writes the groups as edited'
 Require (($json.chatCommands.futureChat.nested -join ',') -eq '1,2' -and $json.mapVote.futureVote -eq 'kept' -and $json.futureRoot -eq $true) 'unknown keys inside the groups survive a save'
 $again=CallStatic $storeType 'Read' @($groupsPath)
 Require ((Get $again 'ChatCommands') -and ((Get $again 'Rules') -join '|') -eq 'No camping|Be nice' -and (Get $again 'VoteChoices') -eq 4 -and (Get $again 'VoteSeconds') -eq 20) 'and they read back the same'
 Rules $groups @('1','2','3','4','5','6','7'); Put $groups 'VoteChoices' 1; Put $groups 'VoteSeconds' 99
 $null=Call $store 'Save' @($groups,$false)
 $json=[IO.File]::ReadAllText($groupsPath) | ConvertFrom-Json
 Require (@($json.chatCommands.rules).Count -eq 5 -and $json.mapVote.choices -eq 2 -and $json.mapVote.seconds -eq 30) 'Save writes at most five rules and clamped numbers'

 $mp=Preset 'mp'; Put $mp 'ChatCommands' $true; Put $mp 'MapVote' $true
 Require ((Get $mp 'UsesChatCommands') -and (Get $mp 'UsesMapVote') -and (Get $mp 'UsesServerCmds')) 'multiplayer: both apply'
 $zm=Preset 'zombies'; Put $zm 'ChatCommands' $true; Put $zm 'MapVote' $true
 Require (-not (Get $zm 'UsesChatCommands') -and -not (Get $zm 'UsesMapVote') -and -not (Get $zm 'UsesServerCmds')) 'Zombies: neither applies'
 $pf=Profiled (Preset 'mp'); Put $pf 'ChatCommands' $true; Put $pf 'MapVote' $true
 Require (-not (Get $pf 'UsesChatCommands') -and -not (Get $pf 'UsesMapVote') -and -not (Get $pf 'UsesServerCmds')) 'a launch profile: neither applies'
 $one=Preset 'mp'; Put $one 'MapVote' $true
 Require (-not (Get $one 'UsesChatCommands') -and (Get $one 'UsesServerCmds')) 'the vote alone needs the script'

 # -- the cfg ---------------------------------------------------------------------
 function Cfg($preset) { CallStatic $controllerType 'BuildServerCfg' @($preset) }
 $head=@('set sv_hostname "Cfg test"','set scr_war_scorelimit 75','set scr_dom_halftime 0','set scr_dom_roundlimit 1',
  'set bot_fill 12','set bot_names nostalgia','set sv_maprotation "gametype war map mp_house"','set bot_DifficultyDefault regular',
  'set party_maxplayers 18','set party_minplayers 1','set party_matchStartDelay 60','set master_server_enable 1','set sv_lanOnly 0','set s2x_autobalance 0')
 $on=Preset 'mp'
 Put $on 'ChatCommands' $true; Put $on 'MapVote' $true; Put $on 'VoteChoices' 4; Put $on 'VoteSeconds' 20
 Rules $on @('No spawn camping',' \ ','Be nice; no "toxic" chat, 100% ^1fair')
 Put $on 'Discord' 'https://discord.gg/abc'
 (Get $on 'ExtraLines').Add('set g_speed 190')
 $expected=($head + @('set s2x_chatcmds 1','set s2x_rules1 "No spawn camping"',"set s2x_rules2 ""Be nice; no 'toxic' chat, 100% ^1fair""",'set s2x_discord "https://discord.gg/abc"',
  'set s2x_mapvote 1','set s2x_mapvote_choices 4','set s2x_mapvote_time 20','set g_speed 190')) -join "`n"
 Require ((Cfg $on) -eq $expected) 'on: after s2x_autobalance, rules packed 1..k and cleaned, Discord, the vote, then the advanced block'
 $off=Preset 'mp'
 Rules $off @('A rule'); Put $off 'Discord' 'discord.gg/abc'; Put $off 'VoteChoices' 5; (Get $off 'ExtraLines').Add('set g_speed 190')
 Require ((Cfg $off) -eq (($head + @('set s2x_chatcmds 0','set s2x_mapvote 0','set g_speed 190')) -join "`n")) 'off: the two off lines only, no rules, Discord, choices or time'
 $bare=Preset 'mp'; Put $bare 'ChatCommands' $true
 $lines=Lines (Cfg $bare)
 Require ($lines -contains 'set s2x_chatcmds 1' -and -not ($lines -match '^set s2x_(rules|discord)') -and $lines[-1] -eq 'set s2x_mapvote 0') 'chat on with no rules and no Discord: nothing empty is written'
 $voteOnly=Preset 'mp'; Put $voteOnly 'MapVote' $true; Rules $voteOnly @('Unused')
 $lines=Lines (Cfg $voteOnly)
 Require (($lines[-4..-1] -join '|') -eq 'set s2x_chatcmds 0|set s2x_mapvote 1|set s2x_mapvote_choices 3|set s2x_mapvote_time 15') 'the vote alone: chat off, then the vote with its defaults'
 $many=Preset 'mp'; Put $many 'ChatCommands' $true; Rules $many @('1','2','3','4','5','6','7')
 $lines=Lines (Cfg $many)
 Require (@($lines -match '^set s2x_rules').Count -eq 5 -and $lines -contains 'set s2x_rules5 "5"' -and -not ($lines -match '^set s2x_rules6')) 'never more than five rules'
 $clamped=Preset 'mp'; Put $clamped 'MapVote' $true; Put $clamped 'VoteChoices' 9; Put $clamped 'VoteSeconds' 3
 $lines=Lines (Cfg $clamped)
 Require ($lines -contains 'set s2x_mapvote_choices 5' -and $lines -contains 'set s2x_mapvote_time 10') 'numbers set out of range are clamped in the cfg'
 $balanced=Preset 'mp'; Put $balanced 'AutoBalance' $true; Put $balanced 'ChatCommands' $true; Put $balanced 'MapVote' $true
 $lines=Lines (Cfg $balanced)
 $at=@{}; for($i=0;$i -lt $lines.Count;$i++){$at[$lines[$i]]=$i}
 Require ($at['set scr_teambalance 0'] + 1 -eq $at['set s2x_chatcmds 1'] -and $at['set s2x_chatcmds 1'] + 1 -eq $at['set s2x_mapvote 1']) 'with auto-balance on they follow its lines'
 $override=Preset 'mp'; Put $override 'MapVote' $true; (Get $override 'ExtraLines').Add('set s2x_mapvote_time 25')
 $lines=Lines (Cfg $override)
 Require ($lines[-2] -eq 'set s2x_mapvote_time 15' -and $lines[-1] -eq 'set s2x_mapvote_time 25') 'the advanced block still comes last, so it wins'
 $text=Cfg $zm
 Require ($text -notmatch 's2x_chatcmds|s2x_rules|s2x_discord|s2x_mapvote') 'Zombies: none of these lines, even with both on'
 Rules $pf @('A rule'); Put $pf 'Discord' 'discord.gg/abc'
 $lines=Lines (Cfg $pf)
 Require ($lines -contains 'set s2x_chatcmds 0' -and $lines -contains 'set s2x_mapvote 0' -and -not ($lines -match '^set s2x_(rules|discord|mapvote_)')) 'a launch profile: only the off lines'

 # -- the sanitiser -------------------------------------------------------------
 $ruleBytes=StaticValue $cfgTextType 'RuleBytes'; $discordBytes=StaticValue $cfgTextType 'DiscordBytes'
 Require ($ruleBytes -eq 120 -and $discordBytes -eq 64) 'caps: 120 bytes a rule, 64 the Discord line'
 function Clean([string]$value,[int]$max=120) { CallStatic $cfgTextType 'Clean' @($value,$max) }
 $x=[string][char]0x2028; $y=[string][char]0x2029; $nel=[string][char]0x85; $del=[string][char]0x7F
 $e3=[string][char]0xE9; $smile=[char]::ConvertFromUtf32(0x1F600)
 # The cfg probe (B11): inside a quoted value only a quote, a line break and a backslash just
 # before the closing quote do harm; everything else arrives byte for byte.
 $cases=@(
  @('plain text','plain text','plain text passes'),
  @('say "hi"',"say 'hi'",'a double quote becomes an apostrophe'),
  @("tab`there`r`nnext","tabherenext",'C0 controls are dropped'),
  @("a${nel}b${del}c${x}d${y}e",'abcde','C1, DEL and the Unicode line and paragraph separators are dropped'),
  @('a;b;c','a;b;c','a semicolon stays'),
  @('https://discord.gg/abc','https://discord.gg/abc','a URL stays, scheme and // included'),
  @('a//b///c','a//b///c','runs of slashes stay'),
  @('100% %d %s %%','100% %d %s %%','percent signs stay'),
  @('^1Red^7 rule^','^1Red^7 rule^','colour codes stay, and so does a trailing lone ^'),
  @("caf$e3 ok $smile","caf$e3 ok $smile",'UTF-8 stays'),
  @('C:\maps\x','C:\maps\x','a backslash inside stays'),
  @('ends with\','ends with','a trailing backslash goes'),
  @('ends with\\\  ','ends with','every trailing backslash goes, with the space around it'),
  @('a\ \','a','a backslash left at the end by the trim goes too'),
  @('  padded  ','padded','the text is trimmed'),
  @('   ','','blank is nothing'),
  @(' \ ','','a lone backslash is nothing')
 )
 foreach($case in $cases){ $got=Clean $case[0]; Require ($got -eq $case[1]) "sanitiser: $($case[2]) ('$got')" }
 Require ((Clean $null) -eq '') 'sanitiser: null is nothing'
 Require ((Clean ('x'*130)) -eq ('x'*120)) 'sanitiser: a rule is cut at 120 bytes'
 Require ((Clean (('x'*119) + '\abc')) -eq ('x'*119)) 'sanitiser: a cut that leaves a backslash at the end drops it'
 Require ((Clean (('x'*119) + ' yy')) -eq ('x'*119)) 'sanitiser: a cut that leaves a space trims it'
 Require ((Clean (('x'*119) + '^1')) -eq (('x'*119) + '^')) 'sanitiser: a cut that leaves a ^ keeps it'
 Require ((Clean ('d'*70) $discordBytes) -eq ('d'*64)) 'sanitiser: the Discord line is cut at 64 bytes'
 Require ((Clean ($e3*70)) -eq ($e3*60)) 'sanitiser: two-byte letters, 60 to 120 bytes'
 Require ((Clean (('x'*118) + $smile)) -eq ('x'*118)) 'sanitiser: an emoji that would cross the cap is left out whole'
 Require ((CallStatic $cfgTextType 'Always' @("a;b//c\d`t""e\")) -eq "a;b//c\d'e\") 'Always only drops controls and turns quotes into apostrophes'
 Require ((CallStatic $cfgTextType 'Fit' @(($e3*3),5)) -eq ($e3*2)) 'Fit counts UTF-8 bytes and cuts between characters'
 Require ((CallStatic $cfgTextType 'Fit' @(('a'+$smile+'b'),4)) -eq 'a') 'Fit never splits a surrogate pair'
 Require ((CallStatic $cfgTextType 'Bytes' @(" caf$e3`t ")) -eq 5) 'Bytes counts what the cfg would get before the cut'

 # -- the embedded script ---------------------------------------------------------
 $catalog=StaticValue $installerType 'Catalog'
 $targets=[string[]]$installerType.GetField('ServerCmdsTargets').GetValue($null)
 Require (($catalog | ForEach-Object {Get $_ 'Resource'}) -contains 'S2x.ServerManager.ServerScripts.s2x_servercmds.gsc' -and
  ($targets -join '|') -eq 's2x\scripts\mp\s2x_servercmds.gsc' -and (StaticValue $installerType 'ServerCmdsResource') -eq 'S2x.ServerManager.ServerScripts.s2x_servercmds.gsc') 'the catalog carries s2x_servercmds.gsc for s2x\scripts\mp'
 foreach($item in $catalog){
  $name=Split-Path -Leaf (Get $item 'Targets')[0]
  $source=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "../../tools/ServerManager/ServerScripts/$name"))
  Require (Same ([byte[]](Call $item 'Load')) $source) "the exe carries ServerScripts\$name byte for byte"
 }
 $script=[byte[]](CallStatic $installerType 'ServerCmdsScript')
 Require ([Array]::IndexOf($script,[byte]13) -lt 0) 'the embedded script has LF line ends only, so its hash does not depend on the checkout'
 $gsc=[Text.Encoding]::UTF8.GetString($script)

 # Its name table is GameData's, in the same order: the vote and !nextmap name maps as the Manager does.
 function Table([string]$letter) {
  $pairs=New-Object System.Collections.Generic.List[string]
  foreach($m in [regex]::Matches($gsc,'(?m)^\s*' + $letter + '\["([^"]+)"\]\s*=\s*"([^"]*)";')){ $pairs.Add($m.Groups[1].Value + '=' + $m.Groups[2].Value) }
  ,$pairs
 }
 $maps=@((StaticValue $gameDataType 'Maps') | ForEach-Object {$_.Key + '=' + $_.Value})
 $modes=@((StaticValue $gameDataType 'Gametypes') | ForEach-Object {$_.Key + '=' + $_.Value})
 $gscMaps=Table 'm'; $gscModes=Table 'g'
 Require ($gscMaps.Count -eq 26 -and ($gscMaps -join '|') -eq ($maps -join '|')) "the script's map names are GameData.Maps ($($gscMaps.Count))"
 Require ($gscModes.Count -eq 9 -and ($gscModes -join '|') -eq ($modes -join '|')) "the script's mode names are GameData.Gametypes ($($gscModes.Count))"

 # A lint of what the engine's compiler and runtime are known not to take, on the code alone.
 function Code([string]$text) {
  # Comments become spaces and strings become "", line breaks kept, so positions still mean lines.
  $out=New-Object Text.StringBuilder
  $state='code'
  for($i=0;$i -lt $text.Length;$i++){
   $c=$text[$i]; $n=if($i+1 -lt $text.Length){$text[$i+1]}else{[char]0}
   switch($state){
    'code' {
     if($c -eq '/' -and $n -eq '/'){ $state='line'; $null=$out.Append(' '); continue }
     if($c -eq '/' -and $n -eq '*'){ $state='block'; $null=$out.Append(' '); continue }
     if($c -eq '"'){ $state='string'; $null=$out.Append('"'); continue }
     $null=$out.Append($c)
    }
    'line' { if($c -eq "`n"){ $state='code'; $null=$out.Append($c) } else { $null=$out.Append(' ') } }
    'block' { if($c -eq '*' -and $n -eq '/'){ $state='code'; $i++; $null=$out.Append('  ') } elseif($c -eq "`n"){ $null=$out.Append($c) } else { $null=$out.Append(' ') } }
    'string' { if($c -eq '\'){ $i++; continue }; if($c -eq '"'){ $state='code'; $null=$out.Append('"') } }
   }
  }
  $out.ToString()
 }
 function Lint([string]$text) {
  $code=Code $text
  $found=New-Object System.Collections.Generic.List[string]
  if($code -match '(?i)getarraykeys\s*\('){ $found.Add('getarraykeys(') }
  if($code.Contains('?')){ $found.Add('?') }
  if($code -match '(?i)(?<![\w.])size\b'){ $found.Add('size') }
  $function=$null; $depth=0
  foreach($line in ($code -split "`n")){
   if($depth -eq 0 -and $line -match '^\s*(\w+)\s*\('){ $function=$Matches[1] }
   if($line -match '(?i)\bsettext\s*\(' -and @('mv_text','mv_hud_result') -notcontains $function){ $found.Add("settext in $function") }
   $depth+=([regex]::Matches($line,'\{')).Count - ([regex]::Matches($line,'\}')).Count
  }
  ,$found
 }
 Require ((Lint "f()`n{`n`tx = a ? b : c;`n}") -contains '?') 'lint: a ternary is caught'
 Require ((Lint "f()`n{`n`t// what?`n`tx = `"?`";`n}").Count -eq 0) 'lint: a ? in a comment or a string is not'
 Require ((Lint "f()`n{`n`tsize = 3;`n}") -contains 'size' -and (Lint "f()`n{`n`tn = a.size;`n}").Count -eq 0) 'lint: a variable named size is caught, .size is not'
 Require ((Lint "f()`n{`n`tk = getArrayKeys(a);`n}") -contains 'getarraykeys(') 'lint: getarraykeys( is caught'
 Require ((Lint "sc_help()`n{`n`te settext(`"x`");`n}") -contains 'settext in sc_help' -and (Lint "mv_text()`n{`n`te settext(t);`n}").Count -eq 0) 'lint: settext outside mv_text and mv_hud_result is caught'
 $problems=Lint $gsc
 Require ($problems.Count -eq 0) "s2x_servercmds.gsc lints clean: no getarraykeys(, no ternary, no variable named size, settext only in mv_text and mv_hud_result $($problems -join ', ')"
 Require ((Code $gsc) -match '(?i)\bsettext\s*\(') 'the settext rule had something to look at'

 # -- the installer -----------------------------------------------------------------
 $records=Join-Path $testRoot 'record'
 $installer=New 'S2x.ServerManager.Services.ServerScriptInstaller' @([string]$records)
 $recordPath=Get $installer 'RecordPath'
 $hash=((New-Object Security.Cryptography.SHA256Managed).ComputeHash($script) | ForEach-Object {$_.ToString('x2')}) -join ''
 function States($result) { ((Get $result 'Targets') | ForEach-Object {(Get $_ 'State').ToString()}) -join ',' }
 function Message($result) { Get $result 'Message' }
 function Stray([string]$root,[string[]]$allowed) {
  $keep=@($allowed | ForEach-Object {[IO.Path]::GetFullPath((Join-Path $root $_))})
  @(Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$keep -notcontains $_.FullName})
 }
 $fresh=Join-Path $testRoot 'game-fresh'
 $result=Call $installer 'InstallServerCmds' @($fresh)
 Require ((States $result) -eq 'Installed' -and $null -eq (Message $result) -and (Same ([IO.File]::ReadAllBytes((Join-Path $fresh $targets[0]))) $script)) 'fresh: written, the exe''s copy, nothing said'
 $record=[IO.File]::ReadAllText($recordPath) | ConvertFrom-Json
 Require (@($record.PSObject.Properties[[IO.Path]::GetFullPath((Join-Path $fresh $targets[0]))].Value) -contains $hash) 'the record holds its hash'
 $stamp=[IO.File]::GetLastWriteTimeUtc((Join-Path $fresh $targets[0])).Ticks
 $result=Call $installer 'InstallServerCmds' @($fresh)
 Require ((States $result) -eq 'Current' -and [IO.File]::GetLastWriteTimeUtc((Join-Path $fresh $targets[0])).Ticks -eq $stamp) 'current: found, nothing written'
 $result=Call $installer 'InstallAutoBalance' @($fresh)
 $record=[IO.File]::ReadAllText($recordPath) | ConvertFrom-Json
 Require ((States $result) -eq 'Installed' -and @($record.PSObject.Properties).Count -eq 2 -and (Stray $fresh @($targets[0],'s2x\scripts\mp\s2x_autobalance.gsc')).Count -eq 0) 'both scripts share the record, one entry each, and no temporary file'

 $old=[Text.Encoding]::ASCII.GetBytes("// an older s2x_servercmds.gsc`ninit()`n{`n}`n")
 $upgrade=Join-Path $testRoot 'game-upgrade'
 $null=Call $installer 'Install' @($upgrade,$old,$targets)
 $result=Call $installer 'InstallServerCmds' @($upgrade)
 Require ((States $result) -eq 'Updated' -and (Same ([IO.File]::ReadAllBytes((Join-Path $upgrade $targets[0]))) $script)) 'updated: an older copy a Manager wrote is replaced'
 $custom=Join-Path $testRoot 'game-custom'
 $mine=[Text.Encoding]::ASCII.GetBytes("// edited by hand`n")
 $path=Join-Path $custom $targets[0]; New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null; [IO.File]::WriteAllBytes($path,$mine)
 $result=Call $installer 'InstallServerCmds' @($custom)
 Require ((States $result) -eq 'Custom' -and (Message $result) -eq 'a custom copy of s2x_servercmds.gsc is in use' -and (Same ([IO.File]::ReadAllBytes($path)) $mine)) 'custom: a copy nobody here wrote is left alone and reported'
 $inTheWay=Join-Path $testRoot 'game-folder'
 New-Item -ItemType Directory -Force (Join-Path $inTheWay $targets[0]) | Out-Null
 $result=Call $installer 'InstallServerCmds' @($inTheWay)
 Require ((States $result) -eq 'Failed' -and (Message $result).StartsWith('could not write ' + $targets[0] + ': ') -and (Message $result) -match 'in the way') 'failed: a folder in the way is reported'
 $result=Call $installer 'InstallAutoBalance' @($inTheWay)
 Require ((States $result) -eq 'Installed') 'one script failing does not stop the other'

 # -- the start path, short of launching anything -----------------------------------
 $owners=New-Object S2x.ServerManager.Services.ManagedServerOwnershipStore((Join-Path $testRoot 'owners'))
 function Controller([string]$game) { New 'S2x.ServerManager.Services.ServerController' @([string]$game,$owners,$installer) }
 $startGame=Join-Path $testRoot 'game-start'; New-Item -ItemType Directory -Path $startGame | Out-Null
 $controller=Controller $startGame
 foreach($case in @(@((Preset 'mp'),'off'),@($zm,'Zombies'),@($pf,'a launch profile'))){
  Require ($null -eq (Call $controller 'PrepareServerCmds' @($case[0])) -and @(Get-ChildItem -LiteralPath $startGame -Recurse).Count -eq 0) "nothing is installed for $($case[1])"
 }
 $snapshot=Preset 'mp'; Put $snapshot 'ChatCommands' $true
 Require ($null -eq (Call $controller 'PrepareServerCmds' @($snapshot)) -and (Get $snapshot 'ChatCommands') -and (Test-Path -LiteralPath (Join-Path $startGame $targets[0]))) 'chat on: the script is in place before the cfg'
 $voteGame=Join-Path $testRoot 'game-vote'; New-Item -ItemType Directory -Path $voteGame | Out-Null
 $snapshot=Preset 'mp'; Put $snapshot 'MapVote' $true
 Require ($null -eq (Call (Controller $voteGame) 'PrepareServerCmds' @($snapshot)) -and (Test-Path -LiteralPath (Join-Path $voteGame $targets[0]))) 'the vote alone installs it too'
 $snapshot=Preset 'mp'; Put $snapshot 'ChatCommands' $true; Put $snapshot 'MapVote' $true
 Require ((Call (Controller $custom) 'PrepareServerCmds' @($snapshot)) -eq 'A custom copy of s2x_servercmds.gsc is in use.' -and (Get $snapshot 'ChatCommands') -and (Get $snapshot 'MapVote')) 'custom copy: the start goes ahead with both on and says so'

 $preset=Preset 'mp'; Put $preset 'ChatCommands' $true; Put $preset 'MapVote' $true; Put $preset 'AutoBalance' $true
 $snapshot=Call $preset 'Copy'
 $note=Call (Controller $inTheWay) 'PrepareScripts' @($snapshot)
 Require ($note.StartsWith('Chat commands and the map vote are not active (could not write ' + $targets[0] + ': ') -and $note.EndsWith(').')) "failed install: the note says both are not active ($note)"
 Require (-not (Get $snapshot 'ChatCommands') -and -not (Get $snapshot 'MapVote') -and (Get $snapshot 'AutoBalance') -and (Get $preset 'ChatCommands') -and (Get $preset 'MapVote')) 'only these two go off, and only for this launch'
 $lines=Lines (Cfg $snapshot)
 Require ($lines -contains 'set s2x_chatcmds 0' -and $lines -contains 'set s2x_mapvote 0' -and $lines -contains 'set s2x_autobalance 1') 'that launch''s cfg says 0 for both and keeps auto-balance'
 foreach($case in @(@('ChatCommands','Chat commands are not active ('),@('MapVote','The map vote is not active ('))){
  $snapshot=Preset 'mp'; Put $snapshot $case[0] $true
  Require ((Call (Controller $inTheWay) 'PrepareServerCmds' @($snapshot)).StartsWith($case[1])) "the note names only what was on: $($case[1])"
 }
 $blocked=Join-Path $testRoot 'game-blocked'
 New-Item -ItemType Directory -Path (Join-Path $blocked 's2x') | Out-Null
 [IO.File]::WriteAllText((Join-Path $blocked 's2x\scripts'),'not a folder')
 $snapshot=Call $preset 'Copy'
 $note=Call (Controller $blocked) 'PrepareScripts' @($snapshot)
 Require ($note -match '^Auto-balance is not active \(.*\); bots use the normal bot fill\. Chat commands and the map vote are not active \(.*\)\.$') 'both scripts failing: one toast note, auto-balance first'
 $snapshot=Call $preset 'Copy'
 Require ($null -eq (Call (Controller $fresh) 'PrepareScripts' @($snapshot))) 'both in place: nothing to say'

 # -- the editor ---------------------------------------------------------------------
 # A plain Application with Theme.xaml, as layout-fit.ps1 does: never S2x.ServerManager.App.
 $app=New-Object System.Windows.Application
 $app.ShutdownMode='OnExplicitShutdown'
 $app.Resources.MergedDictionaries.Add([Windows.Application]::LoadComponent((New-Object Uri(('/' + $asm.GetName().Name + ';component/theme.xaml'),[UriKind]::Relative))))
 [S2x.ServerManager.Services.ThemeManager]::Initialize((Join-Path $testRoot 'never-written.json'))
 $app.Dispatcher.Invoke([Action]{}, [Windows.Threading.DispatcherPriority]::ContextIdle)
 function View($editor) {
  $view=New-Object S2x.ServerManager.Views.EditorView
  $view.DataContext=$editor
  $view.Measure([Windows.Size]::new(1160,740)); $view.Arrange([Windows.Rect]::new(0,0,1160,740)); $view.UpdateLayout()
  $view
 }
 function Apply($editor) { $target=New 'S2x.ServerManager.Models.ServerPreset'; $null=Call $editor 'Apply' @($target); $target }
 # IsVisible needs a window; a view laid out on its own is hidden when something above it is collapsed.
 function Hidden($element) {
  for($e=$element;$e -ne $null;$e=[Windows.Media.VisualTreeHelper]::GetParent($e)){ if($e.Visibility -ne [Windows.Visibility]::Visible){ return $true } }
  return $false
 }
 function Pick($editor,[string]$list,[string]$label) { $option=(Get $editor $list) | Where-Object {(Get $_ 'Label') -eq $label}; Call (Get $option 'PickCommand') 'Execute' @($null) }
 $fleetType=TypeOf 'S2x.ServerManager.ViewModels.FleetViewModel'
 $fleet=CallStatic $fleetType 'DemoEditor' @('mp')
 $editor=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,(Preset 'mp'),$false)
 $view=View $editor
 $chatSwitch=$view.FindName('tglChatCommands'); $voteSwitch=$view.FindName('tglMapVote')
 Require ((Get $editor 'ServerCmdsEnabled') -and $chatSwitch.IsEnabled -and $voteSwitch.IsEnabled -and -not $chatSwitch.IsChecked -and -not $voteSwitch.IsChecked -and -not (Hidden $chatSwitch) -and -not (Hidden $voteSwitch) -and
  -not (Get $editor 'IsDirty')) 'multiplayer: both switches there, off and enabled'
 Require ((Get $editor 'ChatSummary') -eq 'off' -and (Get $editor 'MapVoteSummary') -eq 'off' -and (Get $editor 'RulesCount') -eq '0/5') 'summaries say off, no rules'
 Require ((Get $editor 'LobbyNumber') -eq '07' -and (Get $editor 'VisibilityNumber') -eq '08' -and (Get $editor 'AdvancedNumber') -eq '09') 'multiplayer sections after them are 07, 08, 09'

 $chatSwitch.IsChecked=$true
 Require ((Get $editor 'ChatCommands') -and (Get $editor 'IsDirty') -and (Get (Apply $editor) 'ChatCommands') -and (Get $editor 'ChatSummary') -eq ('on ' + $dot + ' no rules')) 'the chat switch marks the editor unsaved and saves on'
 Put $editor 'RulesText' "  No camping  `r`n`r`nBe nice`r`n   `r`nThree`r`nFour`r`nFive`r`nSix"
 Require ((Get $editor 'RulesCount') -eq '6/5' -and (Get $editor 'RulesNote') -eq 'Only the first 5 rules are used.' -and
  ((Get (Apply $editor) 'Rules') -join '|') -eq 'No camping|Be nice|Three|Four|Five') 'rules: one per line, blank lines skipped, trimmed, the first five saved and the rest flagged'
 $cutNote='A rule is cut at 120 bytes; accented letters and emoji take 2 to 4 each.'
 Put $editor 'RulesText' ("No camping`r`n" + ('r'*121))
 Require ((Get $editor 'RulesCount') -eq '2/5' -and (Get $editor 'RulesNote') -eq $cutNote) 'a rule over 120 bytes is flagged'
 Put $editor 'RulesText' ("No camping`r`n" + ([string][char]0xE9)*61)
 $twoByte=Get $editor 'RulesNote'
 Put $editor 'RulesText' ("No camping`r`n" + ([string][char]0xE9)*60)
 Require ($twoByte -eq $cutNote -and (Get $editor 'RulesNote') -eq '') 'the note counts bytes: 61 accented letters are over, 60 are not'
 Put $editor 'RulesText' "No camping`r`nBe nice"
 Put $editor 'Discord' '  discord.gg/abc '
 Require ((Get $editor 'ChatSummary') -eq ('on ' + $dot + ' 2 rules ' + $dot + ' discord') -and (Get (Apply $editor) 'Discord') -eq 'discord.gg/abc' -and
  (Get $editor 'RulesNoteVisibility').ToString() -eq 'Collapsed') 'summary on, rules and discord; Discord saved trimmed'

 $voteSwitch.IsChecked=$true
 Pick $editor 'ChoiceOptions' '4'; Pick $editor 'TimeOptions' '20'
 $applied=Apply $editor
 Require ((Get $applied 'MapVote') -and (Get $applied 'VoteChoices') -eq 4 -and (Get $applied 'VoteSeconds') -eq 20 -and (Get $editor 'MapVoteSummary') -eq ('4 choices ' + $dot + ' 20 s')) 'the vote: switch, choices and time save, and the summary says them'
 Require (@((Get $editor 'ChoiceOptions') | Where-Object {Get $_ 'IsOn'}).Count -eq 1 -and (Get (@((Get $editor 'TimeOptions') | Where-Object {Get $_ 'IsOn'})[0]) 'Label') -eq '20') 'one segment lit in each row'
 $note=Get $editor 'MapVoteNote'
 Require ($note.Contains("Choice 1 is always the rotation's next map; a tie or no votes keeps the rotation. A voted map replaces the next rotation entry.") -and
  $note.Contains('Needs at least 3 rotation entries to offer a choice.') -and $note.Contains('Needs the S2x build with map-vote support; older builds show no vote.')) 'one rotation entry: the notes say it needs three'
 Put $editor 'PickedMap' ((Get $editor 'MapOptions') | Where-Object {(Get $_ 'Key') -eq 'mp_london'})
 Put $editor 'PickedGametype' ((Get $editor 'GametypeOptions') | Where-Object {(Get $_ 'Key') -eq 'war'}); $null=Call $editor 'Add'; $null=Call $editor 'Add'
 Require ((Get $editor 'MapVoteNote').Contains('Needs at least 3')) 'the same map and mode twice is still two entries to choose from'
 Put $editor 'PickedGametype' ((Get $editor 'GametypeOptions') | Where-Object {(Get $_ 'Key') -eq 'dom'}); $null=Call $editor 'Add'
 Require (-not (Get $editor 'MapVoteNote').Contains('Needs at least 3')) 'three different entries: that note goes'
 $card=(Get $fleet 'Servers')[0]; $cardPreset=Get $card 'Preset'
 Require (-not (Get $card 'RotationSummary').EndsWith('vote')) 'the card says nothing of a vote that is off'
 Put $cardPreset 'MapVote' $true
 Require ((Get $card 'RotationSummary').EndsWith(' ' + $dot + ' vote')) 'the card''s rotation line says vote when it is on'
 Put $cardPreset 'LaunchProfileId' 'fixture-profile'; Put $cardPreset 'LaunchEntryKey' 'fixture-entry'
 Require (-not (Get $card 'RotationSummary').EndsWith('vote')) 'but not for a profile server'
 Put $cardPreset 'MapVote' $false; Put $cardPreset 'LaunchProfileId' $null; Put $cardPreset 'LaunchEntryKey' $null

 # Every field is in the unsaved check: each change alone lights it, and undoing it clears it.
 $clean=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,(Preset 'mp'),$false)
 $steps=@(
  @({Put $clean 'ChatCommands' $true},{Put $clean 'ChatCommands' $false},'the chat switch'),
  @({Put $clean 'RulesText' 'A rule'},{Put $clean 'RulesText' ''},'the rules'),
  @({Put $clean 'Discord' 'discord.gg/x'},{Put $clean 'Discord' ''},'Discord'),
  @({Put $clean 'MapVote' $true},{Put $clean 'MapVote' $false},'the vote switch'),
  @({Pick $clean 'ChoiceOptions' '5'},{Pick $clean 'ChoiceOptions' '3'},'the choices'),
  @({Pick $clean 'TimeOptions' '30'},{Pick $clean 'TimeOptions' '15'},'the vote time')
 )
 foreach($step in $steps){
  & $step[0]; $dirty=Get $clean 'IsDirty'
  & $step[1]; $undone=-not (Get $clean 'IsDirty')
  Require ($dirty -and $undone) "unsaved marker: $($step[2])"
 }
 Put $clean 'RulesText' "1`r`n2`r`n3`r`n4`r`n5"
 $five=Call $clean 'Snapshot'
 Put $clean 'RulesText' "1`r`n2`r`n3`r`n4`r`n5`r`n6"
 Require ((Call $clean 'Snapshot') -eq $five -and ((Get (Apply $clean) 'Rules') -join '') -eq '12345' -and (Get $clean 'RulesCount') -eq '6/5') 'a sixth rule is not saved, so it is no change to the file; the counter flags it'

 # Saved through the store and read back as the file has it.
 $roundPath=Join-Path $presets 'round.json'
 $round=Apply $editor; Put $round 'FileName' 'round'; Put $round 'FilePath' $roundPath; Put $round 'Port' 27030
 $null=Call $store 'Save' @($round,$true)
 $back=CallStatic $storeType 'Read' @($roundPath)
 Require ((Get $back 'ChatCommands') -and ((Get $back 'Rules') -join '|') -eq 'No camping|Be nice' -and (Get $back 'Discord') -eq 'discord.gg/abc' -and
  (Get $back 'MapVote') -and (Get $back 'VoteChoices') -eq 4 -and (Get $back 'VoteSeconds') -eq 20) 'what the editor saves reads back the same'
 $reopened=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,$back,$false)
 Require (-not (Get $reopened 'IsDirty') -and (Get $reopened 'RulesText') -eq ("No camping" + [Environment]::NewLine + "Be nice") -and (Get $reopened 'VoteChoices') -eq 4) 'and reopens unchanged'

 # Zombies: the sections are hidden, the switches disabled, and Save writes off.
 Call (Get $editor 'SetZombiesCommand') 'Execute' @($null)
 $view.UpdateLayout()
 Require (-not (Get $editor 'ServerCmdsEnabled') -and -not $chatSwitch.IsEnabled -and -not $voteSwitch.IsEnabled -and (Hidden $chatSwitch) -and (Hidden $voteSwitch) -and
  -not (Get (Apply $editor) 'ChatCommands') -and -not (Get (Apply $editor) 'MapVote')) 'switching to Zombies hides and disables both and saves off'
 Require ((Get $editor 'LobbyNumber') -eq '03' -and (Get $editor 'VisibilityNumber') -eq '04' -and (Get $editor 'AdvancedNumber') -eq '05') 'Zombies keeps 03, 04, 05'
 Call (Get $editor 'SetMultiplayerCommand') 'Execute' @($null)
 Require ((Get $editor 'ChatCommands') -and (Get $editor 'MapVote')) 'and back to multiplayer they are as they were'
 $zombieFleet=CallStatic $fleetType 'DemoEditor' @('zombies')
 $zombieEditor=Get $zombieFleet 'Editor'
 Put $zombieEditor 'ChatCommands' $true; Put $zombieEditor 'MapVote' $true
 $zombieView=View $zombieEditor
 Require (-not $zombieView.FindName('tglChatCommands').IsEnabled -and (Hidden $zombieView.FindName('tglMapVote')) -and
  -not (Get (Apply $zombieEditor) 'ChatCommands') -and -not (Get (Apply $zombieEditor) 'MapVote')) 'a Zombies preset: hidden, disabled, and Save writes off'

 # A launch profile's server: its package writes the cfg, so neither can apply.
 $profilePreset=Profiled (Preset 'mp'); Put $profilePreset 'ChatCommands' $true; Put $profilePreset 'MapVote' $true
 $profileEditor=New 'S2x.ServerManager.ViewModels.EditorViewModel' @($fleet,$profilePreset,$false)
 $profileView=View $profileEditor
 Put $profileEditor 'ChatCommands' $true; Put $profileEditor 'MapVote' $true
 Require (-not (Get $profileEditor 'ServerCmdsEnabled') -and -not $profileView.FindName('tglChatCommands').IsEnabled -and -not $profileView.FindName('tglMapVote').IsChecked -and
  (Get $profileEditor 'ServerCmdsNoteVisibility').ToString() -eq 'Visible' -and -not (Get (Apply $profileEditor) 'ChatCommands') -and -not (Get (Apply $profileEditor) 'MapVote')) 'a profile server: disabled, says why, and Save writes off'
 Write-Output 'All chat command and map vote checks passed.'
}
finally {
 if($app){$app.Shutdown()}
 $never=Join-Path $testRoot 'never-written.json'
 $wrote=Test-Path -LiteralPath $never
 # Only the exact GUID test directory created above is removed.
 if([IO.Path]::GetFullPath($testRoot).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) -and [IO.Path]::GetFileName($testRoot).StartsWith('s2x-servercmds-')) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
 if($wrote){ throw 'The test wrote theme settings' }
}
