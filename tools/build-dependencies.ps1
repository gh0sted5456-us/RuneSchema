param([string]$BuildDirectory = "$PSScriptRoot/../../server-equipment-build/dependencies")
$ErrorActionPreference = 'Stop'
$root=[IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Force $root | Out-Null
$references=[ordered]@{
    Zycore='C:/rs062-feature/_deps/zydis-build/zycore/Zycore.vcxproj'
    Zydis='C:/rs062-feature/_deps/zydis-build/Zydis.vcxproj'
    safetyhook='C:/rs062-feature/_deps/safetyhook-build/safetyhook.vcxproj'
    efsw='C:/rs062-feature/_deps/efsw-build/efsw.vcxproj'
    fmt='C:/rs062-feature/_deps/fmt-build/fmt.vcxproj'
    ImGui='C:/rs062-feature/_deps/ue4ss-build/deps/third/imgui/ImGui.vcxproj'
}
$manifest=@()
foreach($name in $references.Keys){
    $reference=$references[$name]
    $output=Join-Path $root $name
    New-Item -ItemType Directory -Force $output | Out-Null
    [xml]$project=Get-Content -LiteralPath $reference -Raw
    $ns=[Xml.XmlNamespaceManager]::new($project.NameTable)
    $ns.AddNamespace('m',$project.DocumentElement.NamespaceURI)
    foreach($node in @($project.SelectNodes('//m:CustomBuild|//m:ProjectReference',$ns))){$node.ParentNode.RemoveChild($node) | Out-Null}
    foreach($group in $project.SelectNodes('//m:ItemDefinitionGroup',$ns)){
        $compile=$group.SelectSingleNode('m:ClCompile',$ns)
        if(!$compile){continue}
        foreach($pair in @(@('RuntimeLibrary','MultiThreadedDLL'),@('Optimization','MinSpace'),@('FavorSizeOrSpeed','Size'))){
            $node=$compile.SelectSingleNode('m:'+$pair[0],$ns)
            if(!$node){$node=$project.CreateElement($pair[0],$project.DocumentElement.NamespaceURI);$compile.AppendChild($node)|Out-Null}
            $node.InnerText=$pair[1]
        }
    }
    $path=Join-Path $output "$name.vcxproj"
    $project.Save($path)
    $sources=@($reference)+@($project.SelectNodes('//m:ClCompile[@Include]|//m:ClInclude[@Include]',$ns) | ForEach-Object {
        $candidate=$_.Include
        if([IO.Path]::IsPathRooted($candidate)){$candidate}else{[IO.Path]::GetFullPath((Join-Path (Split-Path $reference) $candidate))}
    })
    $inputs=@($sources | Sort-Object -Unique | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {@{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash}})
    $inputs | ConvertTo-Json | Set-Content "$output/inputs.json"
    & 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' $path /t:Rebuild /p:Configuration=Game__Shipping__Win64 /p:Platform=x64 /p:BuildProjectReferences=false "/p:OutDir=$output/" "/p:IntDir=$output/obj/" /m:2 /v:minimal /nologo /nodeReuse:false /fl "/flp:logfile=$output/build.log;verbosity=normal"
    if($LASTEXITCODE -ne 0){throw "Dependency rebuild failed: $name"}
    $library=Join-Path $output "$name.lib"
    if(!(Test-Path -LiteralPath $library)){throw "Missing rebuilt library: $library"}
    $manifest+=@{name=$name;path=$library;sha256=(Get-FileHash -LiteralPath $library).Hash;reference=$reference}
}
$manifest | ConvertTo-Json | Set-Content "$root/libraries.json"
