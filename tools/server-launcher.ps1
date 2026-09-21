# S2x Dedicated Server Launcher — WPF
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\S2x\tools\server-launcher.ps1
#   pwsh       -NoProfile -ExecutionPolicy Bypass -File D:\S2x\tools\server-launcher.ps1
#   ... -File server-launcher.ps1 -GameDir "C:\Games\Call of Duty WWII"
#
# Keep ServerLauncher.xaml next to this file.
# The game folder is taken from -GameDir, a remembered choice, the current directory, this
# script's folder or up to two folders above it (the release ships it as <game>\s2x\tools),
# the Steam registry entry, or a folder picker; the picked folder is remembered.
# Behaviour matches the WinForms version: same presets, same server.cfg, same launch args.

param([string]$GameDir)

Set-StrictMode -Off
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

# ── Game path ──────────────────────────────────────────────────────────────────
$rememberedGameDirFile = Join-Path $env:LOCALAPPDATA "s2x\launcher-gamedir.txt"
function Test-GameDir($dir) { $dir -and (Test-Path (Join-Path $dir "s2x.exe")) }

if (-not (Test-GameDir $GameDir)) {
    $candidates = @()
    if (Test-Path $rememberedGameDirFile) { $candidates += (Get-Content $rememberedGameDirFile -Raw).Trim() }
    $candidates += (Get-Location).Path
    $candidates += $PSScriptRoot
    $candidates += (Split-Path $PSScriptRoot -Parent)
    $candidates += (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
    $candidates += (Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 476600" -ErrorAction SilentlyContinue).InstallLocation
    $candidates += "D:\Program Files\Steam\steamapps\common\Call of Duty WWII"
    $GameDir = $candidates | Where-Object { Test-GameDir $_ } | Select-Object -First 1
}
if (-not (Test-GameDir $GameDir)) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker = New-Object System.Windows.Forms.FolderBrowserDialog
    $picker.Description = "Select the Call of Duty WWII folder that contains s2x.exe"
    $picker.ShowNewFolderButton = $false
    if ($picker.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK -and (Test-GameDir $picker.SelectedPath)) {
        $GameDir = $picker.SelectedPath
        New-Item -ItemType Directory -Force (Split-Path $rememberedGameDirFile -Parent) | Out-Null
        [System.IO.File]::WriteAllText($rememberedGameDirFile, $GameDir, [System.Text.UTF8Encoding]::new($false))
    } else {
        [System.Windows.MessageBox]::Show("Could not find s2x.exe. Run this script from the game folder, pass -GameDir, or pick the folder that contains s2x.exe.", "S2x Server Launcher", "OK", "Warning") | Out-Null
        exit 1
    }
}

$PresetDir = Join-Path $GameDir "s2x\presets"
if (-not (Test-Path $PresetDir)) { New-Item -ItemType Directory -Force $PresetDir | Out-Null }

# ── Data ───────────────────────────────────────────────────────────────────────
$Maps = [ordered]@{
    "mp_shipment_s2"       = "Shipment 1944"
    "mp_d_day"             = "Pointe du Hoc"
    "mp_aachen_v2"         = "Aachen"
    "mp_carentan_s2"       = "Carentan"
    "mp_carentan_s2_winter" = "Winter Carentan"
    "mp_canon_farm"        = "Gustav Cannon"
    "mp_flak_tower"        = "Flak Tower"
    "mp_forest_01"         = "Ardennes Forest"
    "mp_london"            = "London Docks"
    "mp_france_village"    = "Sainte Marie du Mont"
    "mp_battleship_2"      = "USS Texas"
    "mp_gibraltar_02"      = "Gibraltar"
    "mp_sandbox_01"        = "Sandbox"
    "mp_house"             = "Groesten Haus"
    "mp_paris_s2"          = "Occupation"
    "mp_prague"            = "Anthropoid"
    "mp_wolfslair"         = "Valkyrie"
    "mp_dunkirk"           = "Dunkirk"
    "mp_egypt_02"          = "Egypt"
    "mp_v2_rocket_02"      = "V2"
    "mp_stalingrad"        = "Stalingrad"
    "mp_market_garden"     = "Market Garden"
    "mp_monte_cassino_v2"  = "Monte Cassino"
    "mp_tank_graveyard_2"  = "Excavation"
    "mp_airship"           = "Airship"
    "mp_fuhrerbunker"      = "Chancellery"
}
# Maps that need a DLC pack on every player's client. Keyed by zone name.
$MapPacks = @{
    "mp_carentan_s2"        = "Season Pass"
    "mp_carentan_s2_winter" = "Season Pass"
    "mp_paris_s2"           = "DLC 1"
    "mp_prague"             = "DLC 1"
    "mp_wolfslair"          = "DLC 1"
    "mp_dunkirk"            = "DLC 2"
    "mp_egypt_02"           = "DLC 2"
    "mp_v2_rocket_02"       = "DLC 2"
    "mp_stalingrad"         = "DLC 3"
    "mp_market_garden"      = "DLC 3"
    "mp_monte_cassino_v2"   = "DLC 3"
    "mp_tank_graveyard_2"   = "DLC 4"
    "mp_airship"            = "DLC 4"
    "mp_fuhrerbunker"       = "DLC 4"
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

$ZombieMaps = [ordered]@{
    "mp_zombie_house"    = "Groesten Haus"
    "mp_zombie_descent"  = "The Final Reich"
    "mp_zombie_island"   = "The Darkest Shore"
    "mp_zombie_berlin"   = "The Shadowed Throne"
    "mp_zombie_windmill" = "The Tortured Path: Into the Storm"
    "mp_zombie_dnk"      = "The Tortured Path: Across the Depths"
    "mp_zombie_dig_02"   = "The Tortured Path: Beyond the Veil"
    "mp_zombie_nest_01"  = "The Frozen Dawn"
}
$ZombieMapPacks = @{
    "mp_zombie_island"   = "DLC 1"
    "mp_zombie_berlin"   = "DLC 2"
    "mp_zombie_windmill" = "DLC 3"
    "mp_zombie_dnk"      = "DLC 3"
    "mp_zombie_dig_02"   = "DLC 3"
    "mp_zombie_nest_01"  = "DLC 4"
}
$ZombieMapKeys   = @($ZombieMaps.Keys)
$ZombieMapValues = @($ZombieMaps.Values)

# Presets saved before v1.3.0 carry the placeholder zone names the launcher used
# to list Zombies maps; the game never had them. The old single Tortured Path
# entry becomes its first chapter.
$LegacyZombieZones = @{
    "nazi_zombie_proto"       = "mp_zombie_house"
    "nazi_zombie_asylum_f"    = "mp_zombie_descent"
    "nazi_zombie_island"      = "mp_zombie_island"
    "nazi_zombie_office"      = "mp_zombie_berlin"
    "nazi_zombie_treasure"    = "mp_zombie_windmill"
    "nazi_zombie_uss"         = "mp_zombie_dnk"
    "nazi_zombie_museum"      = "mp_zombie_dig_02"
    "nazi_zombie_mountaineer" = "mp_zombie_nest_01"
}

$BotNamePools = @("default", "modern", "nostalgia")
$script:isZombies = $false
$script:suppressModeSwitch = $false
$MiddleDot    = [string][char]0x00B7

# Picker labels: the map name, plus the pack it needs when it is not in the base game.
function Get-MapDisplay($key, $name, $packs) {
    if ($packs.ContainsKey($key)) { return "$name  $MiddleDot  $($packs[$key])" }
    return $name
}
$MapDisplay       = @($MapKeys       | ForEach-Object { Get-MapDisplay $_ $Maps[$_]       $MapPacks })
$ZombieMapDisplay = @($ZombieMapKeys | ForEach-Object { Get-MapDisplay $_ $ZombieMaps[$_] $ZombieMapPacks })

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
    "txtName","pnlColors","txtNamePreview","cmbPreset","btnLoadPreset","btnSavePreset","btnDeletePreset",
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

# One config and one pid file per port, so several servers can run from this
# folder, each owned by the launcher window that started it.
function Get-PortCfgPath($port) { Join-Path $GameDir "s2x\server-$port.cfg" }
function Get-PortPidPath($port) { Join-Path $GameDir "s2x\server-$port.pid" }
$lblCfgPath.Text = Get-PortCfgPath 27016

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
    $i = [array]::IndexOf($ZombieMapKeys, $key)
    if ($i -ge 0) { return $ZombieMapValues[$i] }
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
foreach ($v in $MapDisplay)     { [void]$cmbMap.Items.Add($v) }
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

# ── Server name colour codes ───────────────────────────────────────────────────
# The engine renders ^0-^7 in sv_hostname (browser, scoreboard); the swatches insert
# a code at the caret and the preview shows the result without the codes.
$ColorCodes = @{ '0' = '#101010'; '1' = '#E0332B'; '2' = '#34C759'; '3' = '#F5C400'
                 '4' = '#3C7BFF'; '5' = '#2AD4E0'; '6' = '#E040C8'; '7' = '#F2F2F2' }
$BrushConverter = New-Object System.Windows.Media.BrushConverter
function Update-NamePreview {
    $txtNamePreview.Inlines.Clear()
    $color = $ColorCodes['7']
    foreach ($part in [regex]::Split($txtName.Text, '(\^[0-9])')) {
        if ($part -match '^\^([0-9])$') {
            if ($ColorCodes.ContainsKey($Matches[1])) { $color = $ColorCodes[$Matches[1]] }
            continue
        }
        if ($part.Length -eq 0) { continue }
        $run = New-Object System.Windows.Documents.Run $part
        $run.Foreground = $BrushConverter.ConvertFromString($color)
        $txtNamePreview.Inlines.Add($run)
    }
}
$txtName.Add_TextChanged({ Update-NamePreview })
foreach ($swatch in $pnlColors.Children) {
    if ($swatch -isnot [System.Windows.Controls.Border] -or -not $swatch.Tag) { continue }
    $swatch.Add_MouseLeftButtonDown({
        param($sender, $e)
        $pos = $txtName.CaretIndex
        $txtName.Text = $txtName.Text.Insert($pos, [string]$sender.Tag)
        $txtName.CaretIndex = $pos + 2
        $txtName.Focus()
        $e.Handled = $true
    })
}
Update-NamePreview

# ── Mode toggle (MP / Zombies) ────────────────────────────────────────────────
$script:cmbMode = New-Object System.Windows.Controls.ComboBox
$cmbMode = $script:cmbMode
$cmbMode.Width = 120
$cmbMode.FontSize = 12
$cmbMode.Margin = "0,0,12,0"
$cmbMode.VerticalAlignment = "Center"
[void]$cmbMode.Items.Add("Multiplayer")
[void]$cmbMode.Items.Add("Zombies")
$cmbMode.SelectedIndex = 0

$modeLabel = New-Object System.Windows.Controls.TextBlock
$modeLabel.Text = "MODE"
$modeLabel.FontSize = 10
$modeLabel.FontWeight = "SemiBold"
$modeLabel.VerticalAlignment = "Center"
$modeLabel.Margin = "0,0,6,0"
$modeLabel.Foreground = $Muted

$titleBar = $barTitle
$titleBarParent = $titleBar.Parent
if ($titleBarParent -and $titleBarParent -is [System.Windows.Controls.Panel]) {
    $modePanel = New-Object System.Windows.Controls.StackPanel
    $modePanel.Orientation = "Horizontal"
    $modePanel.HorizontalAlignment = "Right"
    $modePanel.VerticalAlignment = "Center"
    $modePanel.Margin = "0,0,100,0"
    [void]$modePanel.Children.Add($modeLabel)
    [void]$modePanel.Children.Add($cmbMode)
    [void]$titleBarParent.Children.Add($modePanel)
}

# Score/gametype panels to show/hide
$script:scorePanel = $null
$script:botPanel = $null
$script:gametypeControl = $cmbGametype
foreach ($ctrl in @($numWar, $numDom, $numHp, $numDm, $numConf, $numSd, $numCtf, $numGun, $numBall, $chkHalftime, $lblScoreSummary)) {
    if ($ctrl) {
        $p = $ctrl.Parent
        while ($p -and -not ($p -is [System.Windows.Controls.Expander])) { $p = $p.Parent }
        if ($p) { $script:scorePanel = $p; break }
    }
}
foreach ($ctrl in @($sldBotFill, $lblBotFill, $cmbBotNames, $lblBotSummary)) {
    if ($ctrl) {
        $p = $ctrl.Parent
        while ($p -and -not ($p -is [System.Windows.Controls.Expander])) { $p = $p.Parent }
        if ($p) { $script:botPanel = $p; break }
    }
}

function Switch-Mode {
    if ($script:suppressModeSwitch) { return }

    # Save current mode's state before switching
    $oldModeName = if ($script:isZombies) { "_lastused_zombies" } else { "_lastused_mp" }
    if ($script:rotationData.Count -gt 0) {
        Save-Preset $oldModeName
    }

    # Load the new mode
    $script:isZombies = ($cmbMode.SelectedIndex -eq 1)
    $newModeName = if ($script:isZombies) { "_lastused_zombies" } else { "_lastused_mp" }

    $cmbMap.Items.Clear()
    $script:rotationData.Clear()

    if ($script:isZombies) {
        foreach ($v in $ZombieMapDisplay) { [void]$cmbMap.Items.Add($v) }
        $cmbGametype.Visibility = "Collapsed"
        if ($script:scorePanel) { $script:scorePanel.Visibility = "Collapsed" }
        if ($script:botPanel) { $script:botPanel.Visibility = "Collapsed" }
    } else {
        foreach ($v in $MapDisplay) { [void]$cmbMap.Items.Add($v) }
        $cmbGametype.Visibility = "Visible"
        if ($script:scorePanel) { $script:scorePanel.Visibility = "Visible" }
        if ($script:botPanel) { $script:botPanel.Visibility = "Visible" }
    }

    if ($cmbMap.Items.Count -gt 0) { $cmbMap.SelectedIndex = 0 }

    # Restore saved state for the new mode
    $path = Join-Path $PresetDir "$newModeName.json"
    if (Test-Path $path) {
        $script:suppressModeSwitch = $true
        try { Set-Config (Get-Content $path -Raw | ConvertFrom-Json) }
        finally { $script:suppressModeSwitch = $false }
    }

    Update-RotationNumbers
    Update-Summaries
}

$cmbMode.Add_SelectionChanged({ Switch-Mode })

# ── Rotation interactions ──────────────────────────────────────────────────────
$btnAdd.Add_Click({
    if ($cmbMap.SelectedIndex -lt 0) { return }
    if ($script:isZombies) {
        Add-Rotation $ZombieMapKeys[$cmbMap.SelectedIndex] "zombies"
    } else {
        if ($cmbGametype.SelectedIndex -lt 0) { return }
        Add-Rotation $MapKeys[$cmbMap.SelectedIndex] $GametypeKeys[$cmbGametype.SelectedIndex]
    }
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
        mode           = if ($script:isZombies) { "zombies" } else { "mp" }
        rotation       = @($script:rotationData | ForEach-Object { @{ map = $_.Map; gametype = $_.Gametype } })
        scoreLimits    = $scores
        singleRoundDom = [bool]$chkHalftime.IsChecked
        botFill        = [int]$sldBotFill.Value
        botNames       = [string]$cmbBotNames.SelectedItem
        port           = [math]::Max(1024, [math]::Min(65535, (Get-BoxInt $txtPort 27016)))
    }
}

function Set-Config($cfg) {
    if ($cfg.mode -eq "zombies") { $cmbMode.SelectedIndex = 1 } else { $cmbMode.SelectedIndex = 0 }
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
        foreach ($entry in $cfg.rotation) {
            # Every restore path (preset load, mode switch) comes through here.
            $map = $entry.map
            if ($map -and $LegacyZombieZones.ContainsKey($map)) { $map = $LegacyZombieZones[$map] }
            Add-Rotation $map $entry.gametype
        }
    }
    Update-RotationNumbers
    Update-Summaries
}

function Update-PresetList {
    # Presets bundled next to the launcher (the zip ships them in tools\presets) are
    # copied in once; a preset the host already has under that name is left alone.
    New-Item -ItemType Directory -Force $PresetDir | Out-Null
    foreach ($bundle in @((Join-Path $PSScriptRoot "presets"), (Join-Path $PSScriptRoot "server-presets"))) {
        if (-not (Test-Path $bundle)) { continue }
        Get-ChildItem $bundle -Filter "*.json" -ErrorAction SilentlyContinue | ForEach-Object {
            $dst = Join-Path $PresetDir $_.Name
            if (-not (Test-Path $dst)) { Copy-Item $_.FullName $dst -ErrorAction SilentlyContinue }
        }
    }
    $cmbPreset.Items.Clear()
    [void]$cmbPreset.Items.Add("(last used)")
    Get-ChildItem $PresetDir -Filter "*.json" -ErrorAction SilentlyContinue |
        Where-Object { $_.BaseName -notlike "_lastused*" } |
        ForEach-Object { [void]$cmbPreset.Items.Add($_.BaseName) }
    $cmbPreset.SelectedIndex = 0
}

function Save-Preset($name) {
    Get-CurrentConfig | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PresetDir "$name.json") -Encoding UTF8
}

