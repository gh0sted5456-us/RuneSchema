# Private, checksum-verified SDK setup for explicit authoring jobs only.
function Get-CacheSdkFile($Metadata) {
    $version=$Metadata.'latest-sdk'
    if($version -notmatch '^10\.0\.\d+$'){throw 'Unsupported .NET SDK release metadata.'}
    $sdk=@($Metadata.releases | ForEach-Object {@($_.sdk)+@($_.sdks)} | Where-Object {$_.version -eq $version}) | Select-Object -First 1
    $file=@($sdk.files | Where-Object {$_.rid -eq 'win-x64' -and $_.url.EndsWith('.zip')}) | Select-Object -First 1
    $uri=if($file){[Uri]$file.url}else{$null}
    if(!$uri -or $uri.Scheme -ne 'https' -or $uri.Host -notin @('builds.dotnet.microsoft.com','download.visualstudio.microsoft.com') -or $uri.UserInfo -or $file.hash -notmatch '^[a-fA-F0-9]{128}$'){throw 'Invalid Microsoft SDK download metadata.'}
    return @{Version=$version;Url=$uri.AbsoluteUri;Hash=$file.hash}
}
function Resolve-CacheDotnet([string]$Stage) {
    $command=Get-Command dotnet -ErrorAction SilentlyContinue
    if($command){$candidate=if($command.CommandType -eq 'Function'){$command.Name}else{$command.Source};if(& $candidate --list-sdks | Where-Object {$_ -match '^10\.'}){return $candidate}}
    $sdkHome=Join-Path $env:LOCALAPPDATA 'RSDragonwilds/RuneSchema/core/tools/dotnet'
    $null=Assert-CachePath $sdkHome ([IO.Path]::GetDirectoryName($sdkHome))
    foreach($folder in @(Get-ChildItem -LiteralPath $sdkHome -Directory -ErrorAction SilentlyContinue | Where-Object {$_.Name -match '^10\.0\.\d+$'} | Sort-Object Name -Descending)){
        $candidate=Join-Path $folder.FullName 'dotnet.exe';$null=Assert-CachePath $candidate $sdkHome
        if((Test-Path -LiteralPath $candidate) -and (& $candidate --list-sdks | Where-Object {$_ -match '^10\.'})){return $candidate}
    }
    if(!$InstallDotNet){throw 'The .NET 10 SDK is needed. Enable automatic .NET setup under Tools > Authoring tools > Dependencies, or install the SDK yourself.'}
    Show-CacheProgress 'Reading Microsoft .NET release metadata' 0
    [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $metadata=Invoke-RestMethod -Uri 'https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json' -TimeoutSec 30
    $sdk=Get-CacheSdkFile $metadata;$zip=Join-Path $Stage 'dotnet-sdk.zip'
    $request=[Net.HttpWebRequest]::Create($sdk.Url);$request.AllowAutoRedirect=$false;$request.Timeout=30000;$request.ReadWriteTimeout=30000
    $response=$request.GetResponse()
    try{
        if($response.ContentLength -gt 1GB){throw 'SDK download exceeds 1 GiB.'}
        $downloadInput=$response.GetResponseStream();$downloadOutput=[IO.File]::Create($zip)
        try{[long]$done=0;$buffer=New-Object byte[] 1048576
            while(($count=$downloadInput.Read($buffer,0,$buffer.Length)) -gt 0){
                $done+=$count;if($done -gt 1GB){throw 'SDK download exceeds 1 GiB.'}
                $downloadOutput.Write($buffer,0,$count);Show-CacheProgress ('Downloading authoring .NET SDK: '+[int]($done/1MB)+' MiB') ([int](15*$done/[Math]::Max([double]$done,[double]$response.ContentLength)))
            }
        }finally{$downloadOutput.Dispose();$downloadInput.Dispose()}
    }finally{$response.Dispose()}
    Show-CacheProgress 'Verifying the Microsoft SDK checksum' 15
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA512).Hash -ine $sdk.Hash){throw 'Microsoft SDK checksum failed. Download was not installed.'}
    $destination=Join-Path $sdkHome $sdk.Version;$stage=Join-Path $sdkHome ('.i-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
    $null=Assert-CachePath $destination $sdkHome;$null=Assert-CachePath $stage $sdkHome
    try{
        Show-CacheProgress 'Installing the verified authoring SDK' 15
        New-Item -ItemType Directory -Path $sdkHome -Force | Out-Null;New-Item -ItemType Directory -Path $stage | Out-Null
        $archive=[IO.Compression.ZipFile]::OpenRead($zip)
        try{[long]$total=0;$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            if($archive.Entries.Count -gt 100000){throw 'SDK archive has too many entries.'}
            $entryCount=0;foreach($entry in $archive.Entries){
                Check-CacheControl;$total+=$entry.Length
                if((++$entryCount % 100) -eq 0){Show-CacheProgress 'Installing the verified authoring SDK' ([int](15+8.0*$entryCount/$archive.Entries.Count))}
                if($entry.Length -lt 0 -or $total -gt 4GB -or !$seen.Add($entry.FullName) -or (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960){throw 'Invalid SDK archive entry.'}
                $path=Assert-CachePath (Join-Path $stage $entry.FullName) $stage
                if($entry.FullName.EndsWith('/')){New-Item -ItemType Directory -Path $path -Force | Out-Null;continue}
                New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($path)) -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path)
            }
        }finally{$archive.Dispose()}
        $candidate=Join-Path $stage 'dotnet.exe'
        if(!(Test-Path -LiteralPath $candidate) -or !(& $candidate --list-sdks | Where-Object {$_ -match '^10\.'})){throw 'The installed authoring SDK did not pass its check.'}
        Check-CacheControl
        if(Test-Path -LiteralPath $destination){$existing=Join-Path $destination 'dotnet.exe';if((Test-Path -LiteralPath $existing) -and (& $existing --list-sdks | Where-Object {$_ -match '^10\.'})){return $existing};throw 'An SDK installation already occupies this version folder; it was retained.'}
        Move-Item -LiteralPath $stage -Destination $destination
        return (Join-Path $destination 'dotnet.exe')
    }finally{if(Test-Path -LiteralPath $stage){$null=Assert-CachePath $stage $sdkHome;Remove-Item -LiteralPath $stage -Recurse -Force}}
}
