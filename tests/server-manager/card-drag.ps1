param([string]$Assembly=(Join-Path $PSScriptRoot '../../tools/ServerManager/bin/Release/net48/S2xServerManager.exe'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationFramework
$asm=[Reflection.Assembly]::LoadFrom((Resolve-Path $Assembly).Path)
$static=[Reflection.BindingFlags]'Static,Public,NonPublic'
$instance=[Reflection.BindingFlags]'Instance,Public,NonPublic'
$behavior=$asm.GetType('S2x.ServerManager.Views.CardReorder')
$fleetType=$asm.GetType('S2x.ServerManager.ViewModels.FleetViewModel')
$payloadType=$behavior.GetNestedType('Payload',[Reflection.BindingFlags]'NonPublic')
function Raw($value) { if($null -eq $value){return $null}; return $value.PSObject.BaseObject }
function InvokeStatic($type,[string]$name,[object[]]$arguments=@()) {
 for($i=0;$i -lt $arguments.Count;$i++) { $arguments[$i]=Raw $arguments[$i] }
 $type.GetMethod($name,$static).Invoke($null,$arguments)
}
function Check($actual,$expected,[string]$label) { if($actual -ne $expected){throw "$label expected=$expected actual=$actual"}; Write-Output "PASS $label" }
function Demo { InvokeStatic $fleetType 'Demo' @('mixed') }
function Payload($fleet,$card) {
 $value=[Activator]::CreateInstance($payloadType,$true)
 $payloadType.GetField('Fleet').SetValue($value,(Raw $fleet))
 $payloadType.GetField('Card').SetValue($value,(Raw $card))
 return $value
}
function DragArgs($payload,$target,[double]$x) {
 $data=New-Object System.Windows.DataObject
 $data.SetData($payloadType,(Raw $payload))
 $ctor=[System.Windows.DragEventArgs].GetConstructors($instance)[0]
 $point=New-Object System.Windows.Point($x,50)
 $arguments=[object[]]@($data,[System.Windows.DragDropKeyStates]::LeftMouseButton,[System.Windows.DragDropEffects]::Move,$target,$point); for($i=0;$i -lt $arguments.Count;$i++){$arguments[$i]=Raw $arguments[$i]}; $eventArgs=$ctor.Invoke($arguments); $eventArgs.RoutedEvent=[System.Windows.UIElement]::DragOverEvent; return $eventArgs
}
function Hint($element) { InvokeStatic $behavior 'GetHint' @($element) }
# Disconnected WPF tree only: no Window.Show, desktop input, servers, or user settings.
$fleet=Demo; $root=New-Object System.Windows.Controls.Grid; $root.DataContext=$fleet
$card=New-Object System.Windows.Controls.Grid; $card.Width=300; $card.Height=200
$card.DataContext=$fleet.Shown[1]; [void]$root.Children.Add($card)
$null=InvokeStatic $behavior 'SetEnabled' @($card,$true)
$root.Measure((New-Object System.Windows.Size(400,300)))
$root.Arrange((New-Object System.Windows.Rect(0,0,400,300))); $root.UpdateLayout()
$first=$fleet.Shown[0]; $second=$fleet.Shown[1]; $payload=Payload $fleet $first
$before=DragArgs $payload $card 30
$null=InvokeStatic $behavior 'Over' @($card,$before)
Check (Hint $card) 'Place before' 'left half hint'
Check $before.Effects ([System.Windows.DragDropEffects]::Move) 'same-fleet move allowed'
$after=DragArgs $payload $card 270
$null=InvokeStatic $behavior 'Over' @($card,$after)
Check (Hint $card) 'Place after' 'right half hint'
$null=InvokeStatic $behavior 'Drop' @($card,$after)
Check ($fleet.Shown[0] -eq $second) $true 'actual drop reordered fleet'
Check ($fleet.Shown[1] -eq $first) $true 'actual drop placed dragged card after target'
Check (Hint $card) '' 'drop clears hint'
$other=Demo; $wrong=DragArgs (Payload $other $other.Shown[0]) $card 30
$null=InvokeStatic $behavior 'Over' @($card,$wrong)
Check $wrong.Effects ([System.Windows.DragDropEffects]::None) 'cross-fleet rejection'
Check (Hint $card) '' 'cross-fleet has no hint'
$null=InvokeStatic $behavior 'Drop' @($card,$wrong)
Check ($fleet.Shown[0] -eq $second) $true 'cross-fleet drop unchanged'
$self=DragArgs (Payload $fleet $second) $card 30
$null=InvokeStatic $behavior 'Over' @($card,$self)
Check $self.Effects ([System.Windows.DragDropEffects]::None) 'self drop rejected'
$null=InvokeStatic $behavior 'Over' @($card,(DragArgs $payload $card 30))
$null=InvokeStatic $behavior 'Leave' @($card,(DragArgs $payload $card 30))
Check (Hint $card) '' 'drag leave clears hint'
# Verify actual visual ancestry inside a templated button, not only the button itself.
$button=New-Object System.Windows.Controls.Button
$text=New-Object System.Windows.Controls.TextBlock; $text.Text='Start'
$button.Content=$text; [void]$card.Children.Add($button)
$root.Measure((New-Object System.Windows.Size(400,300))); $root.Arrange((New-Object System.Windows.Rect(0,0,400,300)))
[void]$button.ApplyTemplate(); $root.UpdateLayout()
Check (InvokeStatic $behavior 'Interactive' @($text,$card)) $true 'button text ancestry excluded'
foreach($type in @([System.Windows.Controls.TextBox],[System.Windows.Controls.PasswordBox],[System.Windows.Controls.ComboBox],[System.Windows.Controls.Primitives.Thumb])) {
 $input=[Activator]::CreateInstance($type); [void]$card.Children.Add($input)
 Check (InvokeStatic $behavior 'Interactive' @($input,$card)) $true "$($type.Name) excluded"
}
$plain=New-Object System.Windows.Controls.TextBlock; [void]$card.Children.Add($plain)
Check (InvokeStatic $behavior 'Interactive' @($plain,$card)) $false 'plain card text eligible'
$null=InvokeStatic $behavior 'Over' @($card,(DragArgs $payload $card 30))
$null=InvokeStatic $behavior 'SetEnabled' @($card,$false)
Check (Hint $card) '' 'disable clears hint'
Check $card.AllowDrop $false 'disable removes drop participation'
Write-Output 'All disconnected-WPF CardReorder behavior checks passed.'


