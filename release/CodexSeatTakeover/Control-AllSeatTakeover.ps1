param([ValidateSet('Apply','Disable','Status')][string]$Action='Apply',[switch]$StartServer)
$ErrorActionPreference='Stop'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$admin=([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$admin) {
    # BF4 is elevated on this installation; Windows requires matching rights.
    # The ordinary UAC prompt is left to the user; it is never bypassed.
    $arguments='-NoProfile -ExecutionPolicy RemoteSigned -File "'+$PSCommandPath+'" -Action '+$Action
    if ($StartServer) {$arguments+=' -StartServer'}
    $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    exit $child.ExitCode
}
$server=Join-Path $PSScriptRoot 'Control-SeatTakeover.ps1'
$client=Join-Path $PSScriptRoot 'Client\Control-DeployClient.ps1'
$stop=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,'Local\CodexBF4DeployWatcherStop')
try {
    if ($Action -eq 'Disable') {
        [void]$stop.Set()
        & $client -Action Disable
        & $server -Action Disable
        return
    }
    if ($Action -eq 'Status') {& $server -Action Status;& $client -Action Status;return}
    & $server -Action Apply -StartServer:$StartServer
    if (!$?) {throw 'Server companion controller failed.'}
    $process=Get-Process bf4_Server_Final -ErrorAction Stop | Where-Object {$_.Path -eq (Join-Path (Split-Path -Parent $PSScriptRoot) 'bf4_Server_Final.exe')} | Select-Object -First 1
    if (!$process) {throw 'Expected BF4 server process not found.'}
    if (Get-Process bf4_no_drm -ErrorAction SilentlyContinue) {& $client -Action Apply;if (!$?) {throw 'Client companion controller failed.'}}
    [void]$stop.Reset()
    $watcher=Join-Path $PSScriptRoot 'Client\Watch-DeployClient.ps1'
    $arguments='-NoProfile -ExecutionPolicy RemoteSigned -File "'+$watcher+'" -ServerPID '+$process.Id
    Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -WindowStyle Hidden
    Write-Output 'Server and spawn UI enabled; client restarts will be watched while this server runs.'
} finally {$stop.Dispose()}
