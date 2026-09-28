param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
# Run with Windows PowerShell -STA after building Release. Never reads real preferences:
# every settings file here is in a unique temp folder, and only files this script made are removed.
$ErrorActionPreference='Stop'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-themes-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$files=@('settings.json','legacy.json','unknown-keys.json','settings-dialog.json')
$blockedName='theme-blocked-settings'
function TestPath([string]$name){ Join-Path $testRoot $name }
function Check($condition,[string]$label){ if(!$condition){throw "FAIL $label"}; Write-Output "PASS $label" }
function Near([double]$a,[double]$b){ [Math]::Abs($a-$b) -lt 0.01 }
try {
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$assembly=[Reflection.Assembly]::LoadFrom(([IO.Path]::GetFullPath($AssemblyPath)))
$app=New-Object S2x.ServerManager.App
$app.InitializeComponent()
$TM=[S2x.ServerManager.Services.ThemeManager]
$TS=[S2x.ServerManager.Services.ThemeSettings]
$Mode=[S2x.ServerManager.Services.ThemeMode]
$Cat=[S2x.ServerManager.Services.ThemeCatalog]
$Res=[S2x.ServerManager.Services.ThemeResources]
$Css=[S2x.ServerManager.Services.Css]
$Fonts=[S2x.ServerManager.Services.ThemeFonts]
$modes=[Enum]::GetValues($Mode)

# ── defaults ────────────────────────────────────────────────────────────────
$path=TestPath 'settings.json'
$TM::Initialize($path)
Check ($TM::Current -eq $Mode::Undead -and $TM::EffectsEnabled -and !$TM::LoadWarning -and !(Test-Path -LiteralPath $path)) 'new user gets Undead with Textures & glow on, and reading writes nothing'
Check ($modes.Count -eq 9 -and $Cat::All.Count -eq 9) 'nine themes: the eight-theme pack plus High contrast'
Check (($modes | ForEach-Object { $_.ToString() }) -join ',' -eq 'FieldOps,Undead,Phosphor,Outrun,PackAPunch,NightVision,Arcade,Prestige,HighContrast') 'theme ids in the design order'

# ── every theme applies, persists and keeps live brush identity ─────────────
$ink=$app.FindResource('Ink')
$panel=$app.FindResource('Panel')
foreach($m in @($modes)+@($Mode::Undead)) {
    $TM::Apply($m)
    if(!([object]::ReferenceEquals($app.FindResource('Ink'),$ink)) -or !([object]::ReferenceEquals($app.FindResource('Panel'),$panel))){throw "FAIL $m replaced a live brush"}
    $warning=$null
    $pref=$TS::LoadPreference($path,[ref]$warning)
    if($pref.Theme -ne $m -or !$pref.Effects -or $warning){throw "FAIL $m preference roundtrip"}
    if($TS::Load($path,[ref]$warning) -ne $m){throw "FAIL $m 1.1.0 Load() roundtrip"}
    foreach($key in 'Fill','FillHot','FontDisplay','FontBody','FontLabel','FontMono','Radius','RadiusBadge','BadgeTransform'){
        if($null -eq $app.FindResource($key)){throw "FAIL $m has no $key"}
    }
    $expected=$Res::For($m,$true).Colors['Ink']
    if($ink.Color -ne $expected){throw "FAIL $m did not recolour Ink in place"}
    if($app.FindResource('SizeBody') -lt 11 -or $app.FindResource('SizeBody') -gt 16){throw "FAIL $m body size out of range"}
    $window=New-Object S2x.ServerManager.Views.SettingsDialog
    $content=$window.Content
    $content.Measure([Windows.Size]::new(600,1200));$content.Arrange([Windows.Rect]::new(0,0,600,$content.DesiredSize.Height));$content.UpdateLayout()
    if($content.DesiredSize.Height -lt 400){throw "FAIL $m settings dialog layout"}
    $window.Close()
}
Write-Output 'PASS every theme applies, persists, recolours live brushes in place and lays out the settings dialog'
Check ((Get-ChildItem -LiteralPath $testRoot -Filter '*.tmp' | Measure-Object).Count -eq 0) 'atomic saves leave no temporary files'

# ── migration of 1.1.0 names and other spellings ────────────────────────────
$legacy=TestPath 'legacy.json'
foreach($case in @(@('Classic','Phosphor'),@('Light','FieldOps'),@('HighContrast','HighContrast'),@('classic','Phosphor'),@('packapunch','PackAPunch'),@('Night Vision','NightVision'),@('field','FieldOps'))){
    [IO.File]::WriteAllText($legacy,('{"Theme":"'+$case[0]+'"}'))
    $warning=$null
    $pref=$TS::LoadPreference($legacy,[ref]$warning)
    if($pref.Theme.ToString() -ne $case[1] -or !$pref.Effects -or $warning){throw "FAIL '$($case[0])' should read as $($case[1])"}
}
Write-Output 'PASS Classic reads as Phosphor, Light as Field Ops, HighContrast stays; design ids and names accepted'
[IO.File]::WriteAllText($legacy,'{"Theme":"Classic"}')
$TM::Initialize($legacy)
Check ($TM::Current -eq $Mode::Phosphor -and !$TM::LoadWarning -and [IO.File]::ReadAllText($legacy) -eq '{"Theme":"Classic"}') 'a 1.1.0 Classic file opens as Phosphor and is not rewritten until the user changes something'

# ── unknown and damaged values fall back without crashing ───────────────────
foreach($text in @('{"Theme":"Neon"}','{broken','[1,2]','{"Theme":42}','','{}')){
    [IO.File]::WriteAllText($legacy,$text)
    $warning=$null
    $pref=$TS::LoadPreference($legacy,[ref]$warning)
    if($pref.Theme -ne $Mode::Undead -or !$pref.Effects -or !$warning){throw "FAIL '$text' did not fall back to Undead with a warning"}
}
$TM::Initialize($legacy)
Check ($TM::Current -eq $Mode::Undead -and $TM::LoadWarning) 'unknown or corrupt preferences fall back to Undead with a warning'
[IO.File]::WriteAllText($legacy,'{"Theme":"Arcade","Effects":"yes"}')
$warning=$null;$pref=$TS::LoadPreference($legacy,[ref]$warning)
Check ($pref.Theme -eq $Mode::Arcade -and $pref.Effects -and !$warning) 'a bad Effects value keeps the saved theme and the default effects'
[IO.File]::WriteAllText($legacy,'{"Theme":"Arcade","Effects":false}')
$warning=$null;$pref=$TS::LoadPreference($legacy,[ref]$warning)
Check ($pref.Theme -eq $Mode::Arcade -and !$pref.Effects) 'Effects:false is read back'

# ── Textures & glow ─────────────────────────────────────────────────────────
function Pixels($element,[int]$w,[int]$h){
    $element.Measure([Windows.Size]::new($w,$h));$element.Arrange([Windows.Rect]::new(0,0,$w,$h));$element.UpdateLayout()
    $bitmap=New-Object Windows.Media.Imaging.RenderTargetBitmap($w,$h,96,96,[Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($element)
    $bytes=New-Object byte[] ($w*$h*4)
    $bitmap.CopyPixels($bytes,$w*4,0)
    return ,$bytes
}
function Distinct($bytes){ $set=New-Object 'System.Collections.Generic.HashSet[int]'; for($i=0;$i -lt $bytes.Length;$i+=4){[void]$set.Add([BitConverter]::ToInt32($bytes,$i))}; return $set.Count }
$TM::Initialize($path)
$TM::Apply($Mode::Phosphor,$true,$true)
Check ($null -ne $app.Resources['Glow'] -and $null -ne $app.Resources['TextGlow'] -and $null -ne $app.Resources['OkGlow']) 'Phosphor with effects on has glow, ok glow and text glow'
$overlay=New-Object S2x.ServerManager.Views.ThemeOverlay
Check ((Distinct (Pixels $overlay 64 48)) -gt 1) 'the overlay draws scanlines and vignette with effects on'
$TM::SetEffects($false)
$warning=$null;$pref=$TS::LoadPreference($path,[ref]$warning)
Check (!$TM::EffectsEnabled -and !$pref.Effects -and $pref.Theme -eq $Mode::Phosphor) 'turning effects off is saved beside the theme'
Check ($null -eq $app.Resources['Glow'] -and $null -eq $app.Resources['TextGlow'] -and $null -eq $app.Resources['OkGlow'] -and $null -eq $app.Resources['CardShadow']) 'effects off removes glow, ok glow, text shadow and card shadow at once'
$overlayPixels=Pixels (New-Object S2x.ServerManager.Views.ThemeOverlay) 64 48
Check ((Distinct $overlayPixels) -eq 1 -and [BitConverter]::ToInt32($overlayPixels,0) -eq 0) 'effects off: the overlay draws nothing'
$TM::Apply($Mode::Arcade,$false,$false)
$backdrop=New-Object S2x.ServerManager.Views.ThemeBackdrop
Check ((Distinct (Pixels $backdrop 96 72)) -eq 1) 'effects off: the desk is one flat colour'
$TM::Apply($Mode::Arcade,$true,$false)
Check ((Distinct (Pixels (New-Object S2x.ServerManager.Views.ThemeBackdrop) 96 72)) -gt 1) 'effects on: the desk carries the theme texture'
$TM::Apply($Mode::HighContrast,$true,$false)
Check ($null -eq $app.Resources['Glow'] -and !$Res::For($Mode::HighContrast,$true).Effects) 'High contrast stays flat with the switch on'
$TM::Apply($Mode::Undead,$true,$true)

# ── keys the Manager does not know survive a save ───────────────────────────
$unknownKeys=TestPath 'unknown-keys.json'
[IO.File]::WriteAllText($unknownKeys,'{"Theme":"Undead","Effects":true,"CloseToTray":false}')
$TM::Initialize($unknownKeys)
$TM::Apply($Mode::Prestige)
$saved=New-Object System.Web.Script.Serialization.JavaScriptSerializer
$values=$saved.DeserializeObject([IO.File]::ReadAllText($unknownKeys))
Check ($values['Theme'] -eq 'Prestige' -and $values['Effects'] -eq $true -and $values.ContainsKey('CloseToTray')) 'saving keeps keys this version does not know'

# ── a save that fails leaves the visible theme and effects as they were ─────
$blocked=TestPath $blockedName
New-Item -ItemType Directory -Force $blocked | Out-Null
$TM::Initialize($blocked)
$before=$app.Resources['Glow']
try{$TM::Apply($Mode::FieldOps);throw 'Write failure was not reported'}catch{if($_.Exception.Message -eq 'Write failure was not reported'){throw}}
Check ($TM::Current -eq $Mode::Undead -and $ink.Color -eq $Res::For($Mode::Undead,$true).Colors['Ink']) 'failed preference save preserves visible theme'
try{$TM::SetEffects($false);throw 'Write failure was not reported'}catch{if($_.Exception.Message -eq 'Write failure was not reported'){throw}}
Check ($TM::EffectsEnabled -and [object]::ReferenceEquals($app.Resources['Glow'],$before)) 'failed preference save preserves Textures & glow'
$dialog=New-Object S2x.ServerManager.Views.SettingsDialog
$picked=$dialog.Pick($Mode::Outrun)
$selected=@($dialog.Tiles | Where-Object { $_.IsSelected })
Check (!$picked -and $TM::Current -eq $Mode::Undead -and $selected.Count -eq 1 -and $selected[0].Mode -eq $Mode::Undead -and $dialog.FindName('message').Text.Length -gt 0) 'settings dialog: a failed save says so and keeps the ring on the visible theme'
$toggled=$dialog.SetEffects($false)
Check (!$toggled -and $TM::EffectsEnabled -and $dialog.FindName('effects').IsChecked -eq $true) 'settings dialog: a failed switch save puts the switch back'
$dialog.Close()
Remove-Item -LiteralPath $blocked

# ── the settings dialog ─────────────────────────────────────────────────────
$TM::Initialize((TestPath 'settings-dialog.json'))
$dialog=New-Object S2x.ServerManager.Views.SettingsDialog
Check ($dialog.Tiles.Count -eq 9 -and (($dialog.Tiles | ForEach-Object { $_.Name }) -join '|') -eq 'Field Ops|Undead|Phosphor|Outrun|Pack-a-Punch|Night Vision|Arcade|Prestige|High contrast') 'settings dialog shows a tile for each theme'
Check ($dialog.Pick($Mode::NightVision) -and $TM::Current -eq $Mode::NightVision -and @($dialog.Tiles | Where-Object { $_.IsSelected })[0].Mode -eq $Mode::NightVision) 'picking a tile applies it and moves the ring'
$dialog.FindName('effects').IsChecked=$false
$warning=$null;$pref=$TS::LoadPreference((TestPath 'settings-dialog.json'),[ref]$warning)
Check (!$TM::EffectsEnabled -and !$pref.Effects -and $pref.Theme -eq $Mode::NightVision) 'the Textures & glow switch applies and saves'
$dialog.FindName('effects').IsChecked=$true
Check ($TM::EffectsEnabled) 'the switch turns effects back on'
$dialog.Close()

# ── CSS tokens to WPF ───────────────────────────────────────────────────────
$start=New-Object Windows.Point;$end=New-Object Windows.Point
foreach($case in @(@(180,0.5,0,0.5,1),@(90,0,0.5,1,0.5),@(0,0.5,1,0.5,0),@(135,0,0,1,1),@(45,0,1,1,0))){
    $Css::AnglePoints($case[0],[ref]$start,[ref]$end)
    if(!(Near $start.X $case[1]) -or !(Near $start.Y $case[2]) -or !(Near $end.X $case[3]) -or !(Near $end.Y $case[4])){throw "FAIL $($case[0])deg maps to $start - $end"}
}
Write-Output 'PASS CSS gradient angles map to WPF start and end points (0, 45, 90, 135, 180deg)'
$gradient=$Css::LinearGradient('linear-gradient(135deg,#6a1bff,#b44cff 55%,#ff4dd8)')
Check ($gradient.GradientStops.Count -eq 3 -and (Near $gradient.GradientStops[1].Offset 0.55) -and (Near $gradient.GradientStops[2].Offset 1) -and $gradient.IsFrozen) 'gradient stops keep their CSS positions'
$fade=$Css::LinearGradient('linear-gradient(180deg, rgba(255,42,109,.06), transparent 30%)')
Check ($fade.GradientStops[1].Color.A -eq 0 -and $fade.GradientStops[1].Color.R -eq 255 -and (Near $fade.GradientStops[1].Offset 0.3)) 'transparent stops fade the neighbouring colour, not black'
$hard=$Css::Shadow('4px 4px 0 #000')
Check ((Near $hard.ShadowDepth ([Math]::Sqrt(32))) -and (Near $hard.Direction 315) -and $hard.BlurRadius -eq 0 -and (Near $hard.Opacity 1)) 'hard offset shadow: depth and direction, no blur'
$glow=$Css::Shadow('0 0 22px rgba(195,20,31,.55)')
Check ($glow.ShadowDepth -eq 0 -and $glow.BlurRadius -eq 22 -and (Near $glow.Opacity 0.55) -and $glow.Color.R -eq 195) 'glow: no offset, blur and alpha kept'
$drop=$Css::Shadow('inset 0 0 0 1px rgba(0,0,0,.35), 0 3px 0 rgba(0,0,0,.35)')
Check ((Near $drop.ShadowDepth 3) -and (Near $drop.Direction 270)) 'inset layers are skipped, a downward shadow points down'
$ring=$Css::Shadow('0 0 0 1px rgba(255,42,109,.12), 0 12px 40px rgba(13,2,33,.8)')
Check ((Near $ring.ShadowDepth 12) -and $ring.BlurRadius -eq 40) 'spread-only rings are skipped for the next layer'
Check ($null -eq $Css::Shadow('none')) 'none is no effect'
Check ((Near ($Css::MixAlpha('color-mix(in oklab, currentColor 14%, transparent)')) 0.14) -and $Css::MixAlpha('transparent') -eq 0) 'color-mix badge tint becomes an alpha'
$skew=$Css::ParseTransform('skewX(-10deg)');$tilt=$Css::ParseTransform('rotate(-2deg)')
Check ($skew.AngleX -eq -10 -and $tilt.Angle -eq -2 -and $Css::ParseTransform('none').Value.IsIdentity) 'badge transforms: skewX, rotate, none'

# ── badges per theme ────────────────────────────────────────────────────────
$TM::Apply($Mode::PackAPunch,$true,$false)
Check ($app.Resources['BadgeTransform'] -is [Windows.Media.SkewTransform]) 'Pack-a-Punch badges skew'
$TM::Apply($Mode::FieldOps,$true,$false)
Check ($app.Resources['BadgeTransform'] -is [Windows.Media.RotateTransform] -and $app.Resources['BadgeBorder'].Left -eq 2 -and $app.FindResource('OkTint').Color.A -eq 0) 'Field Ops badges tilt, 2px, untinted'
$TM::Apply($Mode::NightVision,$true,$false)
Check ($app.Resources['BadgeDashedVisibility'] -eq [Windows.Visibility]::Visible -and $app.Resources['BadgeSolidVisibility'] -eq [Windows.Visibility]::Collapsed) 'Night Vision badges are dashed'
$TM::Apply($Mode::Prestige,$true,$false)
Check ($app.Resources['BadgeDoubleVisibility'] -eq [Windows.Visibility]::Visible) 'Prestige badges are double-ruled'
$TM::Apply($Mode::Outrun,$true,$false)
Check ($app.Resources['RadiusBadgeValue'] -eq 999 -and $app.FindResource('OkTint').Color.A -eq [Math]::Round(0.14*255)) 'Outrun badges are pills tinted at 14%'

# ── fonts ───────────────────────────────────────────────────────────────────
$embedded=$Fonts::EmbeddedFontFiles()
foreach($theme in $Cat::All){
    foreach($family in @($theme.DisplayFont,$theme.BodyFont,$theme.LabelFont,$theme.MonoFont)){
        $spec=$Fonts::Find($family)
        if($null -eq $spec){throw "FAIL $($theme.Name) names $family, which has no font spec"}
        $resolved=$Fonts::Resolve($family,$true)
        if($null -eq $resolved.Family -or !$resolved.Used){throw "FAIL $family did not resolve"}
        if($embedded.Count -eq 0 -and $resolved.Embedded){throw "FAIL $family claims to be embedded with no font files built in"}
    }
}
Write-Output ("PASS every theme font role resolves (" + $embedded.Count + " embedded font files; the rest use installed fallbacks)")
# A font dropped into Assets/Fonts must be loadable from the exe: WPF cannot open a pack
# resource with [ ] in its name (google/fonts' Family[wght].ttf), so such a file would be
# built in and silently never used.
$fontBase=New-Object Uri(('pack://application:,,,/' + $assembly.GetName().Name + ';component/'))
foreach($key in $embedded){
    if($key -match '[\[\]]'){throw "FAIL $key has square brackets; rename it (see Assets/Fonts/README.md)"}
    try{ [void](New-Object Windows.Media.GlyphTypeface((New-Object Uri($fontBase,$key)))) }catch{ throw "FAIL $key cannot be loaded from the exe: $($_.Exception.Message)" }
}
Write-Output 'PASS every embedded font file loads from the exe'

# ── contrast floors (WCAG ratios on the chrome colour) ──────────────────────
foreach($m in $modes){
    $c=$Res::For($m,$true).Colors
    $floors=@(@('Ink','Bar',7),@('Label','Bar',4.5),@('Muted','Bar',4.5),@('Dim','Bar',4.5),@('Faint','Bar',4),@('Ink','Surface',7),@('Accent','Bar',3),@('Ok','Bar',3),@('Danger','Bar',3),@('Accent','Surface',3))
    foreach($f in $floors){
        $ratio=$Res::Contrast($c[$f[0]],$c[$f[1]])
        if($ratio -lt $f[2]){throw ("FAIL {0} {1} on {2} is {3:N2}, under {4}" -f $m,$f[0],$f[1],$ratio,$f[2])}
    }
    # Button text sits in the middle of the fill: 4.0 there, and never under 2.85 at an end
    # (Pack-a-Punch's white is the lowest: the design's own 4.1 mid-gradient, 2.87 at the pink end).
    $fill=$Res::For($m,$true).Fill
    if($fill -is [Windows.Media.SolidColorBrush]){$stops=@($fill.Color);$middle=$fill.Color}
    else{
        $stops=@($fill.GradientStops | ForEach-Object { $_.Color })
        $list=@($fill.GradientStops | Sort-Object Offset)
        $after=@($list | Where-Object { $_.Offset -ge 0.5 })[0]; $before=@($list | Where-Object { $_.Offset -le 0.5 })[-1]
        $t=if($after.Offset -eq $before.Offset){0}else{(0.5-$before.Offset)/($after.Offset-$before.Offset)}
        $middle=$Res::Mix($before.Color,$after.Color,$t)
    }
    foreach($stop in $stops){ if($Res::Contrast($c['FillInk'],$stop) -lt 2.85){throw "FAIL $m button text on its fill's end"} }
    if($Res::Contrast($c['FillInk'],$middle) -lt 4.0){throw "FAIL $m button text on its fill's middle"}
}
Write-Output 'PASS text, status and button colours meet their contrast floors in every theme'
}
finally {
    if($app){$app.Shutdown()}
    # Delete only known test files and the uniquely-created empty test directories.
    foreach($name in $files){ $file=Join-Path $testRoot $name; if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file} }
    $blocked=Join-Path $testRoot $blockedName
    if(Test-Path -LiteralPath $blocked){Remove-Item -LiteralPath $blocked}
    Remove-Item -LiteralPath $testRoot
}
