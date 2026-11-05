param([ValidateSet('Apply','Disable','Status')][string]$Action='Apply')
$ErrorActionPreference='Stop'
$addonRoot=$PSScriptRoot
$gameRoot=Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $addonRoot))
$serverFile=Join-Path $gameRoot 'bf4_no_drm.exe'
$addonFile=Join-Path $addonRoot 'BF4DeployAll.dll'
$expectedClientHash='0F3B4CA5E4B9642413B8DF5D869D94337A8BB690238AE2B2E5A780A68942AEB4'
if (!(Test-Path -LiteralPath $serverFile)) {throw 'Client executable not found beside the Server folder.'}
if ((Get-FileHash -LiteralPath $serverFile -Algorithm SHA256).Hash -ne $expectedClientHash) {throw 'Unsupported BF4 client executable.'}
$candidates=@(Get-Process bf4_no_drm -ErrorAction SilentlyContinue | Where-Object {!$_.Path -or $_.Path -eq $serverFile})
if (!$candidates.Count) {
    if ($Action -ne 'Apply') {Write-Output 'BF4 client is not running.';return}
    throw 'BF4 client is not running.'
}
if ($candidates.Count -ne 1) {throw 'Multiple BF4 client processes are running.'}
$serverProcess=$candidates[0]
if ([IntPtr]::Size -ne 8) { throw 'Use 64-bit PowerShell.' }

