# S2x Dedicated Server Launcher — WPF
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\S2x\tools\server-launcher.ps1
#   pwsh       -NoProfile -ExecutionPolicy Bypass -File D:\S2x\tools\server-launcher.ps1
#
# Keep ServerLauncher.xaml next to this file.
# Behaviour matches the WinForms version: same presets, same server.cfg, same launch args.

Set-StrictMode -Off
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

# ── Game path ──────────────────────────────────────────────────────────────────
$GameDir = (Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 476600" -ErrorAction SilentlyContinue).InstallLocation
if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir "s2x.exe"))) {
    $GameDir = "D:\Program Files\Steam\steamapps\common\Call of Duty WWII"
}
if (-not (Test-Path (Join-Path $GameDir "s2x.exe"))) {
    [System.Windows.MessageBox]::Show("Could not find s2x.exe. Place this script in the game folder or install S2x.", "S2x Server Launcher", "OK", "Warning") | Out-Null
    exit 1
}

$PresetDir = Join-Path $GameDir "s2x\presets"
if (-not (Test-Path $PresetDir)) { New-Item -ItemType Directory -Force $PresetDir | Out-Null }

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
    "mp_france_village" = "Sainte Marie du Mont"
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
$MapKeys   = @($Maps.Keys)
$MapValues = @($Maps.Values)

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
$GametypeKeys   = @($Gametypes.Keys)
$GametypeValues = @($Gametypes.Values)

$DefaultScoreLimits = [ordered]@{
    "war" = 75; "dom" = 200; "hp"   = 250
    "dm"  = 30; "conf" = 65; "sd"   = 4
    "ctf" = 3;  "gun"  = 18; "ball" = 28
}

$BotNamePools = @("default", "modern", "nostalgia")
$MiddleDot    = [string][char]0x00B7

# ── Load the XAML, loudly ──────────────────────────────────────────────────────
function Write-LoadFailure($message, $detail) {
    Write-Host ""
    Write-Host "  XAML FAILED TO LOAD" -ForegroundColor Red
    Write-Host "  $message" -ForegroundColor Red
    if ($detail) { Write-Host ""; Write-Host $detail -ForegroundColor DarkYellow }
    Write-Host ""
}

$xamlPath = Join-Path $PSScriptRoot "ServerLauncher.xaml"
if (-not (Test-Path $xamlPath)) {
    Write-LoadFailure "ServerLauncher.xaml not found next to the script." "Expected: $xamlPath"
    exit 1
}

$window = $null
try {
    $reader = New-Object System.Xml.XmlTextReader($xamlPath)
    $window = [Windows.Markup.XamlReader]::Load($reader)
}
catch {
    $ex = $_.Exception
    $lines = @()
    while ($ex) {
        $where = ""
        if ($ex -is [System.Windows.Markup.XamlParseException]) {
            $where = " (line $($ex.LineNumber), position $($ex.LinePosition))"
        }
        $lines += "$($ex.GetType().Name)$where`n  $($ex.Message)"
        $ex = $ex.InnerException
    }
    Write-LoadFailure "XamlReader.Load threw." ($lines -join "`n`n")
    exit 1
}
finally {
    if ($reader) { $reader.Close() }
}

if (-not $window) {
    Write-LoadFailure "XamlReader.Load returned null with no exception." "The root element is probably not a <Window>."
    exit 1
}

# ── Resolve controls, and say which ones are missing ───────────────────────────
$ControlNames = @(
    "barTitle","btnMinimize","btnClose",
    "txtName","cmbPreset","btnLoadPreset","btnSavePreset","btnDeletePreset",
    "lblRotationCount","btnClear","cmbMap","cmbGametype","btnAdd","lstRotation",
    "numWar","numDom","numHp","numDm","numConf","numSd","numCtf","numGun","numBall",
    "lblScoreSummary","chkHalftime",
    "lblBotSummary","lblBotFill","sldBotFill","cmbBotNames",
    "lblNetSummary","txtPort","chkConnect","lblCfgPath",
    "dotStatus","lblStatus","lblStatusSub","btnStop","btnLaunch"
)
$missing = @()
foreach ($name in $ControlNames) {
    $control = $window.FindName($name)
    if (-not $control) { $missing += $name }
    Set-Variable -Name $name -Value $control -Scope Script
}
if ($missing.Count -gt 0) {
    Write-LoadFailure "The window loaded but these x:Name controls were not found:" ("  " + ($missing -join "`n  "))
    exit 1
}

