"""Update only the JobBoard-owned files created in this authoring session."""
import hashlib
import json
from pathlib import Path
source = Path(__file__).resolve().parent / 'AuthoringSource'
project = Path(r'F:\Unreal Projects\Dye')
files = {
    'JobBoardInventoryComponent.h': 'Plugins/Coinage/Source/Dominion/Public/JobBoardInventoryComponent.h',
    'JobBoardUsableItemInterface.h': 'Plugins/Coinage/Source/Dominion/Public/JobBoardUsableItemInterface.h',
    'JobBoardScrollAuthoring.h': 'Plugins/RuneSchemaBridgeAuthoring/Source/RuneSchemaBridgeAuthoring/Public/JobBoardScrollAuthoring.h',
    'JobBoardScrollAuthoring.cpp': 'Plugins/RuneSchemaBridgeAuthoring/Source/RuneSchemaBridgeAuthoring/Private/JobBoardScrollAuthoring.cpp',
    'RuneSchemaQuestItems.uplugin': 'Plugins/RuneSchemaQuestItems/RuneSchemaQuestItems.uplugin',
}
audit = []
for name, relative in files.items():
    destination = project / relative
    if destination.exists():
        text = destination.read_text()
        assert 'JobBoard' in text or 'RuneSchema Quest Items' in text or 'class DOMINION_API UInventoryComponent' in text or 'class DOMINION_API UUsableItemInterface' in text
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes((source / name).read_bytes())
    audit.append({'path': relative, 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
(project / 'Build/JobBoard-20261015/AuthoringSourceAudit.json').write_text(json.dumps(audit, indent=2))
print('SHARED_QUEST_ITEM_AUTHORING_SOURCE_STAGED', len(audit))
