param([string]$AssemblyPath = (Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
$ErrorActionPreference='Stop'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('s2x-admin-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$helpers=@()
try {
    Add-Type -AssemblyName System.Web.Extensions
    [Reflection.Assembly]::LoadFrom([IO.Path]::GetFullPath($AssemblyPath)) | Out-Null
    $fixture=Join-Path $testRoot 'AdminPipeFixture.exe'
    Add-Type -TypeDefinition (Get-Content (Join-Path $PSScriptRoot 'AdminPipeFixture.cs') -Raw) -ReferencedAssemblies System.Web.Extensions -OutputAssembly $fixture -OutputType ConsoleApplication
    $store=New-Object S2x.ServerManager.Services.ManagedServerOwnershipStore($testRoot)
    $client=New-Object S2x.ServerManager.Services.ServerAdminClient($store)
    $port=55443
    $missing=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
    if($missing.ok -or $missing.message -notmatch 'No action was sent'){throw 'Unowned server was not refused'}
    Write-Output 'PASS unowned server refused before sending'
    foreach($scenario in @('success','wrong-id','oversize','silent')) {
        $nonce=[Guid]::NewGuid().ToString('N')
        $process=Start-Process -FilePath $fixture -ArgumentList @($scenario,$nonce,'-dedicated','+set','net_port',$port,'-server-manager-admin',$nonce) -WindowStyle Hidden -PassThru
        $helpers+=$process
        $store.Register($process.Id,$port,$nonce)
        $record=$store.Read($port)
        if($record.Instance -ne $nonce -or $record.Pid -ne $process.Id -or $record.ExePath -ne [IO.Path]::GetFullPath($fixture)){throw 'Ownership roundtrip/native executable path failed'}
        $watch=[Diagnostics.Stopwatch]::StartNew()
        $reply=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
        if($scenario -eq 'success') {
            if(!$reply.ok -or $reply.players.Count -ne 1 -or $reply.players[0].name -ne 'Fixture human'){throw 'Valid correlated pipe reply failed'}
        } elseif($reply.ok){throw "$scenario was accepted"}
        if($scenario -eq 'silent' -and $watch.Elapsed.TotalSeconds -gt 10){throw 'Silent pipe exceeded bounded wait'}
        Write-Output "PASS $scenario reply handling"
        if(!$process.HasExited){if($process.Path -ne $fixture){throw 'Fixture identity changed'};$process.Kill();$process.WaitForExit()}
        $stale=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
        if($stale.ok -or $stale.message -notmatch 'No action was sent'){throw 'Stale process record accepted'}
    }
    Write-Output 'PASS exited process records refused'
    $nonce=[Guid]::NewGuid().ToString('N')
    $owner=Start-Process -FilePath $fixture -ArgumentList @('idle',$nonce,'-dedicated','+set','net_port',$port,'-server-manager-admin',$nonce) -WindowStyle Hidden -PassThru
    $helpers+=$owner
    $store.Register($owner.Id,$port,$nonce)
    $refused=$false
    try{$store.Register($owner.Id,$port,[Guid]::NewGuid().ToString('N'))}catch{$refused=$true}
    if(!$refused -or $store.Read($port).Instance -ne $nonce){throw 'Mismatched launch nonce adopted or overwritten'}
    Write-Output 'PASS launch nonce mismatch cannot adopt a process'
    # An owned process without a bridge stands in for a pre-1.1.1 s2x.exe.
    $reply=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
    if($reply.ok -or $reply.message -notmatch 'No administration bridge answered' -or $reply.message -notmatch 'No action was sent'){throw 'Missing bridge was not explained'}
    Write-Output 'PASS server without a bridge explained, nothing sent'
    $impostor=Start-Process -FilePath $fixture -ArgumentList @('success',$nonce) -WindowStyle Hidden -PassThru
    $helpers+=$impostor
    $reply=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
    if($reply.ok -or $reply.message -notmatch 'another process'){throw 'Wrong pipe owner PID accepted'}
    Write-Output 'PASS wrong pipe server PID refused'
    foreach($helper in @($owner,$impostor)){if(!$helper.HasExited -and $helper.Path -eq $fixture){$helper.Kill();$helper.WaitForExit()}}
    $nonce=[Guid]::NewGuid().ToString('N')
    $owner=Start-Process -FilePath $fixture -ArgumentList @('idle',$nonce,'-dedicated','+set','net_port',$port,'-server-manager-admin',$nonce) -WindowStyle Hidden -PassThru
    $helpers+=$owner
    $store.Register($owner.Id,$port,$nonce)
    $record=$store.Read($port)
    $record.CreatedUtcTicks--
    $serializer=New-Object Web.Script.Serialization.JavaScriptSerializer
    [IO.File]::WriteAllText((Join-Path $testRoot "$port.json"),$serializer.Serialize($record))
    $reply=$client.RequestAsync($port,'players',$null,$null).GetAwaiter().GetResult()
    if($reply.ok -or $reply.message -notmatch 'identity no longer matches'){throw 'Mismatched creation time accepted'}
    Write-Output 'PASS reused PID creation-time mismatch refused'
    $owner.Kill();$owner.WaitForExit()
    $nonce=[Guid]::NewGuid().ToString('N')
    $owner=Start-Process -FilePath $fixture -ArgumentList @('success',$nonce,'-dedicated','+set','net_port',$port,'-server-manager-admin',$nonce) -WindowStyle Hidden -PassThru
    $helpers+=$owner
    $store.Register($owner.Id,$port,$nonce)
    $audit=Join-Path $testRoot 'actions.jsonl'
    New-Item -ItemType Directory -Path $audit | Out-Null
    $reply=$client.RequestAsync($port,'announce',$null,'Synthetic test only').GetAwaiter().GetResult()
    if(!$reply.ok -or !$reply.AuditWarning){throw 'Audit failure lost accepted action result'}
    Remove-Item -LiteralPath $audit
    if(!$owner.HasExited){$owner.Kill();$owner.WaitForExit()}
    Write-Output 'PASS accepted action retained when audit write fails'

    if([S2x.ServerManager.Services.ManagedServerOwnershipStore]::HasNonce('-server-manager-admin bad','bad')){throw 'Invalid nonce accepted'}
    $mutation=$client.RequestAsync($port,'kick','stale-token','Fixture reason').GetAwaiter().GetResult()
    if($mutation.ok -or !(Test-Path (Join-Path $testRoot 'actions.jsonl'))){throw 'Failure audit missing'}
    Write-Output 'PASS local structured failure audit'
    Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
    $app=New-Object S2x.ServerManager.App
    $app.InitializeComponent()
    [S2x.ServerManager.Services.ThemeManager]::Initialize((Join-Path $testRoot 'unused-theme.json'))
    $window=New-Object S2x.ServerManager.Views.AdminWindow($port,'Fixture server',$client)
    $flags=[Reflection.BindingFlags]'Instance,NonPublic'
    $window.GetType().GetField('_available',$flags).SetValue($window,$true)
    $window.GetType().GetField('_notices',$flags).SetValue($window,$true)
    $window.FindName('message').Text='Synthetic message'
    $player=New-Object S2x.ServerManager.Services.AdminPlayer
    $player.name='Fixture human';$player.token='fixture-only'
    $window.FindName('players').ItemsSource=@($player)
    $window.FindName('players').SelectedItem=$player
    if(!$window.FindName('warn').IsEnabled -or !$window.FindName('kick').IsEnabled){throw 'Human action controls unavailable'}
    foreach($kind in @('isBot','isHost')){
        $player.$kind=$true
        $window.GetType().GetMethod('UpdateButtons',$flags).Invoke($window,@())|Out-Null
        if($window.FindName('warn').IsEnabled -or $window.FindName('kick').IsEnabled){throw 'Protected target controls enabled'}
        $player.$kind=$false
    }
    $window.GetType().GetField('_notices',$flags).SetValue($window,$false)
    $window.GetType().GetMethod('UpdateButtons',$flags).Invoke($window,@())|Out-Null
    if($window.FindName('announce').IsEnabled -or $window.FindName('warn').IsEnabled -or !$window.FindName('kick').IsEnabled){throw 'Notice readiness gating failed'}
    Write-Output 'PASS human/bot/host and notice-readiness UI gating'

}
finally {
    if($app){$app.Shutdown()}
    foreach($process in $helpers){try{if(!$process.HasExited -and $process.Path -eq $fixture){$process.Kill();$process.WaitForExit()}}catch{}}
    # All files are test-created direct children; never recursively delete a computed path.
    foreach($name in @('AdminPipeFixture.exe','55443.json','actions.jsonl')){
        $file=Join-Path $testRoot $name
        if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file}
    }
    Remove-Item -LiteralPath $testRoot
}
