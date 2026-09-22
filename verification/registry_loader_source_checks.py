"""Static checks for registry input flexibility and owned diagnostics."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
registry=(root/'raw/src/Loader/DragonWildsRegistryLoader.cpp').read_text()
players=(root/'raw/src/Loader/Spawn/PlayerRules.cpp').read_text()
base=(root/'raw/src/Loader/DragonWildsModLoaderBase.cpp').read_text()
schema=(root/'raw/include/Generator/LoaderSchemas.h').read_text()

def require(name,value):
    if not value: raise AssertionError(name)

require('recommended document', 'SchemaVersion' in registry and 'Entries' in registry)
require('bare entry array', 'return {{"SchemaVersion",1},{"Entries",source}}' in registry)
require('merged manifest conversion', 'RuneSchemaRegistryBridgeManifest' in registry and 'row.value("id"' in registry)
require('FModel guidance', 'FModel export detected' in registry and 'do not paste an arbitrary asset dump' in registry)
require('cooked registry discovery', 'DA_RuneSchemaRegistry' in registry and 'RuneSchemaRegistryJson' in registry and 'RegistryOwner' in registry)
require('custom registry mounts supported', 'mount=="Script"||mount=="Engine"' in registry and '"GraphClass",{{"type","string"},{"pattern","^/[A-Za-z0-9_.-]+/.+_C$"}}' in schema)
require('entry isolation', 'unrelated entries continue' in registry and '"Status","Rejected"' in registry)
require('nameplate identity diagnostics', "Nameplate '{}:{}' in file '{}'" in players)
require('global loader context', "Loader '/{}' rejected mod '{}'" in base)
print('PASS: flexible registry inputs and ownership-aware diagnostics')
