param(
    [Parameter(Mandatory)][string]$Source,
    [Parameter(Mandatory)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$sourcePath = [IO.Path]::GetFullPath($Source)
$outputPath = [IO.Path]::GetFullPath($OutputRoot)
if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Missing ChromaticSkillcapes source: $sourcePath" }

$skills = [ordered]@{
    Artisan = @('GE_Skillcape_Artisan', 'Master Artisan', '10% chance to craft without consuming materials.')
    Attack = @('GE_Skillcape_MeleeAttack', 'Effortless Strikes', '15% chance for melee attacks to cost no stamina.')
    Construction = @('GE_Skillcape_Construction', 'Master Builder', 'Building repairs consume no materials.')
    Cooking = @('GE_Skillcape_Cooking', 'Perfect Cooking', 'Food cannot burn while this cape is equipped.')
    Farming = @('GE_Skillcape_Farming', 'Endless Plant Cure', 'Plant cure potion is not consumed while this cape is equipped.')
    Magic = @('GE_Skillcape_MagicAttack', 'Effortless Sorcery', '15% chance for magic attacks to cost no stamina.')
    Mining = @('GE_Skillcape_Mining', 'Master Miner', '10% native Mining skillcape proc chance.')
    Ranged = @('GE_Skillcape_RangeAttack', 'Effortless Shots', '15% chance for ranged attacks to cost no stamina.')
    Runecrafting = @('GE_Skillcape_Runecrafting', 'Anima Vent Mastery', 'Doubles runes produced from anima vents.')
    Woodcutting = @('GE_Skillcape_Woodcutting', 'Master Woodcutter', '10% native Woodcutting skillcape proc chance.')
}

$document = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json -AsHashtable
$effects = [ordered]@{}
foreach ($skill in $skills.Keys) {
    $className = $skills[$skill][0]
    $effects["Effects/Skillcape$skill"] = [ordered]@{
        Class = "/Game/Gameplay/GameplayEffects/Equipment/SkillCapes/$className.$($className)_C"
        '$Comment' = "Verified infinite native $skill skillcape effect."
    }
}

$patches = [ordered]@{}
foreach ($target in $document.Keys) {
    if ($target -notmatch 'ITEM_RS_Skillcape_([^_.]+)_([^_.]+)') { continue }
    $skill = $Matches[1]
    $color = $Matches[2]
    if (!$skills.Contains($skill)) { throw "Unsupported skill in $target" }
    $iconName = "T_Icon_Tag_Skill_$skill"
    $patches["$skill-$color"] = [ordered]@{
        '$Patch' = $target
        '$Target' = [ordered]@{
            '$Append' = [ordered]@{
                GrantedEffects = @("ZZ_ChromaticSkillcapeEffects:Effects/Skillcape$skill")
                BuffDatas = @([ordered]@{
                    Title = $skills[$skill][1]
                    Description = $skills[$skill][2]
                    BuffIcon = [ordered]@{
                        AssetPathName = "/Game/Art/UI/Skills/Icons/Tags/$iconName.$iconName"
                    }
                })
            }
        }
    }
}
if ($patches.Count -ne $document.Count) { throw "Mapped $($patches.Count) of $($document.Count) capes" }

$effectsFolder = Join-Path $outputPath 'effects'
$assetsFolder = Join-Path $outputPath 'assets'
New-Item -ItemType Directory -Path $effectsFolder,$assetsFolder -Force | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $effectsFolder '00-SkillcapeEffects.jsonc'), ($effects | ConvertTo-Json -Depth 8) + "`n", $utf8)
[IO.File]::WriteAllText((Join-Path $assetsFolder 'ZZ-SkillcapeEffectGrants.jsonc'), ($patches | ConvertTo-Json -Depth 12) + "`n", $utf8)
Write-Output "Mapped $($patches.Count) Chromatic Skillcapes to $($effects.Count) cooked effect aliases."
