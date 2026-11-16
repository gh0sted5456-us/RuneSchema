function Set-RuneSchemaRuntimeLayout([string]$PayloadRoot) {
    $root=[IO.Path]::GetFullPath($PayloadRoot).TrimEnd('\','/')
    if(-not(Test-Path -LiteralPath $root -PathType Container)){throw 'Runtime staging root is missing'}
    function Assert-LayoutPath([string]$path){
        $absolute=[IO.Path]::GetFullPath($path)
        if(-not $absolute.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Runtime layout path escaped staging'}
        for($part=$absolute;$part -and $part.Length -ge $root.Length;$part=[IO.Path]::GetDirectoryName($part)){
            if((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Runtime layout contains a redirected path'}
        }
    }
    $legacy=Join-Path $root 'plugins'
    if(-not(Test-Path -LiteralPath $legacy)){return}
    Assert-LayoutPath $legacy
    $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($legacy)
    while($pending.Count){foreach($item in Get-ChildItem -LiteralPath $pending.Pop() -Force){Assert-LayoutPath $item.FullName;if($item.PSIsContainer){$pending.Push($item.FullName)}}}
    $helpy=Join-Path $root 'Helpy';$core=Join-Path $root 'dlls/plugins'
    foreach($path in @($helpy,$core)){Assert-LayoutPath $path}
    foreach($plugin in Get-ChildItem -LiteralPath $legacy -Directory | Sort-Object @{Expression={if($_.Name -eq 'RuneSchema.Helpy'){0}else{1}}},Name){
        $destination=if($plugin.Name -eq 'RuneSchema.Helpy'){$helpy}elseif($plugin.Name -eq 'RuneSchema.HUD' -or ((Test-Path -LiteralPath (Join-Path $plugin.FullName 'plugin.json')) -and (Get-Content -LiteralPath (Join-Path $plugin.FullName 'plugin.json') -Raw|ConvertFrom-Json).Host -eq 'helpy')){Join-Path $helpy ('plugins/'+$plugin.Name)}else{Join-Path $core $plugin.Name}
        Assert-LayoutPath $destination
        if(Test-Path -LiteralPath $destination){throw "Duplicate runtime plugin destination: $destination"}
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Move-Item -LiteralPath $plugin.FullName -Destination $destination
    }
    $order=Join-Path $legacy 'plugins.txt'
    if(Test-Path -LiteralPath $order){$destination=Join-Path $root 'settings/plugins.txt';Assert-LayoutPath $destination;if(Test-Path -LiteralPath $destination){throw 'Duplicate plugin order files in staging'};New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force|Out-Null;Move-Item -LiteralPath $order -Destination $destination}
    if(@(Get-ChildItem -LiteralPath $legacy -Force).Count){throw 'Unexpected files remain in legacy plugin staging'}
    Remove-Item -LiteralPath $legacy
    $dll=Join-Path $helpy 'dll'
    if(Test-Path -LiteralPath $dll){foreach($file in Get-ChildItem -LiteralPath $dll -Force){$destination=Join-Path $helpy $file.Name;Assert-LayoutPath $destination;if(Test-Path -LiteralPath $destination){throw 'Duplicate Helpy binary in staging'};Move-Item -LiteralPath $file.FullName -Destination $destination};Remove-Item -LiteralPath $dll}
    $bridge=Join-Path $helpy 'RuneSchema.Helpy.dll'
    if(Test-Path -LiteralPath $bridge){$destination=Join-Path $helpy 'Helpy.dll';Assert-LayoutPath $destination;if(Test-Path -LiteralPath $destination){throw 'Duplicate Helpy bridge'};Move-Item -LiteralPath $bridge -Destination $destination}
    $manifest=Join-Path $helpy 'plugin.json'
    if(Test-Path -LiteralPath $manifest){$data=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json;if($data.Id -ne 'RuneSchema.Helpy'){throw 'Helpy manifest identity mismatch'};$data.EntryPoint='Helpy.dll';$data|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $manifest -Encoding utf8}
}
