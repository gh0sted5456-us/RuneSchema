"""Static checks for extensible player and nameplate contracts."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
source=(root/'raw/src/Loader/Spawn/PlayerRules.cpp').read_text()
header=(root/'raw/include/Loader/DragonWildsSpawnLoader.h').read_text()
events=(root/'raw/include/Loader/PlayerActivityEvents.h').read_text()
schema=(root/'raw/include/Generator/LoaderSchemas.h').read_text()

def require(name,value):
    if not value: raise AssertionError(name)

require('exact nameplate objects','BP_Player_Nameplate' in source and 'PlayerNameTextBlock' in source)
require('nameplate reflected targets','Nameplate.Native.Component' in source and 'Nameplate.Native.Widget' in source and 'Nameplate.Native.Text' in source)
require('player reflected targets','Native.Pawn' in source and 'Native.Components.' in source)
require('bounded reflected patch','patch.dump().size()>65536' in source and 'patch.size()>maximum' in source)
require('state native overrides','componentProperties.merge_patch' in source and 'ScaleConfigured' in header)
for operator in ('NotEquals','Exists','GreaterThan','GreaterOrEqual','LessThan','LessOrEqual'):
    require(f'condition {operator}',operator in events and operator in schema)
print('PASS: extensible player/nameplate reflected and conditional contracts')
