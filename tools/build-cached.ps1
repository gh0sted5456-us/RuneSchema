param(
    [string]$ReferenceProject = "$PSScriptRoot/../../selective-roaming-skeletal-build/RuneSchema.vcxproj",
    [string]$BuildDirectory = "$PSScriptRoot/../../server-equipment-build"
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$reference = (Resolve-Path $ReferenceProject).Path
$referenceRoot = Split-Path $reference
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
if ($buildRoot -eq $referenceRoot -or $buildRoot -eq $sourceRoot) { throw 'Build directory must be separate from source and dependency reference.' }
New-Item -ItemType Directory -Force $buildRoot | Out-Null
$pythonCommand = if ($env:RUNESCHEMA_PYTHON) {
    $env:RUNESCHEMA_PYTHON
} else {
    $discoveredPython = Get-Command python -ErrorAction SilentlyContinue
    if (!$discoveredPython) { throw 'Set RUNESCHEMA_PYTHON to a Python 3 executable.' }
    $discoveredPython.Source
}
& $pythonCommand "$PSScriptRoot/check-dependencies.py"
if ($LASTEXITCODE) { throw 'Dependency source/header verification failed' }
& "$PSScriptRoot/build-dependencies.ps1" -BuildDirectory "$buildRoot/dependencies"
& cmake -S "$sourceRoot/core" -B "$buildRoot/core" -G 'Visual Studio 18 2026' -A x64 '-DRUNESCHEMA_JSON_INCLUDE_DIR=C:/rs062-feature/_deps/nlohmann_json-src/include'
if ($LASTEXITCODE) { throw 'JSON core configure failed' }
& cmake --build "$buildRoot/core" --config Release --clean-first --parallel 2
if ($LASTEXITCODE) { throw 'JSON core build failed' }
& ctest --test-dir "$buildRoot/core" -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'JSON core tests failed' }
[xml]$project = Get-Content -Raw -LiteralPath $reference
$ns = [Xml.XmlNamespaceManager]::new($project.NameTable)
$ns.AddNamespace('m', $project.DocumentElement.NamespaceURI)
if (!$referenceRoot.EndsWith('-build', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Reference project directory must have a paired -build source directory.'
}
$oldSource = $referenceRoot.Substring(0, $referenceRoot.Length - '-build'.Length)
if (!(Test-Path -LiteralPath $oldSource)) { throw "Missing reference source directory: $oldSource" }
foreach ($node in @($project.SelectNodes('//m:CustomBuild|//m:ProjectReference|//m:ClCompile[@Include]|//m:ClInclude[@Include]', $ns))) {
    $node.ParentNode.RemoveChild($node) | Out-Null
}
foreach ($node in $project.SelectNodes('//*[not(*)]', $ns)) {
    $node.InnerText = $node.InnerText.Replace($referenceRoot, $buildRoot).Replace($referenceRoot.Replace('\','/'), $buildRoot.Replace('\','/')).Replace($oldSource, $sourceRoot).Replace($oldSource.Replace('\','/'), $sourceRoot.Replace('\','/'))
}
foreach ($group in $project.SelectNodes('//m:ItemDefinitionGroup', $ns)) {
    $compile = $group.SelectSingleNode('m:ClCompile', $ns)
    foreach ($pair in @(@('Optimization','MinSpace'), @('FavorSizeOrSpeed','Size'), @('IntrinsicFunctions','true'))) {
        $node = $compile.SelectSingleNode('m:' + $pair[0], $ns)
        if (!$node) { $node = $project.CreateElement($pair[0], $project.DocumentElement.NamespaceURI); $compile.AppendChild($node) | Out-Null }
        $node.InnerText = $pair[1]
    }
    $compileOptions = $compile.SelectSingleNode('m:AdditionalOptions', $ns)
    if (!$compileOptions) {
        $compileOptions = $project.CreateElement('AdditionalOptions', $project.DocumentElement.NamespaceURI)
        $compile.AppendChild($compileOptions) | Out-Null
    }
    $compileOptions.InnerText += " /experimental:deterministic /pathmap:`"$sourceRoot`"=RuneSchema /pathmap:`"$buildRoot`"=Build"
    $deps = $group.SelectSingleNode('m:Link/m:AdditionalDependencies', $ns)
    if ($deps) {
        $deps.InnerText = (@((Join-Path $referenceRoot 'Game__Shipping__Win64/lib/UE4SS.lib')) +
            @('Zycore','Zydis','safetyhook','efsw','fmt','ImGui' | ForEach-Object { Join-Path $buildRoot "dependencies/$_/$_.lib" }) +
            @((Join-Path $buildRoot 'core/Release/RuneSchemaJsonCore.lib'),'kernel32.lib')) -join ';'
        foreach ($dependency in ($deps.InnerText -split ';')) {
            if ($group.Condition -match 'Game__Shipping__Win64\|x64' -and [IO.Path]::IsPathRooted($dependency) -and !(Test-Path -LiteralPath $dependency)) { throw "Missing dependency: $dependency" }
        }
    }
    $link = $group.SelectSingleNode('m:Link', $ns)
    foreach ($pair in @(@('GenerateMapFile','true'), @('MapFileName', (Join-Path $buildRoot 'RuneSchema.map')), @('LinkTimeCodeGeneration','Default'), @('AdditionalOptions','%(AdditionalOptions) /PDBALTPATH:RuneSchema.pdb'))) {
        $node = $link.SelectSingleNode('m:' + $pair[0], $ns)
        if (!$node) { $node = $project.CreateElement($pair[0], $project.DocumentElement.NamespaceURI); $link.AppendChild($node) | Out-Null }
        $node.InnerText = $pair[1]
    }
}
$items = $project.CreateElement('ItemGroup', $project.DocumentElement.NamespaceURI)
foreach ($file in Get-ChildItem "$sourceRoot/src" -Filter *.cpp -Recurse) {
    if ($file.DirectoryName -eq (Join-Path $sourceRoot 'src/Core')) { continue }
    $node = $project.CreateElement('ClCompile', $project.DocumentElement.NamespaceURI)
    $node.SetAttribute('Include', $file.FullName)
    $items.AppendChild($node) | Out-Null
}
$project.DocumentElement.AppendChild($items) | Out-Null
$project.Save((Join-Path $buildRoot 'RuneSchema.vcxproj'))
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
$start.UseShellExecute = $false
$start.Environment.Clear()
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $start.Environment[$entry.Key] = $entry.Value }
$start.Arguments = '"' + (Join-Path $buildRoot 'RuneSchema.vcxproj') + '" /t:Rebuild /p:Configuration=Game__Shipping__Win64 /p:Platform=x64 /p:BuildProjectReferences=false /m:2 /v:minimal /nologo /nodeReuse:false /fl /flp:"logfile=' + (Join-Path $buildRoot 'build.log') + ';verbosity=normal"'
$inputs = @($reference) + @(Get-ChildItem "$sourceRoot/src", "$sourceRoot/include" -Recurse -File | ForEach-Object FullName)
$shipping = $project.SelectNodes('//m:ItemDefinitionGroup', $ns) | Where-Object Condition -Match 'Game__Shipping__Win64\|x64'
foreach ($group in $shipping) { $inputs += @($group.SelectSingleNode('m:Link/m:AdditionalDependencies', $ns).InnerText -split ';' | Where-Object { [IO.Path]::IsPathRooted($_) }) }
@($inputs | Sort-Object -Unique | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash} }) | ConvertTo-Json | Set-Content (Join-Path $buildRoot 'input-manifest.json')
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
if ($process.ExitCode -ne 0) { throw "Build failed: $($process.ExitCode)" }
