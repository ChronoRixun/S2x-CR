param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
# Run with Windows PowerShell -STA after building Release. Lays out the real windows off-screen
# (demo data only: no game folder, no presets, no servers, no network) in every theme and fails
# when text is cut by a clip without an ellipsis: a label, a button, a stat, a title. Text that
# trims ("CR's Small Map Mo...") or wraps is allowed; text scrolled out of a scroll viewer is
# not a cut. The settings file is a temp path that is never written.
$ErrorActionPreference='Stop'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-layout-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$assembly=[Reflection.Assembly]::LoadFrom(([IO.Path]::GetFullPath($AssemblyPath)))
# A plain Application with the Manager's Theme.xaml, never S2x.ServerManager.App: an Application
# queues its Startup when it is created, this test lets the dispatcher run (Loaded, bindings), and
# App's OnStartup would then start the real Manager on the real game folder and settings.
$app=New-Object System.Windows.Application
$app.ShutdownMode='OnExplicitShutdown'
$app.Resources.MergedDictionaries.Add([Windows.Application]::LoadComponent((New-Object Uri(('/' + $assembly.GetName().Name + ';component/theme.xaml'),[UriKind]::Relative))))
$TM=[S2x.ServerManager.Services.ThemeManager]
$TM::Initialize((Join-Path $testRoot 'never-written.json'))
# The Startup the Application queued runs here, before any window: it is the base one and does nothing.
$app.Dispatcher.Invoke([Action]{}, [Windows.Threading.DispatcherPriority]::ContextIdle)
if($TM::Current -ne [S2x.ServerManager.Services.ThemeMode]::Undead){ throw 'Something changed the theme at startup' }
$fleetType=$assembly.GetType('S2x.ServerManager.ViewModels.FleetViewModel')
$static=[Reflection.BindingFlags]'Static,Public,NonPublic'
$VTH=[Windows.Media.VisualTreeHelper]
$LI=[Windows.Controls.Primitives.LayoutInformation]

function Walk($node,[scriptblock]$visit){
    & $visit $node
    for($i=0;$i -lt $VTH::GetChildrenCount($node);$i++){ Walk ($VTH::GetChild($node,$i)) $visit }
}
function Visible($element){
    for($e=$element;$e -ne $null;$e=$VTH::GetParent($e)){
        if($e -is [Windows.UIElement] -and ($e.Visibility -ne [Windows.Visibility]::Visible -or $e.Opacity -eq 0)){ return $false }
    }
    return $true
}
function Label($block){
    $text=$block.Text
    if([string]::IsNullOrEmpty($text)){ $text=(($block.Inlines | ForEach-Object { if($_ -is [Windows.Documents.Run]){$_.Text} }) -join '') }
    return ($text -replace [char]0x2009,'')
}
# Every visible TextBlock that neither trims nor wraps must fit inside every clip above it,
# up to the nearest scroll viewer (whose clip is scrolling, not cutting).
function Cuts($window,[string]$where){
    $found=New-Object System.Collections.Generic.List[string]
    Walk $window {
        param($node)
        if($node -isnot [Windows.Controls.TextBlock]){ return }
        $block=$node
        if($block.TextTrimming -ne [Windows.TextTrimming]::None -or $block.TextWrapping -ne [Windows.TextWrapping]::NoWrap){ return }
        if($block.ActualWidth -le 0 -or !(Visible $block)){ return }
        $label=Label $block
        if([string]::IsNullOrWhiteSpace($label)){ return }
        # Its own clip: arranged narrower than it needs.
        $clip=$LI::GetLayoutClip($block)
        if($clip -ne $null -and $clip.Bounds.Width -lt $block.ActualWidth - 1){ $found.Add("$where : '$label' cut to $([int]$clip.Bounds.Width) of $([int]$block.ActualWidth) px"); return }
        $box=New-Object Windows.Rect(0,0,$block.ActualWidth,$block.ActualHeight)
        for($e=$VTH::GetParent($block);$e -ne $null -and $e -ne $window;$e=$VTH::GetParent($e)){
            if($e -is [Windows.Controls.ScrollContentPresenter] -or $e -is [Windows.Controls.Primitives.Popup]){ break }
            if($e -isnot [Windows.FrameworkElement]){ continue }
            $clip=$LI::GetLayoutClip($e)
            if($clip -eq $null -and $e.Clip -ne $null){ $clip=$e.Clip }
            if($clip -eq $null){ continue }
            $inside=$block.TransformToAncestor($e).TransformBounds($box)
            $bounds=$clip.Bounds
            # Horizontal cuts only: rounded-corner clips and baseline rounding shave a pixel or two.
            if($inside.Left -lt $bounds.Left - 1.5 -or $inside.Right -gt $bounds.Right + 1.5){
                $found.Add("$where : '$label' cut by $($e.GetType().Name) ($([int]$inside.Left)-$([int]$inside.Right) outside $([int]$bounds.Left)-$([int]$bounds.Right))"); return
            }
        }
    }
    return ,$found
}
function Show($window,[double]$width,[double]$height){
    $window.WindowStartupLocation='Manual'; $window.Left=-20000; $window.Top=-20000
    $window.ShowInTaskbar=$false; $window.ShowActivated=$false
    if($width -gt 0){ $window.Width=$width; $window.Height=$height }
    $window.Show(); $window.UpdateLayout()
    $app.Dispatcher.Invoke([Action]{}, [Windows.Threading.DispatcherPriority]::ContextIdle)
    $window.UpdateLayout()
}
function Fleet([string]$kind,[string]$state){
    $fleet=$fleetType.GetMethod($kind,$static).Invoke($null,@($state))
    return $fleet
}
# The chat commands and map vote sections at their fullest: five long rules (each over the 120
# bytes the server takes) and a sixth, so the counter is red and both rules notes show, a
# Discord line longer than the box, and the vote on with every note (the demo's empty rotation
# adds the three-entries one). One screen, on the stopped demo server, because every window
# this run opens makes the ones after it slower to lay out.
$longRules=@(
    'Be respectful to every player on the server, whatever team they are on, and keep the chat clean and friendly for everyone who joins',
    'No spawn camping, spawn trapping or boosting; play the objective when the mode has one and do not hold the match hostage for kills',
    'No cheating, exploits, macros or glitching out of the map; report anyone you see doing it in our Discord with a clip if you can',
    'English in all chat please, so the admins can read it; team chat for call-outs, and keep voice chat free of music and background noise',
    'Have fun, say gg at the end of the match, and invite your friends: the server fills faster with humans and the bots leave as you join',
    'A sixth rule, one more than the server takes')
