"""Cook only the explicit owned JobBoard closure to an isolated output."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(r'F:\Unreal Projects\Dye')
WORK = ROOT / 'Build/JobBoard-20261015'
name = sys.argv[1] if len(sys.argv) > 1 else 'BoardCook-v1'
assert name.replace('-', '').isalnum()
out = WORK / name
assert not out.exists(), 'Existing cook protected'
contracts = len(sys.argv) > 2 and sys.argv[2] == 'contracts'
shared = len(sys.argv) > 2 and sys.argv[2] == 'shared'
packages = json.loads((WORK / ('SharedCookPackages.json' if shared else 'ContractCookPackages.json' if contracts else 'CookPackages.json')).read_text())
assert all(p.startswith(('/ColorsOfMoney/Core/JobBoard/', '/Game/Mods/JobBoard/',
                         '/RuneSchemaQuestItems/', '/Game/Mods/JobBoard_DyeQuests/', '/Game/Mods/Coinage/Art/Props/QuestBoard/Source/scene/Textures/')) for p in packages)
command = [r'D:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    str(ROOT / 'RSDragonwilds.uproject'), '-run=Cook', '-TargetPlatform=Windows',
    '-PACKAGE=' + '+'.join(packages), '-cooksinglepackagenorefs', '-CookSkipRequests',
    '-SkipHardReferences', '-SkipSoftReferences', '-OutputDir=' + str(out / '[Platform]'),
    '-ddc=InstalledNoZenLocalFallback', '-unattended', '-nop4', '-nosplash', '-nullrhi',
    '-nosound', '-stdout', '-FullStdOutLogOutput']
with (WORK / (name + '.stdout.log')).open('w') as stream:
    subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True)
actual = list(out.rglob('*.uasset'))
assert len(actual) == len(packages), (len(actual), len(packages))
print('JOBBOARD_EXPLICIT_COOK_COMPLETE', len(packages))