$ScoreBoxes = [ordered]@{
    "war" = $numWar; "dom" = $numDom;  "hp"   = $numHp
    "dm"  = $numDm;  "conf" = $numConf; "sd"   = $numSd
    "ctf" = $numCtf; "gun"  = $numGun;  "ball" = $numBall
}

$brush  = New-Object System.Windows.Media.BrushConverter
$Accent = $brush.ConvertFromString("#FFE8A33D")
$Ok     = $brush.ConvertFromString("#FF5FBF7A")
$Danger = $brush.ConvertFromString("#FFC7524A")
$Muted  = $brush.ConvertFromString("#FF8A9299")
$Off    = $brush.ConvertFromString("#FF4A5157")

$CfgPath = Join-Path $GameDir "s2x\server.cfg"
$lblCfgPath.Text = $CfgPath

# ── Themed dialogs ─────────────────────────────────────────────────────────────
function New-Dialog($title, $bodyPanel, $okText) {
    $dlg = New-Object System.Windows.Window
    $dlg.Title                = $title
    $dlg.WindowStyle          = "None"
    $dlg.SizeToContent        = "Height"
    $dlg.Width                = 400
    $dlg.ResizeMode           = "NoResize"
    $dlg.WindowStartupLocation = "CenterOwner"
    $dlg.Owner                = $window
    $dlg.Background           = $brush.ConvertFromString("#FF0E1012")
    $dlg.Foreground           = $brush.ConvertFromString("#FFE6E8EA")
    $dlg.BorderBrush          = $brush.ConvertFromString("#FF2A2F34")
    $dlg.BorderThickness      = 1
    $dlg.FontFamily           = "Segoe UI"
    $dlg.Resources            = $window.Resources

    $stack = New-Object System.Windows.Controls.StackPanel
    $stack.Margin = "22"

    $head = New-Object System.Windows.Controls.TextBlock
    $head.Text       = $title.ToUpper()
    $head.FontSize   = 11.5
    $head.FontWeight = "Bold"
    $head.Margin     = "0,0,0,14"
    [void]$stack.Children.Add($head)
    [void]$stack.Children.Add($bodyPanel)

    $row = New-Object System.Windows.Controls.StackPanel
    $row.Orientation         = "Horizontal"
    $row.HorizontalAlignment = "Right"
    $row.Margin              = "0,18,0,0"

    $cancel = New-Object System.Windows.Controls.Button
    $cancel.Content    = "Cancel"
    $cancel.Style      = $window.Resources["Ghost"]
    $cancel.IsCancel   = $true
    $cancel.Add_Click({ $dlg.DialogResult = $false }.GetNewClosure())
    [void]$row.Children.Add($cancel)

    $ok = New-Object System.Windows.Controls.Button
    $ok.Content   = $okText
    $ok.Style     = $window.Resources["Primary"]
    $ok.Padding   = "24,10"
    $ok.FontSize  = 12
    $ok.Margin    = "10,0,0,0"
    $ok.IsDefault = $true
    $ok.Add_Click({ $dlg.DialogResult = $true }.GetNewClosure())
    [void]$row.Children.Add($ok)

    [void]$stack.Children.Add($row)
    $dlg.Content = $stack
    $dlg.Add_MouseLeftButtonDown({ $dlg.DragMove() }.GetNewClosure())
    return $dlg
}

function New-DialogText($text) {
    $t = New-Object System.Windows.Controls.TextBlock
    $t.Text         = $text
    $t.FontSize     = 13
    $t.TextWrapping = "Wrap"
    $t.Foreground   = $Muted
    return $t
}

function Show-Prompt($title, $label, $default) {
    $panel = New-Object System.Windows.Controls.StackPanel
    $cap = New-DialogText $label
    $cap.FontSize = 12.5
    $cap.Margin   = "0,0,0,8"
    [void]$panel.Children.Add($cap)

    $box = New-Object System.Windows.Controls.TextBox
    $box.Text     = $default
    $box.FontSize = 14
    [void]$panel.Children.Add($box)

    $dlg = New-Dialog $title $panel "Save"
    $dlg.Add_ContentRendered({ $box.SelectAll(); $box.Focus() | Out-Null }.GetNewClosure())
    if ($dlg.ShowDialog()) { return $box.Text }
    return $null
}

function Show-Confirm($title, $message) {
    $panel = New-Object System.Windows.Controls.StackPanel
    [void]$panel.Children.Add((New-DialogText $message))
    return [bool](New-Dialog $title $panel "Delete").ShowDialog()
}

