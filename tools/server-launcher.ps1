Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ── Game path ──────────────────────────────────────────────────────────────────
$GameDir = (Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 476600" -ErrorAction SilentlyContinue).InstallLocation
if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir "s2x.exe"))) {
    $GameDir = "D:\Program Files\Steam\steamapps\common\Call of Duty WWII"
}
if (-not (Test-Path (Join-Path $GameDir "s2x.exe"))) {
    [System.Windows.Forms.MessageBox]::Show("Could not find s2x.exe. Place this script in the game folder or install S2x.", "S2x Server Launcher", 0, 48)
    exit
}

# ── Data ───────────────────────────────────────────────────────────────────────
$Maps = [ordered]@{
    "mp_shipment_s2"    = "Shipment 1944"
    "mp_d_day"          = "Pointe du Hoc"
    "mp_aachen_v2"      = "Aachen"
    "mp_carentan_s2"    = "Carentan"
    "mp_canon_farm"     = "Gustav Cannon"
    "mp_flak_tower"     = "Flak Tower"
    "mp_forest_01"      = "Ardennes Forest"
    "mp_london"         = "London Docks"
    "mp_france_village"  = "Sainte Marie du Mont"
    "mp_battleship_2"   = "USS Texas"
    "mp_gibraltar_02"   = "Gibraltar"
    "mp_dunkirk"        = "Dunkirk"
    "mp_egypt_02"       = "Egypt"
    "mp_paris_s2"       = "Occupation"
    "mp_v2_rocket_02"   = "V2"
    "mp_stalingrad"     = "Stalingrad"
    "mp_prague"         = "Anthropoid"
    "mp_market_garden"  = "Market Garden"
}

$Gametypes = [ordered]@{
    "war"  = "Team Deathmatch"
    "dom"  = "Domination"
    "hp"   = "Hardpoint"
    "dm"   = "Free-for-All"
    "conf" = "Kill Confirmed"
    "sd"   = "Search and Destroy"
    "ctf"  = "Capture the Flag"
    "gun"  = "Gun Game"
    "ball" = "Gridiron"
}

$DefaultScoreLimits = @{
    "war"  = 75;  "dom" = 200; "hp"   = 250
    "dm"   = 30;  "conf"= 65;  "sd"   = 4
    "ctf"  = 3;   "gun" = 18;  "ball" = 28
}

$BotNamePools = @("default", "modern", "nostalgia")

# ── Form ───────────────────────────────────────────────────────────────────────
$form = New-Object System.Windows.Forms.Form
$form.Text = "S2x Dedicated Server Launcher"
$form.Size = New-Object System.Drawing.Size(720, 660)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$form.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$form.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)

$y = 12

# ── Server Name ────────────────────────────────────────────────────────────────
$lblName = New-Object System.Windows.Forms.Label
$lblName.Text = "Server Name"
$lblName.Location = New-Object System.Drawing.Point(14, $y)
$lblName.AutoSize = $true
$form.Controls.Add($lblName)

$txtName = New-Object System.Windows.Forms.TextBox
$txtName.Text = "S2x Dedicated Server"
$txtName.Location = New-Object System.Drawing.Point(140, ($y - 2))
$txtName.Size = New-Object System.Drawing.Size(550, 24)
$txtName.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$txtName.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$form.Controls.Add($txtName)
$y += 34

# ── Map Rotation ───────────────────────────────────────────────────────────────
$lblRotation = New-Object System.Windows.Forms.Label
$lblRotation.Text = "Map Rotation"
$lblRotation.Location = New-Object System.Drawing.Point(14, $y)
$lblRotation.AutoSize = $true
$form.Controls.Add($lblRotation)
$y += 22

# Add controls: map dropdown, gametype dropdown, Add button
$cmbMap = New-Object System.Windows.Forms.ComboBox
$cmbMap.Location = New-Object System.Drawing.Point(14, $y)
$cmbMap.Size = New-Object System.Drawing.Size(200, 24)
$cmbMap.DropDownStyle = "DropDownList"
$cmbMap.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$cmbMap.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
foreach ($kv in $Maps.GetEnumerator()) { [void]$cmbMap.Items.Add($kv.Value) }
$cmbMap.SelectedIndex = 0
$form.Controls.Add($cmbMap)