function Import-Preset($name) {
    $path = Join-Path $PresetDir "$name.json"
    if (-not (Test-Path $path)) { return }
    $cfg = Get-Content $path -Raw | ConvertFrom-Json

    $script:suppressModeSwitch = $true
    try {
        $wantZombies = ($cfg.mode -eq "zombies")
        $cmbMode.SelectedIndex = if ($wantZombies) { 1 } else { 0 }
        $script:isZombies = $wantZombies

        $cmbMap.Items.Clear()
        if ($wantZombies) {
            foreach ($v in $ZombieMapDisplay) { [void]$cmbMap.Items.Add($v) }
            $cmbGametype.Visibility = "Collapsed"
            if ($script:scorePanel) { $script:scorePanel.Visibility = "Collapsed" }
            if ($script:botPanel) { $script:botPanel.Visibility = "Collapsed" }
        } else {
            foreach ($v in $MapDisplay) { [void]$cmbMap.Items.Add($v) }
            $cmbGametype.Visibility = "Visible"
            if ($script:scorePanel) { $script:scorePanel.Visibility = "Visible" }
            if ($script:botPanel) { $script:botPanel.Visibility = "Visible" }
        }
        if ($cmbMap.Items.Count -gt 0) { $cmbMap.SelectedIndex = 0 }

        Set-Config $cfg
    } finally {
        $script:suppressModeSwitch = $false
    }
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

function Get-LaunchPort { [math]::Max(1024, [math]::Min(65535, (Get-BoxInt $txtPort 27016))) }
function Update-CfgPathLabel { $lblCfgPath.Text = Get-PortCfgPath (Get-LaunchPort) }
function Stop-S2xServer {
    # Stop only the server on this window's port: the one it started, or the one a
    # previous window left behind (its pid file). Servers on other ports stay up,
    # and so does a game client.
    $port = Get-LaunchPort
    $pidPath = Get-PortPidPath $port
    $ids = @()
    if ($script:serverProcess) { $ids += $script:serverProcess.Id }
    if (Test-Path $pidPath) {
        $raw = Get-Content $pidPath -Raw -ErrorAction SilentlyContinue
        if ($raw -match '^\s*(\d+)') { $ids += [int]$Matches[1] }
    }
    foreach ($id in ($ids | Select-Object -Unique)) {
        # A pid file can outlive its server and Windows reuses PIDs, so only a
        # dedicated server started on this port qualifies, never a game client.
        $cmd = (Get-CimInstance Win32_Process -Filter "ProcessId = $id" -ErrorAction SilentlyContinue).CommandLine
        if ($cmd -and $cmd -match "\bs2x\.exe" -and $cmd -match "-dedicated" -and $cmd -match "net_port $port(\s|$)") {
            Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
        }
    }
    Remove-Item $pidPath -Force -ErrorAction SilentlyContinue
    $script:serverProcess = $null
}

$btnLaunch.Add_Click({
    if ($script:rotationData.Count -eq 0) {
        Show-Notice "Nothing to launch" "Add at least one map to the rotation."
        return
    }

    $modeName = if ($script:isZombies) { "_lastused_zombies" } else { "_lastused_mp" }
    Save-Preset "_lastused"
    Save-Preset $modeName
    $port = Get-LaunchPort
    Stop-S2xServer
    Start-Sleep -Milliseconds 1500

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    $cfgPath = Get-PortCfgPath $port
    [System.IO.File]::WriteAllText($cfgPath, (Build-ServerCfg), $utf8NoBom)
    Update-CfgPathLabel

    # Both modes start from the rotation the port's cfg carries; a command-line +map
    # runs before the dedicated party exists and is dropped, so Zombies never left
    # the virtual lobby.
    $modeFlag = if ($script:isZombies) { " -zombies" } else { "" }
    $launchArgs = "-noupdate -dedicated$modeFlag +set net_port $port +exec $(Split-Path $cfgPath -Leaf) +map_rotate"

    try {
        $script:serverProcess = Start-Process -FilePath (Join-Path $GameDir "s2x.exe") `
            -ArgumentList $launchArgs -WorkingDirectory $GameDir -PassThru -ErrorAction Stop
        Set-Content -Path (Get-PortPidPath $port) -Value $script:serverProcess.Id
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
Update-CfgPathLabel
$txtPort.Add_TextChanged({ Update-CfgPathLabel })
Update-RotationNumbers
Update-Summaries

$window.ShowDialog() | Out-Null