function Show-Notice($title, $message) {
    $panel = New-Object System.Windows.Controls.StackPanel
    [void]$panel.Children.Add((New-DialogText $message))
    (New-Dialog $title $panel "OK").ShowDialog() | Out-Null
}

# ── Rotation model ─────────────────────────────────────────────────────────────
$script:rotationData = New-Object System.Collections.ObjectModel.ObservableCollection[object]
$lstRotation.ItemsSource = $script:rotationData

function Get-MapName($key) {
    $i = [array]::IndexOf($MapKeys, $key)
    if ($i -ge 0) { return $MapValues[$i] }
    return $key
}
function Get-GtName($key) {
    $i = [array]::IndexOf($GametypeKeys, $key)
    if ($i -ge 0) { return $GametypeValues[$i] }
    return $key
}

function New-RotationRow($mapKey, $gtKey, $index) {
    return [pscustomobject]@{
        Num      = "{0:00}" -f $index
        Map      = $mapKey
        Gametype = $gtKey
        MapName  = Get-MapName $mapKey
        GtName   = Get-GtName $gtKey
        Line     = "gametype $gtKey map $mapKey"
    }
}

function Update-RotationNumbers {
    for ($i = 0; $i -lt $script:rotationData.Count; $i++) {
        $entry = $script:rotationData[$i]
        $want  = "{0:00}" -f ($i + 1)
        if ($entry.Num -ne $want) {
            $script:rotationData[$i] = New-RotationRow $entry.Map $entry.Gametype ($i + 1)
        }
    }
    $n = $script:rotationData.Count
    if ($n -eq 1) { $lblRotationCount.Text = "1 entry" } else { $lblRotationCount.Text = "$n entries" }
}

function Add-Rotation($mapKey, $gtKey) {
    $script:rotationData.Add((New-RotationRow $mapKey $gtKey ($script:rotationData.Count + 1)))
    Update-RotationNumbers
}

function Move-Rotation($index, $delta) {
    $j = $index + $delta
    if ($index -lt 0 -or $j -lt 0 -or $j -ge $script:rotationData.Count) { return }
    $script:rotationData.Move($index, $j)
    Update-RotationNumbers
    $lstRotation.SelectedIndex = $j
}

# ── Populate ───────────────────────────────────────────────────────────────────
foreach ($v in $MapValues)      { [void]$cmbMap.Items.Add($v) }
foreach ($v in $GametypeValues) { [void]$cmbGametype.Items.Add($v) }
foreach ($p in $BotNamePools)   { [void]$cmbBotNames.Items.Add($p) }
$cmbMap.SelectedIndex      = 0
$cmbGametype.SelectedIndex = 0
$cmbBotNames.SelectedIndex = 0

# ── Summaries + input filtering ────────────────────────────────────────────────
function Get-BoxInt($box, $fallback) {
    $v = 0
    if ([int]::TryParse($box.Text, [ref]$v)) { return $v }
    return $fallback
}

function Update-Summaries {
    $changed = 0
    foreach ($k in $ScoreBoxes.Keys) {
        if ((Get-BoxInt $ScoreBoxes[$k] $DefaultScoreLimits[$k]) -ne $DefaultScoreLimits[$k]) { $changed++ }
    }
    if ($changed -gt 0) { $lblScoreSummary.Text = "$changed changed" } else { $lblScoreSummary.Text = "defaults" }
    $lblBotSummary.Text = "$([int]$sldBotFill.Value) bots $MiddleDot $($cmbBotNames.SelectedItem)"
    $lblNetSummary.Text = ":" + $txtPort.Text
}

foreach ($box in (@($ScoreBoxes.Values) + @($txtPort))) {
    $box.Add_PreviewTextInput({
        param($sender, $e)
        if ($e.Text -notmatch '^[0-9]+$') { $e.Handled = $true }
    })
    $box.Add_TextChanged({ Update-Summaries })
}

$sldBotFill.Add_ValueChanged({
    $lblBotFill.Text = [string][int]$sldBotFill.Value
    Update-Summaries
})
$cmbBotNames.Add_SelectionChanged({ Update-Summaries })

# ── Rotation interactions ──────────────────────────────────────────────────────
$btnAdd.Add_Click({
    if ($cmbMap.SelectedIndex -lt 0 -or $cmbGametype.SelectedIndex -lt 0) { return }
    Add-Rotation $MapKeys[$cmbMap.SelectedIndex] $GametypeKeys[$cmbGametype.SelectedIndex]
})

$btnClear.Add_Click({
    $script:rotationData.Clear()
    Update-RotationNumbers
})