$cmbGametype = New-Object System.Windows.Forms.ComboBox
$cmbGametype.Location = New-Object System.Drawing.Point(222, $y)
$cmbGametype.Size = New-Object System.Drawing.Size(160, 24)
$cmbGametype.DropDownStyle = "DropDownList"
$cmbGametype.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$cmbGametype.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
foreach ($kv in $Gametypes.GetEnumerator()) { [void]$cmbGametype.Items.Add($kv.Value) }
$cmbGametype.SelectedIndex = 0
$form.Controls.Add($cmbGametype)

$btnAdd = New-Object System.Windows.Forms.Button
$btnAdd.Text = "Add to Rotation"
$btnAdd.Location = New-Object System.Drawing.Point(390, $y)
$btnAdd.Size = New-Object System.Drawing.Size(110, 26)
$btnAdd.FlatStyle = "Flat"
$btnAdd.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 50)
$form.Controls.Add($btnAdd)

$btnRemove = New-Object System.Windows.Forms.Button
$btnRemove.Text = "Remove"
$btnRemove.Location = New-Object System.Drawing.Point(508, $y)
$btnRemove.Size = New-Object System.Drawing.Size(80, 26)
$btnRemove.FlatStyle = "Flat"
$btnRemove.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 50)
$form.Controls.Add($btnRemove)

$btnClear = New-Object System.Windows.Forms.Button
$btnClear.Text = "Clear"
$btnClear.Location = New-Object System.Drawing.Point(596, $y)
$btnClear.Size = New-Object System.Drawing.Size(60, 26)
$btnClear.FlatStyle = "Flat"
$btnClear.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 50)
$form.Controls.Add($btnClear)
$y += 32

$lstRotation = New-Object System.Windows.Forms.ListBox
$lstRotation.Location = New-Object System.Drawing.Point(14, $y)
$lstRotation.Size = New-Object System.Drawing.Size(676, 120)
$lstRotation.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$lstRotation.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$lstRotation.BorderStyle = "FixedSingle"
$form.Controls.Add($lstRotation)
$y += 130

# Track internal rotation data
$script:rotationData = [System.Collections.ArrayList]::new()

$btnAdd.Add_Click({
    $mapIdx = $cmbMap.SelectedIndex
    $gtIdx = $cmbGametype.SelectedIndex
    if ($mapIdx -lt 0 -or $gtIdx -lt 0) { return }
    $mapKey = @($Maps.Keys)[$mapIdx]
    $mapName = @($Maps.Values)[$mapIdx]
    $gtKey = @($Gametypes.Keys)[$gtIdx]
    $gtName = @($Gametypes.Values)[$gtIdx]
    [void]$script:rotationData.Add(@{ map = $mapKey; gametype = $gtKey })
    [void]$lstRotation.Items.Add("$gtName on $mapName")
})

$btnRemove.Add_Click({
    $sel = $lstRotation.SelectedIndex
    if ($sel -ge 0) {
        $lstRotation.Items.RemoveAt($sel)
        $script:rotationData.RemoveAt($sel)
    }
})

$btnClear.Add_Click({
    $lstRotation.Items.Clear()
    $script:rotationData.Clear()
})

# ── Score Limits ───────────────────────────────────────────────────────────────
$grpScore = New-Object System.Windows.Forms.GroupBox
$grpScore.Text = "Score Limits"
$grpScore.Location = New-Object System.Drawing.Point(14, $y)
$grpScore.Size = New-Object System.Drawing.Size(676, 118)
$grpScore.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$form.Controls.Add($grpScore)

$scoreControls = @{}
$sx = 12; $sy = 22
$col = 0
foreach ($kv in $Gametypes.GetEnumerator()) {
    $lbl = New-Object System.Windows.Forms.Label
    $lbl.Text = $kv.Value
    $lbl.Location = New-Object System.Drawing.Point($sx, ($sy + 2))
    $lbl.Size = New-Object System.Drawing.Size(110, 18)
    $lbl.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    $grpScore.Controls.Add($lbl)

    $nud = New-Object System.Windows.Forms.NumericUpDown
    $nud.Location = New-Object System.Drawing.Point(($sx + 112), $sy)
    $nud.Size = New-Object System.Drawing.Size(60, 22)
    $nud.Minimum = 1
    $nud.Maximum = 999
    $nud.Value = $DefaultScoreLimits[$kv.Key]
    $nud.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    $nud.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
    $grpScore.Controls.Add($nud)
    $scoreControls[$kv.Key] = $nud

    $col++
    if ($col % 4 -eq 0) { $sx = 12; $sy += 28 } else { $sx += 184 }
}

