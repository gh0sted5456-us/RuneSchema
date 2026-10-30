param([Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference = 'Stop'
function Read-Source([string]$Relative) { Get-Content -LiteralPath (Join-Path $Root $Relative) -Raw }
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
$build = Read-Source 'build/build.ps1'
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseInput($build, [ref]$tokens, [ref]$parseErrors)
Require ($parseErrors.Count -eq 0) 'Build script syntax is invalid'
function Assignment-Value([string]$Name) {
    $nodes = @($ast.FindAll({param($node) $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -ceq ('$'+$Name)}, $true))
    Require ($nodes.Count -eq 1) "Expected one literal assignment for $Name"
    $literals = @($nodes[0].Right.FindAll({param($node) $node -is [Management.Automation.Language.StringConstantExpressionAst]}, $true))
    Require ($literals.Count -eq 1) "Expected one string literal for $Name"
    $literals[0].Value
}
$version = Assignment-Value 'Version'
$pattern = Assignment-Value 'releaseVersionPattern'
Require ($version -eq '0.8.1.0') 'This release must be 0.8.1.0'
$numericVersion = [regex]::Match($version, '^\d+\.\d+\.\d+(?:\.\d+)?').Value
$resourceVersion = $numericVersion.Replace('.', ',')
foreach ($valid in @('0.8.0', '0.8.0.0', '0.8.0.1', '0.8.0.1b', '1.0.0', '0.7.7.9m', '0.7.7.9e', '0.7.8.0-alpha', '0.7.8.0-alpha.2')) {
    Require ($valid -cmatch $pattern) "Build rejects supported version $valid"
}
foreach ($invalid in @('0.8', '0.8.0garbage', '../0.8.0', '0.8.0.0.0', '0.7.7.9M', '0.8.0.1B', '0.8.0.1bb', '0.8.0b')) {
    Require ($invalid -cnotmatch $pattern) "Build accepts invalid version $invalid"
}
$packaging = Read-Source 'build/package-licenses.ps1'
Require ($packaging.Contains("'"+$pattern+"'")) 'Package finalization version pattern differs from build validation'
$workflow = Read-Source '.github/workflows/build-packages.yml'
$tagPattern = [regex]::Match($workflow, '\$tagMatch = \[regex\]::Match\(\$tag, ''([^'']+)''\)').Groups[1].Value
Require (-not [string]::IsNullOrEmpty($tagPattern)) 'Release tag validation is missing'
foreach ($tag in @($version, ('v'+$version), '0.7.8.0-alpha', 'v0.7.7.9m')) {
    Require ($tag -cmatch $tagPattern) "Publish workflow rejects supported tag $tag"
}
foreach ($tag in @('v0.8', 'v0.8.0-extra', '../0.8.0')) {
    Require ($tag -cnotmatch $tagPattern) "Publish workflow accepts invalid tag $tag"
}
Require ((Read-Source 'source/raw/include/Utility/BuildInfo.h').Contains('Version[] = "'+$version+'";')) 'Core runtime version differs'
Require ((Read-Source 'source/raw/CMakeLists.txt').Contains('project(RuneSchema VERSION '+$numericVersion+' LANGUAGES')) 'Core numeric CMake version differs'
Require ((Read-Source 'source/helpy/source/include/Core/HelpyBranding.h').Contains('Version[]="'+$version+'";')) 'RSMK display version differs'
Require ((Read-Source 'source/helpy/CMakeLists.txt').Contains('project(Helpy VERSION '+$numericVersion+' LANGUAGES')) 'RSMK numeric CMake version differs'
$resources = Read-Source 'source/helpy/source/resources/RSMK.rc'
Require ($resources.Contains('FILEVERSION '+$resourceVersion) -and $resources.Contains('PRODUCTVERSION '+$resourceVersion)) 'RSMK numeric Windows version differs'
Require ($resources.Contains('"FileVersion", "'+$version+'"') -and $resources.Contains('"ProductVersion", "'+$version+'"')) 'RSMK Windows display version differs'
$coreResources = Read-Source 'source/raw/resources/RuneSchema.rc'
Require ($coreResources.Contains('FILEVERSION '+$resourceVersion) -and $coreResources.Contains('PRODUCTVERSION '+$resourceVersion)) 'Core numeric Windows version differs'
Require ($coreResources.Contains('"FileVersion", "'+$version+'"') -and $coreResources.Contains('"ProductVersion", "'+$version+'"')) 'Core Windows display version differs'
foreach ($plugin in @('RuneSchema.Helpy', 'RSNetworking', 'RuneSchema.RegistryBridge', 'RuneSchema.QuestItems')) {
    $manifest = Read-Source "source/plugins/$plugin/plugin.json" | ConvertFrom-Json
    Require ($manifest.Version -eq $version -and $manifest.BuiltForRuneSchema -eq $version) "Plugin $plugin has mismatched release metadata"
}
Write-Output "RuneSchema and RSMK $version release metadata and legacy tag compatibility verified."
