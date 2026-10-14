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
$raw = [byte[]](255,255,255,255) + [Text.Encoding]::UTF8.GetBytes("s2x_infoResponse`n\challenge\abc\gamename\S2\protocol\1\clients\8\bots\7\sv_running\1")
$parsed = ConvertFrom-InfoResponse $raw
Check ($parsed -and $parsed['sv_running'] -eq '1') 'Raw info must retain its final value'
$parsed = ConvertFrom-InfoResponse (Add-S2Trailer $raw)
Check ($parsed -and $parsed['sv_running'] -eq '1') 'Legacy checksum trailer must remain compatible'
$broken = Add-S2Trailer $raw; $broken[-2] = $broken[-2] -bxor 1
Check ($null -eq (ConvertFrom-InfoResponse $broken)) 'Corrupt legacy trailer must be rejected'
Check ($null -eq (ConvertFrom-InfoResponse ([byte[]](0,255,255,255) + $raw[4..($raw.Length-1)]))) 'Bad OOB header must be rejected'
# Real loopback datagrams exercise queries without Discord or a game process.
Add-Type -TypeDefinition @"
using System;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
public sealed class StatusUdpFixture : IDisposable {
    readonly UdpClient udp = new UdpClient(new IPEndPoint(IPAddress.Loopback,0));
    public int Port { get { return ((IPEndPoint)udp.Client.LocalEndPoint).Port; } }
    public Task Work;
    static byte[] Response(string challenge, bool legacy) {
        var bytes=new byte[]{255,255,255,255}.Concat(Encoding.ASCII.GetBytes("s2x_infoResponse\n\\challenge\\"+challenge+"\\gamename\\S2\\protocol\\1\\clients\\8\\bots\\7\\sv_running\\1")).ToArray();
        if(!legacy)return bytes;
        uint sum=0;int i;for(i=0;i+1<bytes.Length;i+=2)sum+=(uint)((bytes[i]<<8)|bytes[i+1]);
        if(i<bytes.Length)sum+=bytes[i];while((sum>>16)!=0)sum=(sum&65535)+(sum>>16);sum=~sum;
        return bytes.Concat(new byte[]{(byte)(sum>>8),(byte)sum,0x02}).ToArray();
    }
    public StatusUdpFixture(bool legacy) {
        udp.Client.ReceiveTimeout=4000;
        Work=Task.Run(()=>{
            var remote=new IPEndPoint(IPAddress.Any,0);var bytes=udp.Receive(ref remote);
            string text=Encoding.ASCII.GetString(bytes,4,bytes.Length-4);
            if(!text.StartsWith("s2x_getInfo ")||text.Length!=44)throw new Exception("Expected raw upstream probe");
            if(legacy){bytes=udp.Receive(ref remote);text=Encoding.ASCII.GetString(bytes,4,bytes.Length-7);}
            var challenge=text.Substring(text.IndexOf(' ')+1);
            using(var spoof=new UdpClient(new IPEndPoint(IPAddress.Loopback,0))){var forged=Response(challenge,legacy);spoof.Send(forged,forged.Length,remote);}
            var wrong=Response("wrong",legacy);udp.Send(wrong,wrong.Length,remote);
            var response=Response(challenge,legacy);udp.Send(response,response.Length,remote);
        });
    }
    public void Dispose(){udp.Close();}
}
"@
$ServerHost = '127.0.0.1'
foreach ($legacy in @($false, $true)) {
    $fixture = [StatusUdpFixture]::new($legacy)
    try {
        $queried = Get-ServerInfos @($fixture.Port) 2000
        $fixture.Work.GetAwaiter().GetResult()
        Check ($queried.Count -eq 1 -and $queried[$fixture.Port]['sv_running'] -eq '1' -and $queried[$fixture.Port]['clients'] -eq '8') 'UDP query must accept verified raw and legacy responses'
        Check ($queried[$fixture.Port]['_legacy'] -eq $legacy) 'Reply framing must be recognized'
    } finally { $fixture.Dispose() }
}
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
$info = @{ hostname = '^1Test ^7Server'; sv_running = '1'; mapname = 'mp_london'; gametype = 'gun'; clients = '8'; bots = '7'; sv_maxclients = '8'; _port = $Port; _rtt_ms = 1 }
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
# The launcher's servers: a preset has a cfg and a pid file, a launch profile's server only
# the pid file, and a stale pid (no s2x process behind it) never marks a server as expected.
$GameDir = Join-Path $testRoot 'game'
[void][IO.Directory]::CreateDirectory((Join-Path $GameDir 's2x'))
Set-Content -LiteralPath (Join-Path $GameDir 's2x\server-27030.cfg') -Encoding UTF8 -Value @('set sv_hostname "Preset"', 'set sv_maprotation "gametype gun map mp_london"')
Set-Content -LiteralPath (Join-Path $GameDir 's2x\server-27030.pid') -Value $PID
Set-Content -LiteralPath (Join-Path $GameDir 's2x\server-27031.pid') -Value $PID
Set-Content -LiteralPath (Join-Path $GameDir 's2x\server-notaport.pid') -Value $PID
$launcher = Get-LauncherServers
Check ($launcher.Count -eq 2 -and $launcher.ContainsKey(27030) -and $launcher.ContainsKey(27031)) 'Both the cfg and the pid-only server must be found'
Check ($launcher[27030].hostname -eq 'Preset' -and $launcher[27030].rotation.Count -eq 1) 'The preset keeps its cfg name and rotation'
Check ($launcher[27031].hostname -eq '' -and $launcher[27031].rotation.Count -eq 0) 'The profile server has no cfg to read'
Check (-not $launcher[27030].expected -and -not $launcher[27031].expected) 'A pid that is not an s2x process must not mark a server as expected'
$GameDir = ''
Check ((Format-Map 'mp_london_srv') -eq 'London Docks' -and (Format-Map 'mp_zombie_dig_02_srv') -eq 'Zombie Dig 02') 'A _srv map variant must be named like its base map'
$countField = New-ServerField $null $info $true ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())
Check ($countField.value.Contains('1 player')) 'Card must show clients minus bots as humans'
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
# A launch-profile server (pid file, no cfg) that is alive but not answering has no name to
# filter on; it must show red under a name filter instead of leaving the card.
$NameFilter = "^\^1Test"
function Get-LauncherServers { return @{ 27031 = @{ port = 27031; hostname = ''; rotation = @(); expected = $true; running = $true } } }
Update-Card | Out-Null
$fields = @($script:published.fields)
Check ($fields[0].value.StartsWith($StatusLights[2] + ' 1 online, 1 not responding')) 'Status must count the silent profile server as down'
Check (@($fields | Where-Object { $_.name -eq ($StatusLights[1] + ' Server on :27031') -and $_.value.StartsWith('Not responding') }).Count -eq 1) 'The silent profile server must get a red entry named by its port'
$NameFilter = ''
function Get-LauncherServers { return @{} }
$existing = @{ title = 'Keep me'; fields = @(@{ name = 'Rules'; value = 'Keep'; inline = $false }, @{ name = 'Recent arrivals / departures'; value = 'Old'; inline = $false }) }
$merged = Merge-Embed $existing @($join.field)
Check ($merged.title -eq 'Keep me' -and @($merged.fields | Where-Object name -eq 'Rules').Count -eq 1) 'Unmanaged embed fields must survive'
Check (@($merged.fields | Where-Object name -eq 'Recent arrivals / departures').Count -eq 1) 'Presence field must replace itself'
Write-Output 'PASS: raw/legacy loopback UDP, final fields, source/challenge validation, human counts, roster joins/leaves, stale/offline/map guards, bounded history, launcher cfg and pid discovery, escaping, dry run, failed/successful publication, published card layout and embed merge'