# Domination halftime checkbox
$chkHalftime = New-Object System.Windows.Forms.CheckBox
$chkHalftime.Text = "Single Round Dom (no halftime)"
$chkHalftime.Location = New-Object System.Drawing.Point(($grpScore.Width - 220), 90)
$chkHalftime.AutoSize = $true
$chkHalftime.Checked = $true
$chkHalftime.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$grpScore.Controls.Add($chkHalftime)

$y += 126

# ── Bot Settings ───────────────────────────────────────────────────────────────
$lblBotFill = New-Object System.Windows.Forms.Label
$lblBotFill.Text = "Bot Fill"
$lblBotFill.Location = New-Object System.Drawing.Point(14, ($y + 2))
$lblBotFill.AutoSize = $true
$form.Controls.Add($lblBotFill)

$nudBotFill = New-Object System.Windows.Forms.NumericUpDown
$nudBotFill.Location = New-Object System.Drawing.Point(140, $y)
$nudBotFill.Size = New-Object System.Drawing.Size(60, 24)
$nudBotFill.Minimum = 0
$nudBotFill.Maximum = 18
$nudBotFill.Value = 17
$nudBotFill.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$nudBotFill.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$form.Controls.Add($nudBotFill)

$lblBotNames = New-Object System.Windows.Forms.Label
$lblBotNames.Text = "Bot Names"
$lblBotNames.Location = New-Object System.Drawing.Point(230, ($y + 2))
$lblBotNames.AutoSize = $true
$form.Controls.Add($lblBotNames)

$cmbBotNames = New-Object System.Windows.Forms.ComboBox
$cmbBotNames.Location = New-Object System.Drawing.Point(320, $y)
$cmbBotNames.Size = New-Object System.Drawing.Size(120, 24)
$cmbBotNames.DropDownStyle = "DropDownList"
$cmbBotNames.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$cmbBotNames.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
foreach ($pool in $BotNamePools) { [void]$cmbBotNames.Items.Add($pool) }
$cmbBotNames.SelectedIndex = 0
$form.Controls.Add($cmbBotNames)

$lblPort = New-Object System.Windows.Forms.Label
$lblPort.Text = "Port"
$lblPort.Location = New-Object System.Drawing.Point(470, ($y + 2))
$lblPort.AutoSize = $true
$form.Controls.Add($lblPort)

$nudPort = New-Object System.Windows.Forms.NumericUpDown
$nudPort.Location = New-Object System.Drawing.Point(510, $y)
$nudPort.Size = New-Object System.Drawing.Size(70, 24)
$nudPort.Minimum = 1024
$nudPort.Maximum = 65535
$nudPort.Value = 27016
$nudPort.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$nudPort.ForeColor = [System.Drawing.Color]::FromArgb(220, 200, 160)
$form.Controls.Add($nudPort)
$y += 40

# ── Auto-connect ───────────────────────────────────────────────────────────────
$chkConnect = New-Object System.Windows.Forms.CheckBox
$chkConnect.Text = "Auto-connect after launch (requires client running)"
$chkConnect.Location = New-Object System.Drawing.Point(14, $y)
$chkConnect.AutoSize = $true
$chkConnect.Checked = $false
$form.Controls.Add($chkConnect)
$y += 34

# ── Launch / Stop ──────────────────────────────────────────────────────────────
$btnLaunch = New-Object System.Windows.Forms.Button
$btnLaunch.Text = "Launch Server"
$btnLaunch.Location = New-Object System.Drawing.Point(14, $y)
$btnLaunch.Size = New-Object System.Drawing.Size(330, 40)
$btnLaunch.FlatStyle = "Flat"
$btnLaunch.BackColor = [System.Drawing.Color]::FromArgb(50, 80, 50)
$btnLaunch.Font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
$form.Controls.Add($btnLaunch)

