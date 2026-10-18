"""Package the exact editor-cooked closure; refuse extra or missing assets."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(r'F:\Unreal Projects\Dye')
WORK = ROOT / 'Build/JobBoard-20261015'
name = sys.argv[1] if len(sys.argv) > 1 else 'BoardCook-v1'
module = sys.argv[2] if len(sys.argv) > 2 else 'JobBoard'
assert module in ('JobBoard', 'JobBoard_DyeQuests', 'RuneSchemaQuestItems')
cooked = WORK / name / 'Windows/RSDragonwilds'
out = WORK / ('SharedQuestItemsCandidate-v3' if module == 'RuneSchemaQuestItems' else 'Candidate-v2')
stage = WORK / ('Packing-v4' if module == 'RuneSchemaQuestItems' else 'Packing-v2') / module
assert not (out / 'mods' / module).exists() and not stage.exists(), 'Existing candidate protected'
stage.mkdir(parents=True)
destination = out / 'mods' / module / 'paks' / module
destination.mkdir(parents=True)
packages = json.loads((WORK / ('CookPackages.json' if module == 'JobBoard' else 'SharedCookPackages.json' if module == 'RuneSchemaQuestItems' else 'ContractCookPackages.json')).read_text())
selected = []
lines = []
for package in packages:
    if package.startswith('/ColorsOfMoney/'):
        relative = 'Plugins/ColorsOfMoney/Content/' + package.removeprefix('/ColorsOfMoney/')
    elif package.startswith('/RuneSchemaQuestItems/'):
        relative = 'Plugins/RuneSchemaQuestItems/Content/' + package.removeprefix('/RuneSchemaQuestItems/')
    else:
        relative = 'Content/' + package.removeprefix('/Game/')
    prefix = cooked / relative
    assert prefix.with_suffix('.uasset').is_file(), str(prefix)
    for path in sorted(prefix.parent.glob(prefix.name + '.*')):
        assert path.suffix in ('.uasset', '.uexp', '.ubulk', '.uptnl'), str(path)
        lines.append('"' + str(path) + '" "../../../RSDragonwilds/' + relative + path.suffix + '"')
        selected.append({'package': package, 'extension': path.suffix, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
assert len(list(cooked.rglob('*.uasset'))) == len(packages)
response = stage / 'Board.response.txt'
if module == 'RuneSchemaQuestItems':
    descriptor = ROOT / 'Plugins/RuneSchemaQuestItems/RuneSchemaQuestItems.uplugin'
    lines.append('"' + str(descriptor) + '" "../../../RSDragonwilds/Plugins/RuneSchemaQuestItems/RuneSchemaQuestItems.uplugin"')
    # Keep the project-level common mount root during legacy-to-Zen conversion.
    # This is a neutral receipt, not another runtime asset or registry declaration.
    receipt = stage / 'SharedPackageReceipt.json'
    receipt.write_text(json.dumps({'content_plugin': module, 'packages': packages, 'installed': False}, indent=2))
    lines.append('"' + str(receipt) + '" "../../../RSDragonwilds/Content/Mods/RuneSchemaQuestItems/SharedPackageReceipt.json"')
response.write_text('\n'.join(lines) + '\n')

def run(args, log):
    with (stage / log).open('w') as stream:
        subprocess.run(list(map(str, args)), stdout=stream, stderr=subprocess.STDOUT, check=True)

pak = r'D:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealPak.exe'
retoc = r'C:\Users\user\Documents\Unreal Projects\RSDWFlintlocks\Build\Tools\retoc\retoc.exe'
legacy = stage / 'Board.pak'
toc = destination / (module + '_P.utoc')
run([pak, legacy, '-Create=' + str(response), '-compress'], 'UnrealPak.log')
run([retoc, 'to-zen', '--version', 'UE5_6', legacy, toc], 'ToZen.log')
run([retoc, 'verify', toc], 'Verify.log')
with (stage / 'Manifest.log').open('w') as stream:
    subprocess.run([retoc, 'manifest', str(toc)], cwd=stage, stdout=stream, stderr=subprocess.STDOUT, check=True)
manifest = json.loads((stage / 'pakstore.json').read_text())
actual = {r['packagestoreentry']['packagename'] for r in manifest['oplog']['entries']}
assert actual == set(packages), sorted(actual ^ set(packages))
if module == 'RuneSchemaQuestItems':
    assert all('/Plugins/RuneSchemaQuestItems/Content/' in chunk['filename']
               for row in manifest['oplog']['entries'] for chunk in row['packagedata'])
if module == 'JobBoard':
    buildings = out / 'mods/JobBoard/buildings'
    buildings.mkdir()
    piece = '/ColorsOfMoney/Core/JobBoard/Data/BUILDPIECE_QuestBoard.BUILDPIECE_QuestBoard'
    (buildings / '10-QuestBoard.json').write_text(json.dumps({'jobboard_quest_board': {
        'Asset': piece, 'Unlock': True, 'AddTo': {'Collection': 'Quest Board', 'PageIndex': 0}}}, indent=2))
audit = {'packages': sorted(actual), 'files': selected, 'explicit_response': str(response),
    'core_dye_dependency': False, 'dialogue': False, 'npc_proxy': False, 'installed': False,
    'gameplay_verified': False, 'archive_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.iterdir()}}
(stage / 'PackageAudit.json').write_text(json.dumps(audit, indent=2))
print('JOBBOARD_STANDALONE_PACKED', len(actual))