# Row buttons live in the item template; catch their clicks as they bubble up.
$lstRotation.AddHandler(
    [System.Windows.Controls.Button]::ClickEvent,
    [System.Windows.RoutedEventHandler]{
        param($sender, $e)
        $btn = $e.OriginalSource -as [System.Windows.Controls.Button]
        if (-not $btn -or -not $btn.Tag) { return }
        $i = $script:rotationData.IndexOf($btn.DataContext)
        if ($i -lt 0) { return }
        switch ([string]$btn.Tag) {
            "up"   { Move-Rotation $i -1 }
            "down" { Move-Rotation $i  1 }
            "del"  { $script:rotationData.RemoveAt($i); Update-RotationNumbers }
        }
        $e.Handled = $true
    })

# ── Config plumbing ────────────────────────────────────────────────────────────
function Get-CurrentConfig {
    $scores = @{}
    foreach ($k in $ScoreBoxes.Keys) {
        $v = Get-BoxInt $ScoreBoxes[$k] $DefaultScoreLimits[$k]
        $scores[$k] = [math]::Max(1, [math]::Min(999, $v))
    }
    return @{
        serverName     = $txtName.Text
        rotation       = @($script:rotationData | ForEach-Object { @{ map = $_.Map; gametype = $_.Gametype } })
        scoreLimits    = $scores
        singleRoundDom = [bool]$chkHalftime.IsChecked
        botFill        = [int]$sldBotFill.Value
        botNames       = [string]$cmbBotNames.SelectedItem
        port           = [math]::Max(1024, [math]::Min(65535, (Get-BoxInt $txtPort 27016)))
    }
}

function Set-Config($cfg) {
    if ($cfg.serverName) { $txtName.Text = [string]$cfg.serverName }
    if ($cfg.port)       { $txtPort.Text = [string][math]::Max(1024, [math]::Min(65535, [int]$cfg.port)) }
    if ($null -ne $cfg.botFill) { $sldBotFill.Value = [math]::Max(0, [math]::Min(18, [int]$cfg.botFill)) }
    if ($cfg.botNames) {
        $bi = [array]::IndexOf($BotNamePools, [string]$cfg.botNames)
        if ($bi -ge 0) { $cmbBotNames.SelectedIndex = $bi }
    }
    if ($null -ne $cfg.singleRoundDom) { $chkHalftime.IsChecked = [bool]$cfg.singleRoundDom }
    if ($cfg.scoreLimits) {
        foreach ($prop in $cfg.scoreLimits.PSObject.Properties) {
            if ($ScoreBoxes.Contains($prop.Name)) {
                $ScoreBoxes[$prop.Name].Text = [string][math]::Max(1, [math]::Min(999, [int]$prop.Value))
            }
        }
    }
    $script:rotationData.Clear()
    if ($cfg.rotation) {
        foreach ($entry in $cfg.rotation) { Add-Rotation $entry.map $entry.gametype }
    }
    Update-RotationNumbers
    Update-Summaries
}

function Update-PresetList {
    $cmbPreset.Items.Clear()
    [void]$cmbPreset.Items.Add("(last used)")
    Get-ChildItem $PresetDir -Filter "*.json" -ErrorAction SilentlyContinue |
        Where-Object { $_.BaseName -ne "_lastused" } |
        ForEach-Object { [void]$cmbPreset.Items.Add($_.BaseName) }
    $cmbPreset.SelectedIndex = 0
}

function Save-Preset($name) {
    Get-CurrentConfig | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PresetDir "$name.json") -Encoding UTF8
}

function Import-Preset($name) {
    $path = Join-Path $PresetDir "$name.json"
    if (Test-Path $path) { Set-Config (Get-Content $path -Raw | ConvertFrom-Json) }
}

$btnSavePreset.Add_Click({
    $entered = Show-Prompt "Save preset" "Preset name" "My Server"
    if ($entered -and $entered.Trim()) {
        $safeName = $entered.Trim() -replace '[\\/:*?"<>|]', '_'
        Save-Preset $safeName
        Update-PresetList
        for ($i = 0; $i -lt $cmbPreset.Items.Count; $i++) {
            if ($cmbPreset.Items[$i] -eq $safeName) { $cmbPreset.SelectedIndex = $i; break }
        }
    }
})

$btnLoadPreset.Add_Click({
    $sel = $cmbPreset.SelectedItem
    if (-not $sel) { return }
    if ($sel -eq "(last used)") { $sel = "_lastused" }
    Import-Preset $sel
})

