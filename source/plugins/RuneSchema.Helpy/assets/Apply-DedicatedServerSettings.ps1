[CmdletBinding()]
param([Parameter(Mandatory)][string]$Changes)
$ErrorActionPreference='Stop'
function Assert-DragonwildsStopped {
    $active=@(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {$_.Name -match '(?i)Dragonwilds.*\.exe$'})
    if($active.Count){throw 'Stop Dragonwilds clients and dedicated servers before applying the staged INI changes. No configuration was changed.'}
}
Assert-DragonwildsStopped
$changePath=[IO.Path]::GetFullPath($Changes)
if(-not (Test-Path -LiteralPath $changePath -PathType Leaf) -or (Get-Item -LiteralPath $changePath).Length -gt 131072){throw 'Missing or oversized staged change file.'}
$utf8=[Text.UTF8Encoding]::new($false,$true)
$request=([IO.File]::ReadAllText($changePath,$utf8) | ConvertFrom-Json)
if($request.Version -ne 1 -or $request.Target -isnot [string] -or @($request.Changes).Count -lt 1 -or @($request.Changes).Count -gt 5){throw 'Unsupported staged change file.'}
$target=[IO.Path]::GetFullPath($request.Target)
if([IO.Path]::GetFileName($target) -ine 'DedicatedServer.ini' -or [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($target)) -ine 'WindowsServer'){throw 'Target must be an existing WindowsServer/DedicatedServer.ini.'}
if([IO.Path]::GetDirectoryName($changePath) -ine [IO.Path]::GetDirectoryName($target)){throw 'Staged changes must remain beside their target INI.'}
if(-not (Test-Path -LiteralPath $target -PathType Leaf)){throw 'Target INI does not exist; no new configuration will be guessed.'}
if((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse-point INI targets are not supported.'}
$section='/Script/Dominion.DedicatedServerSettings'
$allowed=@('OwnerId','ServerName','DefaultWorldName','AdminPassword','WorldPassword')
$patch=@{}
foreach($change in @($request.Changes)){
    if($change.Section -isnot [string] -or $change.Section -ine $section -or $change.Key -isnot [string] -or $change.Key -notin $allowed -or $patch.ContainsKey($change.Key)){throw 'Only unique documented scalar settings may be applied.'}
    foreach($value in @($change.Expected,$change.Value)){if($value -isnot [string] -or $value.Length -gt 8192 -or $value.IndexOfAny([char[]]@("`r","`n",[char]0)) -ge 0){throw 'Invalid staged scalar value.'}}
    $patch[$change.Key]=$change
}
$original=$null;$temporary=$null
if(-not ('RuneSchemaOfflineConfig.Native' -as [type])){
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace RuneSchemaOfflineConfig {
    public static class Native {
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ReplaceFile(string replaced, string replacement, string backup, uint flags, IntPtr exclude, IntPtr reserved);
    }
}
'@
}
try{
    # Exclude writers while comparing/applying. Rename sharing is needed for
    # Windows atomic replacement; recheck that the server stayed stopped.
    $original=[IO.File]::Open($target,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::Read -bor [IO.FileShare]::Delete))
    if($original.Length -gt 262144){throw 'INI exceeds the 256 KiB editor limit.'}
    $bytes=New-Object byte[] $original.Length
    $offset=0;while($offset -lt $bytes.Length){$read=$original.Read($bytes,$offset,$bytes.Length-$offset);if($read -le 0){throw 'Incomplete INI read.'};$offset+=$read}
    $text=$utf8.GetString($bytes)
    if($text.IndexOf([char]0) -ge 0){throw 'UTF-16/binary INI is unsupported. Original unchanged.'}
    $currentSection='';$seen=@{};$output=[Text.StringBuilder]::new()
    foreach($match in [regex]::Matches($text,'[^\r\n]*(?:\r\n|\n|\r|$)')){
        if(-not $match.Length){continue}
        $line=$match.Value;$body=$line.TrimEnd([char[]]@("`r","`n"));$ending=$line.Substring($body.Length)
        $trimmed=$body.Trim().TrimStart([char]0xFEFF)
        if($trimmed.StartsWith('[') -and $trimmed.EndsWith(']')){$currentSection=$trimmed.Substring(1,$trimmed.Length-2)}
        elseif($currentSection -ieq $section -and $trimmed -and -not $trimmed.StartsWith(';') -and -not $trimmed.StartsWith('#')){
            $equals=$body.IndexOf('=')
            if($equals -ge 0){$key=$body.Substring(0,$equals).Trim();if($patch.ContainsKey($key)){
                if($seen.ContainsKey($key)){throw 'An edited scalar has duplicate declarations. Resolve ambiguity before applying.'}
                $seen[$key]=$true;$change=$patch[$key]
                if($body.Substring($equals+1) -cne $change.Expected){throw 'An edited setting changed since staging. Reload and review it; no changes applied.'}
                $line=$body.Substring(0,$equals+1)+$change.Value+$ending
            }}
        }
        [void]$output.Append($line)
    }
    if($seen.Count -ne $patch.Count){throw 'An edited key is missing. Reload and review the active profile before applying.'}
    $updated=$utf8.GetBytes($output.ToString());if($updated.Length -gt 262144){throw 'Edited INI exceeds 256 KiB.'}
    $suffix=[Guid]::NewGuid().ToString('N');$temporary=$target+'.helpy-'+$suffix+'.tmp';$backup=$target+'.helpy-'+$suffix+'.bak'
    $staged=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$staged.Write($updated,0,$updated.Length);$staged.Flush($true)}finally{$staged.Dispose()}
    Assert-DragonwildsStopped
    if(-not [RuneSchemaOfflineConfig.Native]::ReplaceFile($target,$temporary,$backup,0,[IntPtr]::Zero,[IntPtr]::Zero)){
        throw ('Atomic INI replacement failed (Windows error '+[Runtime.InteropServices.Marshal]::GetLastWin32Error()+'). Check the active INI and adjacent backup before retrying.')
    }
    $temporary=$null
    Write-Output ('Applied '+$patch.Count+' reviewed scalar setting(s). Runtime player/admin records preserved. Backup: '+$backup)
    Write-Output 'Start the server to apply its new configuration. Keep the staged file and backup private; both may contain passwords.'
}finally{
    if($original){$original.Dispose()}
    if($temporary -and (Test-Path -LiteralPath $temporary -PathType Leaf)){Remove-Item -LiteralPath $temporary -Force}
}
