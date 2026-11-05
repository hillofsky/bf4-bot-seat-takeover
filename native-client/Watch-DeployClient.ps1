param([Parameter(Mandatory=$true)][int]$ServerPID)
$ErrorActionPreference='Stop'
$created=$false
$mutex=[Threading.Mutex]::new($true,'Local\CodexBF4DeployClientWatcher',[ref]$created)
if (!$created) {$mutex.Dispose();return}
$stop=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,'Local\CodexBF4DeployWatcherStop')
$loadedPID=0;$nextAttempt=[DateTime]::MinValue;$lastError=''
try {
    while (!( $stop.WaitOne(0) )) {
        if (!(Get-Process -Id $ServerPID -ErrorAction SilentlyContinue)) {break}
        $game=Get-Process bf4_no_drm -ErrorAction SilentlyContinue | Select-Object -First 1
        if (!$game) {$loadedPID=0}
        elseif ($game.Id -ne $loadedPID -and (Get-Date) -ge $nextAttempt) {
            try {
                & (Join-Path $PSScriptRoot 'Control-DeployClient.ps1') -Action Apply
                if (!$?) {throw 'Client companion controller failed.'}
                $loadedPID=$game.Id;$lastError=''
                Add-Content -LiteralPath (Join-Path $PSScriptRoot 'watcher.log') -Value ((Get-Date -Format s)+' Loaded client PID '+$loadedPID)
            } catch {
                $message=$_.Exception.Message
                if ($message -ne $lastError) {Add-Content -LiteralPath (Join-Path $PSScriptRoot 'watcher.log') -Value ((Get-Date -Format s)+' '+$message);$lastError=$message}
                $nextAttempt=(Get-Date).AddSeconds(10)
            }
        }
        [void]$stop.WaitOne(2000)
    }
} finally {$stop.Dispose();$mutex.ReleaseMutex();$mutex.Dispose()}
