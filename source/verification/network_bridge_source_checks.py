"""Static contract checks for the generic cooked RuneSchema transport."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
source=(root/'raw/src/Runtime/RegistryBridge.cpp').read_text()
header=(root/'raw/include/Runtime/RegistryBridge.h').read_text()
asset=(root.parent/'v23-network-bridge-staging/UnrealProject/Plugins/RuneSchema/Content/Networking/BPC_RuneSchemaRegistryBridge.uasset').read_bytes()

def require(name,condition):
    if not condition: raise AssertionError(name)

for symbol in (b'ServerRequestRuneSchemaAction',b'ClientRuneSchemaReceipt',b'ClientRuneSchemaNotification'):
    require(f'cooked RPC {symbol!r}',symbol in asset)
require('generic-first legacy fallback',source.index('ServerRequestRuneSchemaAction')<source.index('ServerRequestRegistryAction'))
require('strict channel allowlist','channel=="system.compat"' in source and 'channel=="registry.action"' in source and 'channel=="quest.control"' in source and 'channel=="quest.state"' in source)
require('build and manifest handshake','payload.value("build"' in source and 'registryFingerprint' in source)
require('compatibility is advisory','continuing in degraded mode' in source and 'Generic actions remain available' in source)
require('bounded payload','MaxActionPayloadBytes = 4 * 1024' in source and 'payload.size()>32' in source)
require('rate and revision checks','MaxRequestsPerSecond = 8' in source and 'revision<=window.Revision' in source)
require('owner authority check','ActorComponent:GetOwner' in source and 'Actor:HasAuthority' in source)
require('targeted receipt','ClientRuneSchemaReceipt' in source and 'FUNC_NetClient' in source)
require('notification receive validation','ClientRuneSchemaNotification' in source and 'nlohmann::json::parse(payload)' in source)
require('world reset clears replay state','m_outboundRevision=0;m_requestWindows.clear()' in source)
require('transport methods declared','HandleGenericRequest' in header and 'SendCompatibilityAck' in header)
require('quest authority service','AuthorityChannelHandler QuestControl' in header and 'QuestControl(caster,entity,action,payloadText)' in source)
require('quest resync notification','SendNotification(source,"quest.state"' in source and 'ClientNotification(owner.Result<UObject*>()' in source)
print('PASS: generic network bridge cooked/runtime contract checks')
