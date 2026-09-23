$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'tools\server-status.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
# Load function definitions only: never execute the script's Discord/scheduler entry point.
foreach ($function in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    Invoke-Expression $function.Extent.Text
}
function Check($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Write-Log([string]$Text) { }
$testRoot = Join-Path $repo ('build\tests\status-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($testRoot)
$RosterFile = Join-Path $testRoot 'roster.json'
$PresenceStateFile = Join-Path $testRoot 'state.json'
$ManagedFields = @('Status', 'Now playing', 'Players', 'Server browser')
$StatusLights = @(0x1F7E2, 0x1F534, 0x1F7E1 | ForEach-Object { [char]::ConvertFromUtf32($_) })
$EntrySpacer = "`n" + [char]0x200B
$GametypeNames = @{ gun = 'Gun Game' }
$MapNames = @{ mp_london = 'London Docks' }
# The param block never runs here, so set what the script derives from -Port 27016 -PublicAddress 203.0.113.7.
$Port = 27016
$PublicHost = '203.0.113.7'
$QueryPorts = @($Port)
$info = @{ hostname = '^1Test ^7Server'; sv_running = '1'; mapname = 'mp_london'; gametype = 'gun'; clients = '1'; bots = '7'; sv_maxclients = '8'; _port = $Port; _rtt_ms = 1 }
function Roster($Players, [long]$Age = 0, [string]$Map = 'mp_london') {
    @{ generated_utc = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $Age; mapname = $Map; players = @($Players) } |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $RosterFile -Encoding UTF8
}
Roster @(@{ id = 'abc'; name = '^1Alice' })
$first = Get-PresenceUpdate $info
Check ($first.state.events.Count -eq 0) 'Initial roster must establish a baseline'
Save-PresenceState $first.state
Roster @(@{ id = 'abc'; name = 'Alice' }, @{ id = 'def'; name = '*Bob* <@everyone>' })
$join = Get-PresenceUpdate $info
Check ($join.state.events.Count -eq 1 -and $join.state.events[0].kind -eq 'joined') 'New player should join once'
Check ($join.field.value.Contains('\*Bob\* \<@everyone\>')) 'Player formatting must be escaped'
Save-PresenceState $join.state
$unchanged = Get-PresenceUpdate $info
Check ($unchanged.state.events.Count -eq 1) 'Unchanged roster must not duplicate arrivals'
Roster @(@{ id = 'def'; name = 'Bob' })
$leave = Get-PresenceUpdate $info
Check ($leave.state.events[0].kind -eq 'left' -and $leave.state.events[0].name -eq 'Alice') 'Departure should use prior name'
Roster @() 1000
Check ($null -eq (Get-PresenceUpdate $info)) 'Stale roster must not produce departures'
Roster @() 0 'mp_other'
Check ($null -eq (Get-PresenceUpdate $info)) 'Old-map roster must not advance baseline'
Roster @()
Check ($null -eq (Get-PresenceUpdate $null)) 'Offline server must not advance baseline'
Check ($null -eq (Get-PresenceUpdate @{ sv_running = '0' })) 'Lobby must not advance baseline'
$empty = Get-PresenceUpdate $info
Check ($empty.state.players.Count -eq 0 -and $empty.state.events.Count -eq 3) 'Empty running roster represents departures'
$many = @(1..40 | ForEach-Object { @{ id = '{0:x}' -f (100 + $_); name = 'Player' + $_ } })
Roster $many
Check ((Get-PresenceUpdate $info).state.events.Count -eq 8) 'History must stay bounded'
$before = [IO.File]::ReadAllText($PresenceStateFile)
$ChannelId = ''; $MessageId = ''; $TokenFile = ''; $DryRun = $true
$script:discordCalls = 0
function Get-ServerInfos([int[]]$PortList) { return @{ $Port = $script:info } }
function Get-MasterListing { return @{ "${PublicHost}:$Port" = $true } }
function Invoke-Discord([string]$Method, [string]$Path, $Body) {
    $script:discordCalls++
    if ($Method -eq 'GET') { return @{ embeds = @(@{ title = 'Keep me'; fields = @() }) } }
    Check ($Body.allowed_mentions.parse.Count -eq 0) 'Updates must suppress mentions'
    if ($script:failPatch) { throw 'Synthetic PATCH failure' }
    $script:published = $Body.embeds[0]
}
Update-Card | Out-Null
Check ($script:discordCalls -eq 0) 'Offline dry run must not contact Discord'
Check ([IO.File]::ReadAllText($PresenceStateFile) -eq $before) 'Dry run must not consume events'
$DryRun = $false; $ChannelId = 'test'; $MessageId = 'test'; $script:failPatch = $true
$failed = $false
try { Update-Card | Out-Null } catch { $failed = $true }
Check $failed 'Synthetic update must fail'
Check ([IO.File]::ReadAllText($PresenceStateFile) -eq $before) 'Failed publication must preserve baseline'
$script:failPatch = $false
Update-Card | Out-Null
Check ([IO.File]::ReadAllText($PresenceStateFile) -ne $before) 'Successful publication must save baseline'
$fields = @($script:published.fields)
Check ($fields[0].name -eq 'Status' -and $fields[0].value.StartsWith($StatusLights[0] + ' 1 server online')) 'Status must count the answering server'
$entries = @($fields | Where-Object { $_.name.StartsWith($StatusLights[0]) })
Check ($entries.Count -eq 1 -and $entries[0].name -eq ($StatusLights[0] + ' Test Server') -and $entries[0].value.Contains('Gun Game on London Docks') -and $entries[0].value.Contains('In the server browser')) 'The server must get one green entry named after it'
Check (@($fields | Where-Object name -eq 'Recent arrivals / departures').Count -eq 1) 'The roster must add the presence field'
$existing = @{ title = 'Keep me'; fields = @(@{ name = 'Rules'; value = 'Keep'; inline = $false }, @{ name = 'Recent arrivals / departures'; value = 'Old'; inline = $false }) }
$merged = Merge-Embed $existing @($join.field)
Check ($merged.title -eq 'Keep me' -and @($merged.fields | Where-Object name -eq 'Rules').Count -eq 1) 'Unmanaged embed fields must survive'
Check (@($merged.fields | Where-Object name -eq 'Recent arrivals / departures').Count -eq 1) 'Presence field must replace itself'
Write-Output 'PASS: roster joins/leaves, stale/offline/map guards, bounded history, escaping, dry run, failed/successful publication, published card layout and embed merge'
