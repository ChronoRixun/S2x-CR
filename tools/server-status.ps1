<#
.SYNOPSIS
    Keeps a Discord embed up to date with the live state of the S2x dedicated servers on a box.

.DESCRIPTION
    Works out which servers to report: every port passed in, plus every s2x\server-<port>.cfg
    the launcher wrote in the game folder. Queries each one the way the game client does (OOB
    "s2x_getInfo" with the S2 packet trailer), asks the master server which of them it lists,
    and edits an existing bot embed: a "Status" field with the count, then one field per
    server, named after it with the colour codes stripped, carrying map and mode, players, the
    rotation from the launcher's config and whether the server browser has it.

    A server the launcher started that has stopped answering stays on the card in red until it
    is stopped from its launcher window (which removes its pid file); one stopped that way just
    leaves the card. -NameFilter is a regex on the raw sv_hostname, colour codes included, so
    "^\^1CR's" reports only the servers whose name starts with a red CR's.

    Every field whose name does not start with a status light, and the title, description,
    colour and footer, are left exactly as they are, so the static parts of the card can still
    be edited by hand.

    Meant to run on the server box itself on a schedule (see -Install). The bot token is read
    from the DISCORD_TOKEN environment variable or from -TokenFile; it is never logged.

.EXAMPLE
    # One update, print the fields instead of sending them
    powershell -ExecutionPolicy Bypass -File s2x\tools\server-status.ps1 -PublicAddress <public ip> -DryRun

.EXAMPLE
    # Register a scheduled task that updates the card every 2 minutes with every server the
    # launcher runs from this folder whose name starts with a red CR's
    powershell -ExecutionPolicy Bypass -File s2x\tools\server-status.ps1 `
        -ChannelId 1551114367171297290 -MessageId 1551427470597689389 `
        -PublicAddress <public ip> -NameFilter "^\^1CR's" -TokenFile C:\s2x-status\token.txt -Install
