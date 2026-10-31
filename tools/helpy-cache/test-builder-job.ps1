$ErrorActionPreference='Stop'
$tool=Join-Path $PSScriptRoot '../../resources/tools/build-helpy-cache.ps1'
. $tool
function Check([bool]$Ok,[string]$Reason){if(!$Ok){throw $Reason}}
$root=Join-Path ([IO.Path]::GetTempPath()) ('RS-builder-job-'+[Guid]::NewGuid().ToString('N'))
$null=Assert-CachePath $root ([IO.Path]::GetTempPath());New-Item -ItemType Directory -Path $root | Out-Null
try{
    $mod=Join-Path $root 'Mod with spaces';New-Item -ItemType Directory -Path (Join-Path $mod 'paks') | Out-Null
    foreach($extension in @('.pak','.utoc','.ucas')){[IO.File]::WriteAllText((Join-Path $mod ('fixture'+$extension)),'unchanged input')}
    $cache=Join-Path $mod 'cache';New-Item -ItemType Directory -Path $cache | Out-Null;[IO.File]::WriteAllText((Join-Path $cache 'old.txt'),'original cache')
    $script:ExpectedMod=$mod;$script:FailBuild=$false
    function dotnet {
        if($args[0] -eq '--list-sdks'){'10.0.401 [fixture]';return}
        if($args -contains '--progress'){
            Check ($args[([array]::IndexOf($args,'--mod-root')+1)] -eq $script:ExpectedMod) 'Installed mode copied or remapped the input folder'
            if($script:FailBuild){$global:LASTEXITCODE=9;return}
            $built=$args[([array]::IndexOf($args,'--output')+1)];New-Item -ItemType Directory -Path $built -Force | Out-Null
            [IO.File]::WriteAllText((Join-Path $built 'offline-helpy.rsmk-cache'),'new cache');[IO.File]::WriteAllText((Join-Path $built 'offline-helpy-report.json'),'{}')
            'RS_PROGRESS|Reading items|1|1'
        }$global:LASTEXITCODE=0
    }
    . $tool -ModDirectory $mod -InstallDotNet
    Invoke-CacheZip
    Check ([IO.File]::ReadAllText((Join-Path $cache 'offline-helpy.rsmk-cache')) -eq 'new cache') 'Installed cache was not published'
    $backup=Get-ChildItem -LiteralPath $mod -Directory -Filter 'cache.previous-*'
    Check ($backup.Count -eq 1 -and [IO.File]::ReadAllText((Join-Path $backup[0].FullName 'old.txt')) -eq 'original cache') 'Original cache was not retained'
    Check ([IO.File]::ReadAllText((Join-Path $mod 'fixture.pak')) -eq 'unchanged input') 'Source PAK changed'
    $script:FailBuild=$true;$failed=$false;try{Invoke-CacheZip}catch{$failed=$true}
    Check ($failed -and [IO.File]::ReadAllText((Join-Path $cache 'offline-helpy.rsmk-cache')) -eq 'new cache') 'Failure overwrote cache'
    $metadata=@{'latest-sdk'='10.0.403';releases=@(@{sdk=@{version='10.0.403';files=@(@{rid='win-x64';url='https://builds.dotnet.microsoft.com/dotnet/Sdk/10.0.403/sdk.zip';hash=('a'*128)})}})}
    Check ((Get-CacheSdkFile $metadata).Version -eq '10.0.403') 'Valid SDK metadata rejected'
    $metadata.releases[0].sdk.files[0].url='https://not-microsoft.example/sdk.zip';$failed=$false;try{Get-CacheSdkFile $metadata}catch{$failed=$true};Check $failed 'Untrusted SDK URL accepted'
    $control=Join-Path $root 'control.json';[IO.File]::WriteAllText($control,'{"Action":"stop"}')
    . $tool -ControlFile $control
    $cancelled=$false;try{Check-CacheControl}catch [OperationCanceledException]{$cancelled=$true};Check $cancelled 'Stop was not cooperative'
    $progress=Join-Path $root 'progress.json';[IO.File]::WriteAllText($control,'{"Action":"pause"}')
    $job=Start-Job -ScriptBlock {param($Tool,$Control,$Progress);. $Tool -ControlFile $Control -ProgressFile $Progress;Check-CacheControl;'resumed'} -ArgumentList $tool,$control,$progress
    try{
        $deadline=[DateTime]::UtcNow.AddSeconds(10);$paused=$false
        while([DateTime]::UtcNow -lt $deadline){try{if(([IO.File]::ReadAllText($progress)|ConvertFrom-Json).State -eq 'Paused'){$paused=$true;break}}catch{};Start-Sleep -Milliseconds 100}
        Check $paused 'Pause did not reach a checkpoint'
        $next=$control+'.next';[IO.File]::WriteAllText($next,'{"Action":"run"}');[IO.File]::Move($next,$control,$true)
        $null=Wait-Job $job -Timeout 10;Check ($job.State -eq 'Completed') 'Resume did not complete';Check ((Receive-Job $job) -contains 'resumed') 'Resume result missing'
    }finally{if($job.State -eq 'Running'){Stop-Job $job};Remove-Job $job}
    'PASS: installed-folder output, retained inputs/cache, existing SDK detection, SDK URL validation, safe stop, pause and resume.'
}finally{$null=Assert-CachePath $root ([IO.Path]::GetTempPath());Remove-Item -LiteralPath $root -Recurse -Force}