$btnStop = New-Object System.Windows.Forms.Button
$btnStop.Text = "Stop Server"
$btnStop.Location = New-Object System.Drawing.Point(356, $y)
$btnStop.Size = New-Object System.Drawing.Size(160, 40)
$btnStop.FlatStyle = "Flat"
$btnStop.BackColor = [System.Drawing.Color]::FromArgb(80, 50, 50)
$btnStop.Font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
$form.Controls.Add($btnStop)

$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Text = ""
$lblStatus.Location = New-Object System.Drawing.Point(524, ($y + 10))
$lblStatus.Size = New-Object System.Drawing.Size(170, 20)
$lblStatus.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Italic)
$form.Controls.Add($lblStatus)

# ── Logic ──────────────────────────────────────────────────────────────────────
$script:serverProcess = $null

function Build-ServerCfg {
    $lines = @()
    $lines += "set sv_hostname `"$($txtName.Text)`""

    # Score limits for gametypes present in the rotation
    $usedGametypes = @{}
    foreach ($entry in $script:rotationData) {
        $usedGametypes[$entry.gametype] = $true
    }
    foreach ($kv in $scoreControls.GetEnumerator()) {
        if ($usedGametypes.ContainsKey($kv.Key) -or $script:rotationData.Count -eq 0) {
            $lines += "set scr_$($kv.Key)_scorelimit $([int]$kv.Value.Value)"
        }
    }

    if ($chkHalftime.Checked) {
        $lines += "set scr_dom_halftime 0"
        $lines += "set scr_dom_roundlimit 1"
    }

    $lines += "set bot_fill $([int]$nudBotFill.Value)"
    $lines += "set bot_names $($cmbBotNames.SelectedItem)"

    # Build rotation string
    if ($script:rotationData.Count -gt 0) {
        $parts = @()
        foreach ($entry in $script:rotationData) {
            $parts += "gametype $($entry.gametype) map $($entry.map)"
        }
        $lines += "set sv_maprotation `"$($parts -join ' ')`""
    }

    return $lines -join "`n"
}

$btnLaunch.Add_Click({
    if ($script:rotationData.Count -eq 0) {
        [System.Windows.Forms.MessageBox]::Show("Add at least one map to the rotation.", "S2x Server Launcher", 0, 48)
        return
    }

    # Kill existing server
    Get-Process s2x -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match "Dedicated|Console" } | Stop-Process -Force
    Start-Sleep -Milliseconds 1500

    # Write server.cfg
    $cfgPath = Join-Path $GameDir "s2x\server.cfg"
    Build-ServerCfg | Set-Content $cfgPath -Encoding UTF8

    # Launch
    $port = [int]$nudPort.Value
    $args = "-noupdate -dedicated +set net_port $port +exec server.cfg +map_rotate"
    $script:serverProcess = Start-Process -FilePath (Join-Path $GameDir "s2x.exe") -ArgumentList $args -WorkingDirectory $GameDir -PassThru

    $lblStatus.Text = "Server running (PID $($script:serverProcess.Id))"
    $lblStatus.ForeColor = [System.Drawing.Color]::FromArgb(100, 200, 100)

    if ($chkConnect.Checked) {
        Start-Sleep -Seconds 12
        $clientProc = Get-Process s2x -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match "Multiplayer" }
        if ($clientProc) {
            # Cannot send console commands to client from here — user connects manually
            [System.Windows.Forms.MessageBox]::Show("Server is ready. Connect with:`nconnect 127.0.0.1:$port", "S2x Server Launcher", 0, 64)
        }
    }
})

$btnStop.Add_Click({
    Get-Process s2x -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match "Dedicated|Console" } | Stop-Process -Force
    $script:serverProcess = $null
    $lblStatus.Text = "Server stopped"
    $lblStatus.ForeColor = [System.Drawing.Color]::FromArgb(200, 100, 100)
})

$form.Add_FormClosing({
    # Don't kill the server on close — let it keep running
})

# ── Show ───────────────────────────────────────────────────────────────────────
[void]$form.ShowDialog()
