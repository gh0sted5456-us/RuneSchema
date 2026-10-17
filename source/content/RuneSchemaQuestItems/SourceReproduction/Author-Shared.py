"""Author the neutral shared consumable class only; do not touch board/providers."""
import json
import base64
import hashlib
from pathlib import Path
import unreal
work = Path(unreal.Paths.project_dir()) / 'Build/JobBoard-20261015'
if not unreal.JobBoardScrollAuthoring.author_scroll_blueprint():
    raise RuntimeError('Shared quest-scroll Blueprint authoring failed')
package = '/RuneSchemaQuestItems/Blueprints/BP_QuestScroll'
bp = unreal.load_asset(package)
template = '/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template'
full = template + '.ITEM_QuestScroll_Template'
identity = base64.urlsafe_b64encode(hashlib.sha256(full.encode()).digest()[:16]).decode().rstrip('=')
asset = unreal.load_asset(template)
if not asset:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', bp.generated_class())
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('ITEM_QuestScroll_Template',
        '/RuneSchemaQuestItems/Items', bp.generated_class(), factory)
for name, value in {'persistence_id': identity, 'internal_name': 'runeschema_quest_scroll_template',
    'name': 'Quest Scroll Template (authoring)', 'flavour_text': 'Authors clone this inert template, assign a new permanent item identity, and bind an existing quest with $QuestScroll. This template grants no quest or reward.',
    'max_stack_size': 25, 'weight': 0.0, 'use_verb': 'Read', 'item_usage_delay': 0.0,
    'gameplay_effects_to_apply': []}.items():
    asset.set_editor_property(name, value)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
    raise RuntimeError('Shared template save failed')
(work / 'SharedCookPackages.json').write_text(json.dumps([package, template], indent=2))
(work / 'SharedTemplateAudit.json').write_text(json.dumps({'class': package + '.BP_QuestScroll_C',
    'template': full, 'persistence_id': identity, 'internal_name': 'runeschema_quest_scroll_template',
    'quest_binding': None, 'rewards': None, 'inert_false_use_item': True, 'installed': False}, indent=2))
unreal.log('RUNESCHEMA_SHARED_QUEST_SCROLL_AUTHORED')