$btnDeletePreset.Add_Click({
    $sel = $cmbPreset.SelectedItem
    if (-not $sel -or $sel -eq "(last used)") { return }
    $path = Join-Path $PresetDir "$sel.json"
    if ((Test-Path $path) -and (Show-Confirm "Delete preset" "Delete preset '$sel'? This cannot be undone.")) {
        Remove-Item $path -Force
        Update-PresetList
    }
})

# ── Server ─────────────────────────────────────────────────────────────────────
$script:serverProcess = $null

function Set-Status($state, $sub) {
    switch ($state) {
        "running" { $dotStatus.Fill = $Ok;     $lblStatus.Foreground = $Ok;     $lblStatus.Text = "Server running" }
        "stopped" { $dotStatus.Fill = $Off;    $lblStatus.Foreground = $Muted;  $lblStatus.Text = "Server stopped" }
        "error"   { $dotStatus.Fill = $Danger; $lblStatus.Foreground = $Danger; $lblStatus.Text = "Launch failed" }
    }
    $lblStatusSub.Text = $sub
}

function Build-ServerCfg {
    $lines = @()
    $lines += "set sv_hostname `"$($txtName.Text)`""

    $used = @{}
    foreach ($entry in $script:rotationData) { $used[$entry.Gametype] = $true }

    foreach ($k in $ScoreBoxes.Keys) {
        if ($used.ContainsKey($k) -or $script:rotationData.Count -eq 0) {
            $v = [math]::Max(1, [math]::Min(999, (Get-BoxInt $ScoreBoxes[$k] $DefaultScoreLimits[$k])))
            $lines += "set scr_${k}_scorelimit $v"
        }
    }

    if ($chkHalftime.IsChecked) {
        $lines += "set scr_dom_halftime 0"
        $lines += "set scr_dom_roundlimit 1"
    }

    $lines += "set bot_fill $([int]$sldBotFill.Value)"
    $lines += "set bot_names $($cmbBotNames.SelectedItem)"

    if ($script:rotationData.Count -gt 0) {
        $parts = @()
        foreach ($entry in $script:rotationData) { $parts += "gametype $($entry.Gametype) map $($entry.Map)" }
        $lines += "set sv_maprotation `"$($parts -join ' ')`""
    }

    return ($lines -join "`n")
}

function Stop-S2xServer {
    Get-Process s2x -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowTitle -match "Dedicated|Console" } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    $script:serverProcess = $null
}

$btnLaunch.Add_Click({
    if ($script:rotationData.Count -eq 0) {
        Show-Notice "Nothing to launch" "Add at least one map to the rotation."
        return
    }

    Save-Preset "_lastused"
    Stop-S2xServer
    Start-Sleep -Milliseconds 1500

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($CfgPath, (Build-ServerCfg), $utf8NoBom)

    $port = [math]::Max(1024, [math]::Min(65535, (Get-BoxInt $txtPort 27016)))
    $launchArgs = "-noupdate -dedicated +set net_port $port +exec server.cfg +map_rotate"

    try {
        $script:serverProcess = Start-Process -FilePath (Join-Path $GameDir "s2x.exe") `
            -ArgumentList $launchArgs -WorkingDirectory $GameDir -PassThru -ErrorAction Stop
        Set-Status "running" "PID $($script:serverProcess.Id) $MiddleDot connect 127.0.0.1:$port"
    }
    catch {
        Set-Status "error" $_.Exception.Message
        return
    }

    if ($chkConnect.IsChecked) {
        Start-Sleep -Seconds 12
        Show-Notice "Server ready" "Connect with:`nconnect 127.0.0.1:$port"
    }
})

$btnStop.Add_Click({
    Stop-S2xServer
    Set-Status "stopped" "no process"
})

# ── Chrome (no WindowChrome — drag and caption buttons wired here) ─────────────
$barTitle.Add_MouseLeftButtonDown({
    param($sender, $e)
    if ($e.ClickCount -eq 2) {
        if ($window.WindowState -eq "Maximized") { $window.WindowState = "Normal" }
        else { $window.WindowState = "Maximized" }
    }
    else { $window.DragMove() }
})
$btnMinimize.Add_Click({ $window.WindowState = "Minimized" })
$btnClose.Add_Click({ $window.Close() })

# ── Init ───────────────────────────────────────────────────────────────────────
Set-Status "stopped" "no process"
$lblBotFill.Text = [string][int]$sldBotFill.Value
Update-PresetList
Import-Preset "_lastused"
Update-RotationNumbers
Update-Summaries

$window.ShowDialog() | Out-Null