#>
param(
    # Address to query. Defaults to the host part of -PublicAddress, else 127.0.0.1.
    [string]$ServerHost = '',
    [int]$Port = 27016,
    # More ports to query, comma-separated, ranges allowed: "27015,27017-27019".
    [string]$Ports = '',
    # Game folder whose s2x\server-<port>.cfg files name the launcher's servers. Defaults to
    # the game folder this script is installed in (<game>\s2x\tools\server-status.ps1).
    [string]$GameDir = '',
    # Regex on the raw server name, colour codes included. Empty reports every server found.
    [string]$NameFilter = '',
    [string]$ChannelId = '',
    [string]$MessageId = '',
    # The public IP the master lists this box under, with or without :port. Empty skips the check.
    [string]$PublicAddress = '',
    [string]$TokenFile = '',
    [string]$MasterHost = 'master.s2x.dev',
    [int]$MasterPort = 20810,
    [int]$IntervalMinutes = 2,
    [string]$LogFile = (Join-Path $env:LOCALAPPDATA 's2x\server-status.log'),
    # Optional local snapshot from s2x_server_events.gsc for the server on -Port (no player addresses).
    [string]$RosterFile = '',
    [string]$PresenceStateFile = '',
    [switch]$DryRun,
    [switch]$Install,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$TaskName = 'S2x Server Status'
# Fields the card's earlier single-server layout used; still replaced so an old card converts.
$ManagedFields = @('Status', 'Now playing', 'Players', 'Server browser')
# A field whose name starts with one of these is a server entry this script wrote.
$StatusLights = @('🟢', '🔴', '🟡')

function Write-Log([string]$Text) {
    $line = '{0:yyyy-MM-dd HH:mm:ss} {1}' -f (Get-Date), $Text
    Write-Host $line
    try {
        $dir = Split-Path $LogFile -Parent
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        if ((Test-Path $LogFile) -and (Get-Item $LogFile).Length -gt 512KB) { Clear-Content $LogFile }
        Add-Content -Path $LogFile -Value $line
    } catch { }
}

function ConvertTo-PortList([string]$Text) {
    $list = @()
    foreach ($part in ($Text -split '[,\s]+')) {
        if ($part -match '^(\d+)-(\d+)$') { $list += ([int]$Matches[1])..([int]$Matches[2]) }
        elseif ($part -match '^\d+$') { $list += [int]$part }
        elseif ($part) { throw "Not a port: '$part'" }
    }
    return $list
}

$PublicHost = ''
$QueryPorts = @($Port)
if ($PublicAddress) {
    $PublicHost, $publicPort = $PublicAddress -split ':', 2
    if ($publicPort -match '^\d+$') { $QueryPorts += [int]$publicPort }
}
$QueryPorts += ConvertTo-PortList $Ports
if (-not $ServerHost) { $ServerHost = if ($PublicHost) { $PublicHost } else { '127.0.0.1' } }
if (-not $GameDir -and $PSScriptRoot -match '\\s2x\\tools$') {
    $candidate = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    if (Test-Path -LiteralPath (Join-Path $candidate 's2x.exe')) { $GameDir = $candidate }
}

# --- S2 packet trailer -------------------------------------------------------------------
# Sys_ChecksumCopy: 16-bit ones'-complement sum of big-endian words, carry folded, inverted.
function Get-S2Checksum([byte[]]$Data) {
    [uint32]$total = 0
    for ($i = 0; $i -lt $Data.Length - 1; $i += 2) { $total += ([uint32]$Data[$i] -shl 8) -bor $Data[$i + 1] }
    if ($Data.Length % 2) { $total += $Data[$Data.Length - 1] }
    while ($total -shr 16) { $total = ($total -band 0xFFFF) + ($total -shr 16) }
    return [uint16]((-bnot $total) -band 0xFFFF)
}

function Add-S2Trailer([byte[]]$Payload) {
    $sum = Get-S2Checksum $Payload
    # Third byte: (sock NS_CLIENT1 = 0) | (to.localNetID NS_SERVER = 2) << 4
    return $Payload + [byte]($sum -shr 8) + [byte]($sum -band 0xFF) + [byte]0x20
}

function Send-Udp([string]$TargetHost, [int]$TargetPort, [byte[]]$Packet, [int]$TimeoutMs = 3000) {
    $udp = New-Object System.Net.Sockets.UdpClient
    try {
        $udp.Client.ReceiveTimeout = $TimeoutMs
        [void]$udp.Send($Packet, $Packet.Length, $TargetHost, $TargetPort)
        $remote = New-Object System.Net.IPEndPoint ([System.Net.IPAddress]::Any, 0)
        $replies = @()
        # Collect every datagram that arrives before the timeout (the master may send several).
        while ($true) {
            try { $replies += ,($udp.Receive([ref]$remote)) } catch { break }
            if ($TargetPort -ne $MasterPort) { break }
            $udp.Client.ReceiveTimeout = 500   # later master datagrams follow the first closely
        }
        Write-Output -NoEnumerate $replies   # hand back the array of datagrams intact
    } finally { $udp.Close() }
}

function ConvertFrom-InfoResponse([byte[]]$Data) {
    if ($Data.Length -lt 8) { return $null }
    $text = [System.Text.Encoding]::UTF8.GetString($Data, 4, $Data.Length - 4 - 3)
    $head = "s2x_infoResponse`n"
    if (-not $text.StartsWith($head)) { return $null }
    $parts = $text.Substring($head.Length).Split('\')
    $info = @{}
    for ($i = 1; $i + 1 -lt $parts.Length; $i += 2) { $info[$parts[$i]] = $parts[$i + 1] }
    return $info
}

# One socket, one challenge per port, every request out at once, then replies are collected
# until each port has answered or the deadline passes. Returns port -> info.
function Get-ServerInfos([int[]]$PortList, [int]$TimeoutMs = 2000) {
    $results = @{}
    if (-not $PortList -or $PortList.Count -eq 0) { return $results }
    $udp = New-Object System.Net.Sockets.UdpClient
    # A port nobody listens on answers with ICMP "port unreachable", which Windows raises
    # as a ConnectionReset on the next Receive of this shared socket. SIO_UDP_CONNRESET off
    # stops that, and the catch below skips one if it still arrives.
    try { [void]$udp.Client.IOControl(-1744830452, [byte[]](0, 0, 0, 0), $null) } catch { }
    try {
        $challenges = @{}
        $started = @{}
        foreach ($p in $PortList) {
            $challenge = -join ((1..8) | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
            $payload = [byte[]](0xFF, 0xFF, 0xFF, 0xFF) + [System.Text.Encoding]::ASCII.GetBytes("s2x_getInfo $challenge")
            $packet = Add-S2Trailer $payload
            $challenges[$p] = $challenge
            $started[$p] = Get-Date
            [void]$udp.Send($packet, $packet.Length, $ServerHost, $p)
        }
        $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
        $remote = New-Object System.Net.IPEndPoint ([System.Net.IPAddress]::Any, 0)
        while ($results.Count -lt $PortList.Count) {
            $left = [int](($deadline - (Get-Date)).TotalMilliseconds)
            if ($left -le 0) { break }
            $udp.Client.ReceiveTimeout = $left
            try { [byte[]]$data = $udp.Receive([ref]$remote) }
            catch {
                $ex = $_.Exception
                while ($ex -and -not ($ex -is [System.Net.Sockets.SocketException])) { $ex = $ex.InnerException }
                if ($ex -and $ex.SocketErrorCode -eq [System.Net.Sockets.SocketError]::ConnectionReset) { continue }
                break
            }
            $info = ConvertFrom-InfoResponse $data
            $p = $remote.Port
            if ($null -eq $info -or -not $challenges.ContainsKey($p) -or $info['challenge'] -ne $challenges[$p]) { continue }
            $info['_port'] = $p
            $info['_rtt_ms'] = [math]::Round(((Get-Date) - $started[$p]).TotalMilliseconds)
            $results[$p] = $info
        }
    } finally { $udp.Close() }
    return $results
}

# Every ip:port the master lists, or $null when it did not answer.
function Get-MasterListing {
    if (-not $PublicHost) { return $null }
    $payload = [byte[]](0xFF, 0xFF, 0xFF, 0xFF) + [System.Text.Encoding]::ASCII.GetBytes('getservers S2 1')
    try { $replies = Send-Udp $MasterHost $MasterPort $payload 4000 } catch { return $null }
    if ($null -eq $replies -or $replies.Count -eq 0) { return $null }
    $head = [System.Text.Encoding]::ASCII.GetBytes('getserversResponse')
    $listed = @{}
    foreach ($data in $replies) {
        $i = 4 + $head.Length + 1
        while ($i + 7 -le $data.Length -and $data[$i] -eq 0x5C) {   # '\'
            if ($data[$i + 1] -eq 0x45 -and $data[$i + 2] -eq 0x4F -and $data[$i + 3] -eq 0x54) { break }  # EOT
            $address = '{0}.{1}.{2}.{3}:{4}' -f $data[$i + 1], $data[$i + 2], $data[$i + 3], $data[$i + 4], (([int]$data[$i + 5] -shl 8) -bor $data[$i + 6])
            $listed[$address] = $true
            $i += 7
        }
    }
    return $listed
}

# --- the launcher's servers ----------------------------------------------------------------
# The launcher writes s2x\server-<port>.cfg and s2x\server-<port>.pid for each server it
# starts and removes the pid file when it stops one, so the cfg files list the servers this
# box runs and a pid file marks one that is meant to be up.
function Get-LauncherServers {
    $servers = @{}
    if (-not $GameDir) { return $servers }
    $dir = Join-Path $GameDir 's2x'
    if (-not (Test-Path -LiteralPath $dir)) { return $servers }
    foreach ($cfg in Get-ChildItem -LiteralPath $dir -Filter 'server-*.cfg' -File -ErrorAction SilentlyContinue) {
        if ($cfg.Name -notmatch '^server-(\d+)\.cfg$') { continue }
        $p = [int]$Matches[1]
        $server = @{ port = $p; hostname = ''; rotation = @(); expected = $false; running = $false }
        foreach ($line in Get-Content -LiteralPath $cfg.FullName -Encoding UTF8 -ErrorAction SilentlyContinue) {
            if ($line -match '^\s*seta?\s+sv_hostname\s+"?([^"]*)"?') { $server.hostname = $Matches[1] }
            elseif ($line -match '^\s*seta?\s+sv_maprotation\s+"?([^"]*)"?') {
                $tokens = $Matches[1].Trim() -split '\s+'
                $gametype = ''
                for ($i = 0; $i + 1 -lt $tokens.Length; $i += 2) {
                    switch ($tokens[$i]) {
                        'gametype' { $gametype = $tokens[$i + 1] }
                        'map' { $server.rotation += @{ map = $tokens[$i + 1]; gametype = $gametype } }
                    }
                }
            }
        }
        $pidFile = Join-Path $dir "server-$p.pid"
        if (Test-Path -LiteralPath $pidFile) {
            $server.expected = $true
            $raw = Get-Content -LiteralPath $pidFile -Raw -ErrorAction SilentlyContinue
            if ($raw -match '^\s*(\d+)') {
                $proc = Get-Process -Id ([int]$Matches[1]) -ErrorAction SilentlyContinue
                $server.running = [bool]($proc -and $proc.ProcessName -eq 's2x')
            }
        }
        $servers[$p] = $server
    }
    return $servers
}

# --- presentation --------------------------------------------------------------------------
$GametypeNames = @{
    war = 'Team Deathmatch'; dom = 'Domination'; hp = 'Hardpoint'; conf = 'Kill Confirmed'
    dm = 'Free-for-All'; sd = 'Search & Destroy'; ctf = 'Capture the Flag'; gun = 'Gun Game'
    koth = 'Hardpoint'; raid = 'War'; ball = 'Gridiron'; infect = 'Infected'; demo = 'Demolition'
    zombies = 'Zombies'
}

# Same names as the launcher's pickers.
$MapNames = @{
    mp_shipment_s2 = 'Shipment 1944'; mp_d_day = 'Pointe du Hoc'; mp_aachen_v2 = 'Aachen'
    mp_carentan_s2 = 'Carentan'; mp_carentan_s2_winter = 'Winter Carentan'; mp_canon_farm = 'Gustav Cannon'
    mp_flak_tower = 'Flak Tower'; mp_forest_01 = 'Ardennes Forest'; mp_london = 'London Docks'
    mp_france_village = 'Sainte Marie du Mont'; mp_battleship_2 = 'USS Texas'; mp_gibraltar_02 = 'Gibraltar'
    mp_sandbox_01 = 'Sandbox'; mp_house = 'Groesten Haus'; mp_paris_s2 = 'Occupation'; mp_prague = 'Anthropoid'
    mp_wolfslair = 'Valkyrie'; mp_dunkirk = 'Dunkirk'; mp_egypt_02 = 'Egypt'; mp_v2_rocket_02 = 'V2'
    mp_stalingrad = 'Stalingrad'; mp_market_garden = 'Market Garden'; mp_monte_cassino_v2 = 'Monte Cassino'
    mp_tank_graveyard_2 = 'Excavation'; mp_airship = 'Airship'; mp_fuhrerbunker = 'Chancellery'
    mp_zombie_house = 'Groesten Haus'; mp_zombie_descent = 'The Final Reich'; mp_zombie_island = 'The Darkest Shore'
    mp_zombie_berlin = 'The Shadowed Throne'; mp_zombie_windmill = 'The Tortured Path: Into the Storm'
    mp_zombie_dnk = 'The Tortured Path: Across the Depths'; mp_zombie_dig_02 = 'The Tortured Path: Beyond the Veil'
    mp_zombie_nest_01 = 'The Frozen Dawn'
}

function Format-Map([string]$Map) {
    if ($MapNames.ContainsKey($Map)) { return $MapNames[$Map] }
    $name = $Map -replace '^mp_', '' -replace '_s2$', '' -replace '_', ' '
    return (Get-Culture).TextInfo.ToTitleCase($name)
}

function Format-Gametype([string]$Gametype) {
    if ($GametypeNames.ContainsKey($Gametype)) { return $GametypeNames[$Gametype] }
    return $Gametype.ToUpperInvariant()
}

function Format-ServerName([string]$Hostname) {
    $name = ($Hostname -replace '\^[0-9]', '' -replace '[\x00-\x1f\x7f]', '' -replace '\s+', ' ').Trim()
    if (-not $name) { $name = 'Server' }
    if ($name.Length -gt 200) { $name = $name.Substring(0, 200) }
    return $name
}

function Join-Names([string[]]$Items) {
    if ($Items.Count -le 1) { return ($Items -join '') }
    return (($Items[0..($Items.Count - 2)] -join ', ') + ' and ' + $Items[-1])
}

function New-ServerField($Server, $Info, $Listed, [long]$Now) {
    $hostname = if ($Info) { [string]$Info['hostname'] } elseif ($Server) { $Server.hostname } else { '' }
    $name = Format-ServerName $hostname
    if ($null -eq $Info) {
        $why = if ($Server -and $Server.running) { 'Not responding' } else { 'Not running' }
        return @{ name = "🔴 $name"; value = "$why · last checked <t:${Now}:R>"; inline = $false }
    }
    $lines = @()
    $mode = Format-Gametype $Info['gametype']
    $map = Format-Map $Info['mapname']
    $playing = if ($Info['sv_running'] -eq '1') { "$mode on $map" } else { "Lobby, next: $mode on $map" }
    $slots = [int]$Info['sv_maxclients']
    $humans = [math]::Max(0, [int]$Info['clients'] - [int]$Info['bots'])
    $people = if ($humans -eq 1) { '1 player online' } else { "$humans players online" }
    $lines += "$playing · $people, bots fill the rest of $slots"
    if ($Server -and $Server.rotation.Count -gt 0) {
        $modes = @($Server.rotation | ForEach-Object { Format-Gametype $_.gametype } | Select-Object -Unique)
        $maps = @($Server.rotation | ForEach-Object { Format-Map $_.map } | Select-Object -Unique)
        $lines += 'Rotation: {0} on {1}' -f (Join-Names $modes), (Join-Names $maps)
    }
    if ($null -ne $Listed) {
        $lines += if ($Listed) { '🟢 In the server browser' } else { '🟡 Not in the browser list right now, hit Refresh again in a minute' }
    }
    $value = $lines -join "`n"
    if ($value.Length -gt 1024) { $value = $value.Substring(0, 1021) + '...' }
    return @{ name = "🟢 $name"; value = $value; inline = $false }
}

function New-StatusField([int]$Online, [int]$Down, [long]$Now) {
    $value = if ($Online + $Down -eq 0) { "🔴 No servers found · last checked <t:${Now}:R>" }
             elseif ($Online -eq 0) { '🔴 None responding, {0} down · last checked <t:{1}:R>' -f $Down, $Now }
             elseif ($Down -eq 0) { '🟢 {0} online · updated <t:{1}:R>' -f ($(if ($Online -eq 1) { '1 server' } else { "$Online servers" })), $Now }
             else { '🟡 {0} online, {1} not responding · updated <t:{2}:R>' -f $Online, $Down, $Now }
    return @{ name = 'Status'; value = $value; inline = $false }
}

# Optional roster comparison for the server on -Port. Only fresh, running-match snapshots
# advance the baseline: an outage or map-loading gap must not announce that everyone left.
function Get-PresenceUpdate($Info) {
    if (-not $RosterFile -or $null -eq $Info -or $Info['sv_running'] -ne '1') { return $null }
    if ([StringComparer]::OrdinalIgnoreCase.Equals([IO.Path]::GetFullPath($RosterFile), [IO.Path]::GetFullPath($PresenceStateFile))) {
        throw 'Presence state must be a different file from the roster'
    }
    if (-not (Test-Path -LiteralPath $RosterFile)) { return $null }
    if ((Get-Item -LiteralPath $RosterFile).Length -gt 64KB) { throw 'Roster file exceeds 64 KiB' }
    $roster = Get-Content -LiteralPath $RosterFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $age = $now - [long]$roster.generated_utc
    if ($age -lt -30 -or $age -gt 90) { return $null }
    if ([string]$roster.mapname -ne [string]$Info['mapname']) { return $null }
    if ($null -eq $roster.players -or @($roster.players).Count -gt 64) { throw 'Invalid player roster' }
    $current = @{}
    foreach ($player in $roster.players) {
        $id = [string]$player.id
        if ($id -notmatch '^[0-9a-fA-F]{1,32}$' -or $current.ContainsKey($id)) { throw 'Invalid roster player ID' }
        $name = ([string]$player.name -replace '\^[0-9]', '' -replace '[\x00-\x1f\x7f]', '').Trim()
        if ($name.Length -gt 36) { $name = $name.Substring(0, 36) }
        if (-not $name) { $name = 'Player' }
        $current[$id] = $name
    }
    $previous = $null
    if (Test-Path -LiteralPath $PresenceStateFile) {
        if ((Get-Item -LiteralPath $PresenceStateFile).Length -gt 64KB) { throw 'Presence state exceeds 64 KiB' }
        $previous = Get-Content -LiteralPath $PresenceStateFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($null -eq $previous.players) { throw 'Invalid presence state' }
    }
    $events = @()
    if ($previous) {
        $old = @{}
        foreach ($player in $previous.players) { $old[[string]$player.id] = [string]$player.name }
        foreach ($id in @($current.Keys | Sort-Object)) {
            if (-not $old.ContainsKey($id)) { $events += @{ name = $current[$id]; kind = 'joined'; at = $now } }
        }
        foreach ($id in @($old.Keys | Sort-Object)) {
            if (-not $current.ContainsKey($id)) { $events += @{ name = $old[$id]; kind = 'left'; at = $now } }
        }
        $events += @($previous.events | Where-Object { $null -ne $_ })
    }
    $events = @($events | Select-Object -First 8)
    $lines = @($events | ForEach-Object {
        # Names are text, not Discord formatting or mentions.
        $name = [string]$_.name -replace '[\\*_~`<>|]', '\$0'
        '{0} {1} · <t:{2}:R>' -f $name, $_.kind, [long]$_.at
    })
    $value = if ($lines.Count) { $lines -join "`n" } else { 'Tracking joins and leaves from the next update.' }
    return @{
        field = @{ name = 'Recent arrivals / departures'; value = $value; inline = $false }
        state = @{
            players = @($current.Keys | Sort-Object | ForEach-Object { @{ id = $_; name = $current[$_] } })
            events = $events
        }
    }
}

function Save-PresenceState($State) {
    $path = [IO.Path]::GetFullPath($PresenceStateFile)
    $dir = Split-Path $path -Parent
    [void][IO.Directory]::CreateDirectory($dir)
    $temporary = Join-Path $dir ('.presence-' + [Guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [IO.File]::WriteAllText($temporary, ($State | ConvertTo-Json -Depth 8 -Compress), [Text.UTF8Encoding]::new($false))
        if ([IO.File]::Exists($path)) { [IO.File]::Replace($temporary, $path, [NullString]::Value) }
        else { [IO.File]::Move($temporary, $path) }
    } finally {
        if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}

# --- Discord --------------------------------------------------------------------------------
function Get-Token {
    if ($env:DISCORD_TOKEN) { return $env:DISCORD_TOKEN }
    if ($TokenFile -and (Test-Path $TokenFile)) { return (Get-Content $TokenFile -Raw).Trim() }
    throw 'No bot token: set DISCORD_TOKEN or pass -TokenFile'
}

function Invoke-Discord([string]$Method, [string]$Path, $Body) {
    $headers = @{ Authorization = "Bot $(Get-Token)"; 'User-Agent' = 'S2xServerStatus/1.0' }
    $uri = "https://discord.com/api/v10$Path"
    for ($attempt = 0; $attempt -lt 2; $attempt++) {
        try {
            if ($null -ne $Body) {
                $json = $Body | ConvertTo-Json -Depth 12 -Compress
                return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers -ContentType 'application/json' -Body ([System.Text.Encoding]::UTF8.GetBytes($json))
            }
            return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers
        } catch {
            $response = $_.Exception.Response
            if ($response -and [int]$response.StatusCode -eq 429 -and $attempt -eq 0) {
                Start-Sleep -Seconds 5; continue
            }
            throw
        }
    }
}

function Test-ManagedField([string]$Name) {
    if ($ManagedFields -contains $Name) { return $true }
    foreach ($light in $StatusLights) { if ($Name.StartsWith($light)) { return $true } }
    return $false
}

function Get-EmbedLength($Embed, $Fields) {
    $length = ([string]$Embed['title']).Length + ([string]$Embed['description']).Length
    if ($Embed['footer']) { $length += ([string]$Embed['footer'].text).Length }
    foreach ($field in $Fields) { $length += ([string]$field.name).Length + ([string]$field.value).Length }
    return $length
}

function Merge-Embed($Existing, $StatusFields) {
    $embed = [ordered]@{}
    foreach ($key in 'title', 'description', 'color', 'footer', 'thumbnail', 'image', 'author', 'url') {
        if ($null -ne $Existing.$key) { $embed[$key] = $Existing.$key }
    }
    $kept = @()
    if ($Existing.fields) {
        $updatedNames = @($StatusFields | ForEach-Object { $_.name })
        $kept = @($Existing.fields | Where-Object { -not (Test-ManagedField $_.name) -and $updatedNames -notcontains $_.name } |
            ForEach-Object { @{ name = $_.name; value = $_.value; inline = [bool]$_.inline } })
    }
    # Discord takes at most 25 fields and 6000 characters per embed. When the box runs
    # more servers than fit, the last entries are left off and the Status line says so.
    $list = [System.Collections.ArrayList]::new()
    foreach ($field in @($StatusFields) + $kept) { [void]$list.Add($field) }
    $dropped = 0
    while ($list.Count -gt 25 -or (Get-EmbedLength $embed $list) -gt 5900) {
        $last = -1
        for ($i = $list.Count - 1; $i -ge 0; $i--) {
            if ($list[$i].name -ne 'Status' -and (Test-ManagedField $list[$i].name)) { $last = $i; break }
        }
        if ($last -lt 0) { break }
        $list.RemoveAt($last)
        $dropped++
    }
    if ($dropped -gt 0) {
        foreach ($field in $list) { if ($field.name -eq 'Status') { $field.value += " · $dropped more not shown" } }
    }
    $embed['fields'] = @($list.ToArray())
    return $embed
}

function Update-Card {
    $launcher = @{}
    try { $launcher = Get-LauncherServers } catch { Write-Log "launcher config: $($_.Exception.Message)" }
    $ports = @(@($QueryPorts) + @($launcher.Keys) | Sort-Object -Unique)
    $infos = @{}
    try { $infos = Get-ServerInfos $ports } catch { Write-Log "query failed: $($_.Exception.Message)" }
    $listed = $null
    try { $listed = Get-MasterListing } catch { Write-Log "master check failed: $($_.Exception.Message)" }

    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $serverFields = @()
    $online = 0
    $down = 0
    foreach ($p in $ports) {
        $info = $infos[$p]
        $server = $launcher[$p]
        # Unknown ports that do not answer are not servers; a launcher server with a pid file is.
        if ($null -eq $info -and -not ($server -and $server.expected)) { continue }
        $hostname = if ($info) { [string]$info['hostname'] } else { $server.hostname }
        if ($NameFilter -and $hostname -notmatch $NameFilter) { continue }
        $isListed = if ($null -eq $listed) { $null } else { $listed.ContainsKey("${PublicHost}:$p") }
        $serverFields += New-ServerField $server $info $isListed $now
        if ($info) { $online++ } else { $down++ }
    }
    $statusFields = @(New-StatusField $online $down $now) + $serverFields
    $presence = $null
    if ($RosterFile) {
        if (-not $PresenceStateFile) { $script:PresenceStateFile = "$RosterFile.presence.json" }
        try { $presence = Get-PresenceUpdate $infos[$Port] } catch { Write-Log "presence: $($_.Exception.Message)" }
        if ($presence) { $statusFields += $presence.field }
    }
    $summary = @(foreach ($p in $ports) {
        $i = $infos[$p]
        if ($i) { '{0} {1} on {2} {3}/{4} {5} ms' -f $p, $i['gametype'], $i['mapname'], $i['clients'], $i['sv_maxclients'], $i['_rtt_ms'] } else { "$p no reply" }
    })
    $listedText = if ($null -eq $listed) { 'unknown' } else { @($ports | Where-Object { $listed.ContainsKey("${PublicHost}:$_") }) -join ',' }
    Write-Log ("servers: {0}; listed: {1}; on card: {2} online, {3} down" -f ($summary -join '; '), $listedText, $online, $down)

    if ($DryRun) {
        $preview = if ($ChannelId -and $MessageId -and ($env:DISCORD_TOKEN -or $TokenFile)) {
            Merge-Embed (Invoke-Discord GET "/channels/$ChannelId/messages/$MessageId").embeds[0] $statusFields
        } else { @{ fields = $statusFields } }
        $preview | ConvertTo-Json -Depth 12
        return
    }
    if (-not $ChannelId -or -not $MessageId) { throw 'Pass -ChannelId and -MessageId (or -DryRun)' }
    $message = Invoke-Discord GET "/channels/$ChannelId/messages/$MessageId"
    if (-not $message.embeds -or $message.embeds.Count -eq 0) { throw 'Target message has no embed to update' }
    $embed = Merge-Embed $message.embeds[0] $statusFields
    [void](Invoke-Discord PATCH "/channels/$ChannelId/messages/$MessageId" @{ embeds = @($embed); allowed_mentions = @{ parse = @() } })
    if ($presence) { Save-PresenceState $presence.state }
    Write-Log 'card updated'
}

# --- scheduled task -------------------------------------------------------------------------
function Install-Task {
    if (-not $ChannelId -or -not $MessageId) { throw '-Install needs -ChannelId and -MessageId' }
    if (-not $TokenFile -and -not $env:DISCORD_TOKEN) { throw '-Install needs -TokenFile (a scheduled task has no DISCORD_TOKEN)' }
    $script = $MyInvocation.PSCommandPath
    if (-not $script) { $script = $PSCommandPath }
    # conhost --headless runs the console host without ever creating a window; powershell.exe
    # -WindowStyle Hidden alone still flashes a console on an interactive desktop.
    $arguments = @(
        '--headless', 'powershell.exe',
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', "`"$script`"",
        '-ServerHost', $ServerHost, '-Port', $Port, '-ChannelId', $ChannelId, '-MessageId', $MessageId
    )
    if ($Ports) { $arguments += @('-Ports', "`"$Ports`"") }
    if ($GameDir) { $arguments += @('-GameDir', "`"$GameDir`"") }
    if ($NameFilter) { $arguments += @('-NameFilter', "`"$NameFilter`"") }
    if ($PublicAddress) { $arguments += @('-PublicAddress', $PublicAddress) }
    if ($TokenFile) { $arguments += @('-TokenFile', "`"$TokenFile`"") }
    if ($RosterFile) { $arguments += @('-RosterFile', "`"$RosterFile`"") }
    if ($PresenceStateFile) { $arguments += @('-PresenceStateFile', "`"$PresenceStateFile`"") }
    $action = New-ScheduledTaskAction -Execute 'conhost.exe' -Argument ($arguments -join ' ')
    $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes $IntervalMinutes)
    $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 1) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    Write-Log "scheduled task '$TaskName' registered: every $IntervalMinutes min, no window, script $script"
}

function Uninstall-Task {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
    Write-Log "scheduled task '$TaskName' removed"
}

try {
    if ($Uninstall) { Uninstall-Task; return }
    if ($Install) { Install-Task; return }
    Update-Card
} catch {
    Write-Log "error: $($_.Exception.Message)"
    exit 1
}
