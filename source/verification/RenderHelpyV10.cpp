// Captures the shipped UI model with synthetic names and placeholder icons.
#define main HelpyV10TestMain
#include "HelpyV10Tests.cpp"
#undef main
void dump(const Frame& f,const std::string& name){std::ofstream out(name+".draws");constexpr char h[]="0123456789abcdef";for(const auto& d:f.draws){out<<int(d.kind)<<'|'<<d.box.x<<'|'<<d.box.y<<'|'<<d.box.w<<'|'<<d.box.h<<'|'<<d.font<<'|'<<d.centreX;for(auto c:d.color)out<<'|'<<c;out<<'|';for(unsigned char c:d.text)out<<h[c>>4]<<h[c&15];out<<'|';for(unsigned char c:d.fallback)out<<h[c>>4]<<h[c&15];out<<'\n';}}
int main(){
 auto m=nodesModel();m.indexStage="Local catalogue ready";m.indexFinished=true;m.indexHasTotal=true;m.indexDone=m.indexTotal=116;m.status="Saved reference index in use. No download required.";
 auto c=m.catalog;const char* npcNames[]{"Traveling General Store","Camp Provisioner","Equipment Vendor","Vanilla dialogue NPC","Quest giver (browse only)"};
 for(auto& e:c.entries[1])if(e.nodeKind=="NPC"){auto n=std::stoi(e.id.substr(e.id.find_last_of('c')+1));e.name=npcNames[n%5];if(n%5>2){e.id="@loaded-npc:"+e.path;e.cooked=true;e.runeSchemaManaged=false;e.temporaryAllowed=e.permanentAllowed=false;e.temporaryReason="Vanilla story NPC: browse only. Select a RuneSchema NPC for a timed portable vendor.";e.permanentReason=e.temporaryReason;}}
 m.Update(c);m.tab=Tab::Enemies;m.ToggleNodeFavorite(m.NodeFavoriteKey(m.catalog.entries[1][0],1));dump(m.Render(),"01-npcs");
 m.Activate("node-subtab","1");dump(m.Render(),"02-ai");m.Activate("node-subtab","2");dump(m.Render(),"03-npc-favorites");
 m.Activate("node",m.catalog.entries[1][0].id);m.npcDuration="300";PS::Authoring::PermanentSpawn=false;dump(m.Render(),"04-timed-npc");PS::Authoring::PermanentSpawn=true;dump(m.Render(),"05-permanent-npc");m.Back();
 m.Activate("node",m.catalog.entries[1][6].id);PS::Authoring::PermanentSpawn=false;dump(m.Render(),"06-native-npc-restriction");m.Back();
 m.tab=Tab::Resources;m.Activate("node-subtab","0");m.ToggleNodeFavorite(m.NodeFavoriteKey(m.catalog.entries[2][0],2));dump(m.Render(),"07-trees");m.Activate("node-subtab","1");dump(m.Render(),"08-stone-ore");
 m.Activate("other-resources");dump(m.Render(),"09-other-resources");m.Activate("node-subtab","2");dump(m.Render(),"10-resource-favorites");return 0;
}