if (!('CodexBF4DeployClient.Native' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace CodexBF4DeployClient {
public static class Native {
 [DllImport("kernel32",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
 [DllImport("kernel32",SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32",SetLastError=true)] public static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint allocation,uint protection);
 [DllImport("kernel32",SetLastError=true)] public static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint type);
 [DllImport("kernel32",SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr process,IntPtr address,byte[] bytes,UIntPtr count,out UIntPtr written);
 [DllImport("kernel32",SetLastError=true)] public static extern IntPtr CreateRemoteThread(IntPtr process,IntPtr attrs,UIntPtr stack,IntPtr start,IntPtr argument,uint flags,out uint tid);
 [DllImport("kernel32",SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr handle,uint timeout);
 [DllImport("kernel32",SetLastError=true)] public static extern bool GetExitCodeThread(IntPtr thread,out uint result);
 [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr GetModuleHandle(string name);
 [DllImport("kernel32",CharSet=CharSet.Ansi,SetLastError=true)] public static extern IntPtr GetProcAddress(IntPtr module,string name);
 [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
 [DllImport("kernel32",SetLastError=true)] public static extern bool FreeLibrary(IntPtr module);
}
}
'@
}
$processHandle=[CodexBF4DeployClient.Native]::OpenProcess(0x43A,$false,[uint32]$serverProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) { throw 'Cannot access the elevated BF4 client; run the combined launcher as administrator.' }
function Get-RemoteModules { $serverProcess.Refresh(); @($serverProcess.Modules) }
function Get-AddonModule {
    Get-RemoteModules | Where-Object { $_.ModuleName -ieq 'BF4DeployAll.dll' } | Select-Object -First 1
}
function Invoke-RemoteCall([IntPtr]$Entry,[IntPtr]$Argument) {
    [uint32]$threadId=0
    $thread=[CodexBF4DeployClient.Native]::CreateRemoteThread($processHandle,[IntPtr]::Zero,[UIntPtr]::Zero,$Entry,$Argument,0,[ref]$threadId)
    if ($thread -eq [IntPtr]::Zero) { throw 'Cannot create addon control call.' }
    try {
        if ([CodexBF4DeployClient.Native]::WaitForSingleObject($thread,15000) -ne 0) { throw 'Addon control call timed out; do not unload the addon until the server stops.' }
        [uint32]$result=0
        if (![CodexBF4DeployClient.Native]::GetExitCodeThread($thread,[ref]$result)) { throw 'Cannot read addon control result.' }
        return $result
    } finally { [void][CodexBF4DeployClient.Native]::CloseHandle($thread) }
}
function Get-RemoteSystemFunction([string]$Name) {
    $localKernel=[CodexBF4DeployClient.Native]::GetModuleHandle('kernel32.dll')
    $localFunction=[CodexBF4DeployClient.Native]::GetProcAddress($localKernel,$Name)
    if ($localFunction -eq [IntPtr]::Zero) { throw "System function unavailable: $Name" }
    $address=$localFunction.ToInt64()
    $localOwner=[Diagnostics.Process]::GetCurrentProcess().Modules | Where-Object {
        $address -ge $_.BaseAddress.ToInt64() -and $address -lt ($_.BaseAddress.ToInt64()+$_.ModuleMemorySize)
    } | Select-Object -First 1
    if (!$localOwner) { throw 'Cannot resolve system function owner.' }
    $remoteOwner=Get-RemoteModules | Where-Object { $_.ModuleName -ieq $localOwner.ModuleName } | Select-Object -First 1
    if (!$remoteOwner) { throw 'Remote system module unavailable.' }
    return [IntPtr]::new($remoteOwner.BaseAddress.ToInt64()+$address-$localOwner.BaseAddress.ToInt64())
}
function Get-RemoteExport($Module,[string]$Name) {
    # Map solely to resolve the RVA; do not run the addon in PowerShell.
    $local=[CodexBF4DeployClient.Native]::LoadLibraryEx($Module.FileName,[IntPtr]::Zero,1)
    if ($local -eq [IntPtr]::Zero) { throw 'Cannot map addon export table.' }
    try {
        $function=[CodexBF4DeployClient.Native]::GetProcAddress($local,$Name)
        if ($function -eq [IntPtr]::Zero) { throw "Addon export unavailable: $Name" }
        return [IntPtr]::new($Module.BaseAddress.ToInt64()+$function.ToInt64()-$local.ToInt64())
    } finally { [void][CodexBF4DeployClient.Native]::FreeLibrary($local) }
}
try {
    $module=Get-AddonModule
    if ($module -and $module.FileName -ine $addonFile) { throw ('Another addon copy is already loaded: '+$module.FileName) }
    if ($Action -eq 'Disable') {
        if (!$module) { Write-Output 'Spawn UI companion is not loaded.'; return }
        $result=Invoke-RemoteCall (Get-RemoteExport $module 'SeatAddonUninstall') ([IntPtr]::Zero)
        if ($result -eq 1) {
            Write-Output 'Spawn UI disabled; safe DLL unload is deferred until the game exits.'; return
        }
        if ($result -ne 0) { throw "Addon refused unload: $result" }
        [void](Invoke-RemoteCall (Get-RemoteSystemFunction 'FreeLibrary') $module.BaseAddress)
        Write-Output 'Spawn UI companion disabled; original game files preserved.'
        return
    }
    if ($Action -eq 'Status') {
        if (!$module) { Write-Output 'Spawn UI companion is not loaded.'; return }
        $state=Invoke-RemoteCall (Get-RemoteExport $module 'SeatAddonCounter') ([IntPtr]::Zero)
        Write-Output ('Spawn UI companion enabled: '+($state -eq 1))
        return
    }
    if (!(Test-Path -LiteralPath $addonFile)) { throw 'Addon DLL is missing.' }
    $manifestPath=Join-Path $addonRoot 'manifest.json'
    if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Addon manifest is missing.' }
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $addonFile -Algorithm SHA256).Hash -ine $manifest.DllSHA256) { throw 'Addon hash differs from manifest.' }
    if (!$module) {
        $pathBytes=[Text.Encoding]::Unicode.GetBytes($addonFile+[char]0)
        $memory=[CodexBF4DeployClient.Native]::VirtualAllocEx($processHandle,[IntPtr]::Zero,[UIntPtr]::new([uint64]$pathBytes.Length),0x3000,4)
        if ($memory -eq [IntPtr]::Zero) { throw 'Cannot allocate addon path buffer.' }
        $completed=$false
        try {
            [UIntPtr]$written=[UIntPtr]::Zero
            if (![CodexBF4DeployClient.Native]::WriteProcessMemory($processHandle,$memory,$pathBytes,[UIntPtr]::new([uint64]$pathBytes.Length),[ref]$written) -or $written.ToUInt64() -ne $pathBytes.Length) { throw 'Cannot write addon path buffer.' }
            [void](Invoke-RemoteCall (Get-RemoteSystemFunction 'LoadLibraryW') $memory)
            $completed=$true
        } finally { if ($completed) { [void][CodexBF4DeployClient.Native]::VirtualFreeEx($processHandle,$memory,[UIntPtr]::Zero,0x8000) } }
        $module=Get-AddonModule
        if (!$module -or $module.FileName -ine $addonFile) { throw 'Windows did not load the addon DLL.' }
    }
    $install=Get-RemoteExport $module 'SeatAddonInstall'
    $deadline=(Get-Date).AddSeconds(30)
    do {
        $result=Invoke-RemoteCall $install ([IntPtr]::Zero)
        if ($result -ne 2) { break }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)
    if ($result -ne 0) { throw "Addon installation rejected with code $result. Original DLL was not replaced." }
    Write-Output 'Spawn UI companion enabled for this server process.'
} finally { [void][CodexBF4DeployClient.Native]::CloseHandle($processHandle) }
