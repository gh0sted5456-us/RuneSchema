param(
    [string]$ExportRoot = 'C:\Users\user\Desktop\Main Game',
    [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $OutputPath) { throw 'Choose a new output file; existing catalogs are preserved.' }
$root = (Resolve-Path -LiteralPath $ExportRoot).Path
$files = @(rg -l '"Type": "SkillData"' $root -g '*.json')
if ($LASTEXITCODE -gt 1) { throw 'Skill search failed.' }
$skills = @(foreach ($file in $files) {
    foreach ($asset in (Get-Content -LiteralPath $file -Raw | ConvertFrom-Json)) {
        if ($asset.Type -ne 'SkillData') { continue }
        [ordered]@{
            name = $asset.Name
            skillType = $asset.Properties.SkillType
            asset = $asset.Package + '.' + $asset.Name
            icon = $asset.Properties.Icon.AssetPathName
            source = $file.Substring($root.Length + 1)
            inDeprecatedFolder = $file -match '\\DeprecatedSkills\\'
            runtimeAvailability = 'Not established by export location'
        }
    }
})
[ordered]@{kind='SkillAssetCatalog';skills=$skills} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Output "Exported $($skills.Count) skill definitions. Folder names do not establish runtime availability."
