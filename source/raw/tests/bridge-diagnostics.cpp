#include "Runtime/BridgeDiagnostics.h"
#include <cassert>
#include <string>

using PS::Network::BridgeDiagnostics;

static BridgeDiagnostics::Event E(std::string stage,std::string outcome,std::string reason="accepted",int64_t revision=1) {
    return {.networkRole="dedicated-server",.side="authority",.channel="consumable.authority",
        .owner="player-1",.entity="SummoningPotions:summon_wolf",.action="consume",.requestRevision=revision,
        .correlationId="1-1-"+std::to_string(revision),.stage=std::move(stage),.outcome=std::move(outcome),.reasonCode=std::move(reason)};
}

int main() {
    BridgeDiagnostics d;d.BeginWorld("dedicated-server","0.7.7.5e",1,"registry-ok","match",true);
    // Normal execution, both legal orderings, dedupe, stale/rate rejection,
    // attachment/permit timeout, callback mismatch/failure, reset/disconnect,
    // compatibility mismatch, and terminal receipt are deterministic records.
    for(auto event:{E("local_action_observed","accepted"),E("request_queued","pending","bridge_not_ready"),
        E("request_transmitted","accepted"),E("server_envelope_received","accepted"),
        E("authoritative_inventory_removal_observed","accepted","inventory_confirmed"),E("consumption_permit_created","accepted"),
        E("permit_matched","accepted"),E("cooked_callback_invoked","accepted"),E("callback_completed","executed","executed"),
        E("activation_published","executed","executed"),E("receipt_received","executed","executed"),
        E("local_action_deduplicated","accepted","duplicate_observation",2),E("revision_stale","rejected","stale_revision",2),
        E("rate_limit_rejected","rejected","rate_limited",3),E("request_expired","expired","bridge_not_ready",4),
        E("consumption_permit_expired","expired","inventory_not_confirmed",5),
        E("callback_threw","rejected","callback_contract_mismatch",6),E("world_reset","cancelled","world_changed",7),
        E("player_disconnected","cancelled","player_disconnected",8),E("compatibility_handshake_rejected","rejected","incompatible",9)})d.Record(event,true);
    auto snapshot=d.Snapshot();assert(snapshot["events"].size()==20);assert(snapshot["activePending"].get<uint64_t>()==0);
    const auto encoded=snapshot.dump();assert(encoded.find("/Game/")==std::string::npos&&encoded.find("DataAsset")==std::string::npos);

    // Redaction and bounded storage/cardinality.
    auto unsafe=E("callback_threw","rejected","C:/Users/name/mod.uasset",10);unsafe.owner="Display Name";unsafe.entity="/Game/Secret.Asset";d.Record(unsafe,true);
    snapshot=d.Snapshot();const auto& last=snapshot["events"].back();
    assert(last["owner"]=="Display_Name"&&last["entity"]=="_Game_Secret.Asset");
    for(int i=0;i<400;++i){auto event=E("retry_attempted","pending","bridge_not_ready",11);event.correlationId="same";d.Record(event,true);}
    snapshot=d.Snapshot();assert(snapshot["events"].size()<=BridgeDiagnostics::GlobalCapacity);
    size_t same=0;for(const auto& event:snapshot["events"])if(event["correlationId"]=="same")++same;
    assert(same<=BridgeDiagnostics::CorrelationCapacity);
#if !defined(NDEBUG)
    d.SetDevelopmentFault(BridgeDiagnostics::Fault::Reorder);assert(d.DevelopmentFault()==BridgeDiagnostics::Fault::Reorder);
#endif
}
