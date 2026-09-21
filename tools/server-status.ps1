<#
.SYNOPSIS
    Keeps a Discord embed up to date with the live state of an S2x dedicated server.

.DESCRIPTION
    Queries the server the way the game client does (OOB "s2x_getInfo" with the S2 packet
    trailer), asks the master server whether the address is listed, and edits the fields
    "Status", "Now playing", "Players" and "Server browser" on an existing bot embed.
    Every other field, the title, description, colour and footer are left exactly as they are,
    so the static parts of the card can still be edited by hand.

    Meant to run on the server box itself on a schedule (see -Install). The bot token is read
    from the DISCORD_TOKEN environment variable or from -TokenFile; it is never logged.

.EXAMPLE
    # One update, print the embed instead of sending it
    powershell -ExecutionPolicy Bypass -File s2x\tools\server-status.ps1 -DryRun

.EXAMPLE
    # Register a scheduled task that updates the card every 2 minutes
    powershell -ExecutionPolicy Bypass -File s2x\tools\server-status.ps1 `
        -ChannelId 1551114367171297290 -MessageId 1551427470597689389 `
        -PublicAddress <public ip>:27016 -TokenFile C:\s2x-status\token.txt -Install
#>
param(
    # Address to query. Defaults to the host part of -PublicAddress, else 127.0.0.1.
    [string]$ServerHost = '',
    [int]$Port = 27016,
    [string]$ChannelId = '',
    [string]$MessageId = '',
    # The address the master lists for this server (public IP:port). Empty skips the check.
    [string]$PublicAddress = '',
    [string]$TokenFile = '',
    [string]$MasterHost = 'master.s2x.dev',
    [int]$MasterPort = 20810,
    [int]$IntervalMinutes = 2,
    [string]$LogFile = (Join-Path $env:LOCALAPPDATA 's2x\server-status.log'),
    # Optional local snapshot from s2x_server_events.gsc (no player addresses).
    [string]$RosterFile = '',
    [string]$PresenceStateFile = '',
    [switch]$DryRun,
    [switch]$Install,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$TaskName = 'S2x Server Status'
if (-not $ServerHost) { $ServerHost = if ($PublicAddress) { ($PublicAddress -split ':')[0] } else { '127.0.0.1' } }
$ManagedFields = @('Status', 'Now playing', 'Players', 'Server browser')

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

function Get-ServerInfo {
    $challenge = -join ((1..8) | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
    $payload = [byte[]](0xFF, 0xFF, 0xFF, 0xFF) + [System.Text.Encoding]::ASCII.GetBytes("s2x_getInfo $challenge")
    $started = Get-Date
    $replies = Send-Udp $ServerHost $Port (Add-S2Trailer $payload)
    if ($null -eq $replies -or $replies.Count -eq 0) { return $null }
    [byte[]]$data = $replies[0]
    $head = [System.Text.Encoding]::ASCII.GetBytes("s2x_infoResponse`n")
    $text = [System.Text.Encoding]::UTF8.GetString($data, 4, $data.Length - 4 - 3)
    if (-not $text.StartsWith("s2x_infoResponse`n")) { return $null }
    $parts = $text.Substring($head.Length).Split('\')
    $info = @{}
    for ($i = 1; $i + 1 -lt $parts.Length; $i += 2) { $info[$parts[$i]] = $parts[$i + 1] }
    if ($info['challenge'] -ne $challenge) { return $null }
    $info['_rtt_ms'] = [math]::Round(((Get-Date) - $started).TotalMilliseconds)
    return $info
}

function Test-MasterListing {
    if (-not $PublicAddress) { return $null }
    $payload = [byte[]](0xFF, 0xFF, 0xFF, 0xFF) + [System.Text.Encoding]::ASCII.GetBytes('getservers S2 1')
    try { $replies = Send-Udp $MasterHost $MasterPort $payload 4000 } catch { return $null }
    if ($null -eq $replies -or $replies.Count -eq 0) { return $null }
    $head = [System.Text.Encoding]::ASCII.GetBytes('getserversResponse')
    foreach ($data in $replies) {
        $i = 4 + $head.Length + 1
        while ($i + 7 -le $data.Length -and $data[$i] -eq 0x5C) {   # '\'
            if ($data[$i + 1] -eq 0x45 -and $data[$i + 2] -eq 0x4F -and $data[$i + 3] -eq 0x54) { break }  # EOT
            $address = '{0}.{1}.{2}.{3}:{4}' -f $data[$i + 1], $data[$i + 2], $data[$i + 3], $data[$i + 4], (([int]$data[$i + 5] -shl 8) -bor $data[$i + 6])
            if ($address -eq $PublicAddress) { return $true }
            $i += 7
        }
    }
    return $false
}

# --- presentation --------------------------------------------------------------------------
$GametypeNames = @{
    war = 'Team Deathmatch'; dom = 'Domination'; hp = 'Hardpoint'; conf = 'Kill Confirmed'
    dm = 'Free-for-All'; sd = 'Search & Destroy'; ctf = 'Capture the Flag'; gun = 'Gun Game'
    koth = 'Hardpoint'; raid = 'War'; ball = 'Gridiron'; infect = 'Infected'; demo = 'Demolition'
}

function Format-Map([string]$Map) {
    $name = $Map -replace '^mp_', '' -replace '_s2$', '' -replace '_', ' '
    return (Get-Culture).TextInfo.ToTitleCase($name)
}

function Format-Gametype([string]$Gametype) {
    if ($GametypeNames.ContainsKey($Gametype)) { return $GametypeNames[$Gametype] }
    return $Gametype.ToUpperInvariant()
}

function New-StatusFields($Info, $Listed) {
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $fields = @()
    if ($null -eq $Info) {
        $fields += @{ name = 'Status'; value = "🔴 Not responding · last checked <t:${now}:R>"; inline = $false }
        $fields += @{ name = 'Now playing'; value = '—'; inline = $true }
        $fields += @{ name = 'Players'; value = '—'; inline = $true }
    } else {
        $fields += @{ name = 'Status'; value = "🟢 Online · updated <t:${now}:R>"; inline = $false }
        $playing = if ($Info['sv_running'] -eq '1') {
            '{0} on {1}' -f (Format-Gametype $Info['gametype']), (Format-Map $Info['mapname'])
        } else { 'Lobby, next: {0} on {1}' -f (Format-Gametype $Info['gametype']), (Format-Map $Info['mapname']) }
        $fields += @{ name = 'Now playing'; value = $playing; inline = $true }
        $humans = [int]$Info['clients']
        $slots = [int]$Info['sv_maxclients']
        $people = if ($humans -eq 1) { '1 player online' } else { "$humans players online" }
        $fields += @{ name = 'Players'; value = "$people · bots fill the rest of $slots"; inline = $true }
    }
    if ($null -ne $Listed) {
        $value = if ($Listed) { '🟢 Listed, find it under Find Match > Server Browser' }
                 else { '🟡 Not in the list right now, hit Refresh again in a minute' }
        $fields += @{ name = 'Server browser'; value = $value; inline = $false }
    }
    return $fields
}

# Optional roster comparison. Only fresh, running-match snapshots advance the
# baseline: an outage or map-loading gap must not announce that everyone left.
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

function Merge-Embed($Existing, $StatusFields) {
    $embed = [ordered]@{}
    foreach ($key in 'title', 'description', 'color', 'footer', 'thumbnail', 'image', 'author', 'url') {
        if ($null -ne $Existing.$key) { $embed[$key] = $Existing.$key }
    }
    $kept = @()
    if ($Existing.fields) {
        $updatedNames = @($StatusFields | ForEach-Object { $_.name })
        $kept = @($Existing.fields | Where-Object { $ManagedFields -notcontains $_.name -and $updatedNames -notcontains $_.name } |
            ForEach-Object { @{ name = $_.name; value = $_.value; inline = [bool]$_.inline } })
    }
    $embed['fields'] = @($StatusFields) + $kept
    return $embed
}

function Update-Card {
    $info = $null
    try { $info = Get-ServerInfo } catch { Write-Log "query failed: $($_.Exception.Message)" }
    $listed = $null
    try { $listed = Test-MasterListing } catch { Write-Log "master check failed: $($_.Exception.Message)" }
    $statusFields = New-StatusFields $info $listed
    $presence = $null
    if ($RosterFile) {
        if (-not $PresenceStateFile) { $script:PresenceStateFile = "$RosterFile.presence.json" }
        try { $presence = Get-PresenceUpdate $info } catch { Write-Log "presence: $($_.Exception.Message)" }
        if ($presence) { $statusFields += $presence.field }
    }
    $summary = if ($info) { '{0} on {1}, {2}/{3}, {4} ms' -f $info['gametype'], $info['mapname'], $info['clients'], $info['sv_maxclients'], $info['_rtt_ms'] } else { 'no reply' }
    Write-Log ("server: {0}; listed: {1}" -f $summary, ($(if ($null -eq $listed) { 'unknown' } else { $listed })))

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