function ChatAndVote($fleet){
    $editor=$fleet.Editor
    $editor.ChatCommands=$true
    $editor.RulesText=$longRules -join [Environment]::NewLine
    $editor.Discord='https://discord.gg/a-very-long-invite-code-for-the-whole-community-server'
    $editor.MapVote=$true
    return $fleet
}

$screens=@(
    @{Name='fleet'; Make={ $f=Fleet 'Demo' 'three-notanswering'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='fleet crashed'; Make={ $f=Fleet 'Demo' 'three-crashed'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='fleet empty'; Make={ $f=Fleet 'Demo' 'empty'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='roster'; Make={ $f=Fleet 'Demo' 'three-notanswering'; $f.ViewMode='roster'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='editor mp'; Make={ $f=Fleet 'DemoEditor' 'mp'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='editor zombies'; Make={ $f=Fleet 'DemoEditor' 'zombies'; $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='editor chat and vote'; Make={ $f=ChatAndVote (Fleet 'DemoEditor' 'empty'); $w=New-Object S2x.ServerManager.MainWindow; $w.DataContext=$f; $w }},
    @{Name='settings'; Size=@(0,0); Make={ New-Object S2x.ServerManager.Views.SettingsDialog }}
)
$failures=New-Object System.Collections.Generic.List[string]
foreach($mode in [Enum]::GetValues([S2x.ServerManager.Services.ThemeMode])){
    $TM::Apply($mode,$true,$false)
    foreach($screen in $screens){
        # The default size and the minimum the main window allows.
        $sizes=if($screen.Size){ ,@(0,0) } else { @(@(1160,740),@(1000,620)) }
        foreach($size in $sizes){
            $window=& $screen.Make
            Show $window $size[0] $size[1]
            $where="$mode $($screen.Name)" + $(if($size[0] -gt 0){" $($size[0])x$($size[1])"}else{''})
            foreach($cut in (Cuts $window $where)){ $failures.Add($cut) }
            $window.Close()
        }
    }
    Write-Output "PASS $mode checked: fleet (three states), roster, editor (mp, zombies, chat commands and map vote) at 1160x740 and 1000x620, settings"
}
if($failures.Count){ $failures | ForEach-Object { Write-Output "CUT $_" }; throw "FAIL $($failures.Count) pieces of text are cut without an ellipsis" }
Write-Output 'PASS no text is cut without an ellipsis in any theme'
}
finally {
    if($app){ $app.Shutdown() }
    $never=Join-Path $testRoot 'never-written.json'
    if(Test-Path -LiteralPath $never){ throw 'The layout test wrote settings' }
    Remove-Item -LiteralPath $testRoot
}
