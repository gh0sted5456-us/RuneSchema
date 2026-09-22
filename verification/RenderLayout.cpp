// Author-side layout preview. Mock item metadata and placeholder artwork only.
#define main BrowserTestsMain
#include "F2BrowserTests.cpp"
#undef main
int main(){
    auto m=model(56);auto c=m.catalog;
    const char* names[]={"Bronze Helmet","Steel Sword","Ash Logs","Cooking Cape","Clean Water","Iron Ore","Goblin Relic","Nature Rune","Maple Logs","Blue Cape","Leather Boots","Food Parcel"};
    for(std::size_t i=0;i<c.entries[0].size();++i)c.entries[0][i].name=names[i%12];
    c.entries[0][1].runtimeClone=true;c.entries[0][1].cooked=false;c.entries[0][1].name="Example runtime clone";
    c.entries[0][4].consumable=true;c.entries[0][6].quest=true;c.entries[0][11].masterwork=true;
    m.Update(c);m.status="Refresh: checking known assets; nothing is spawned.";m.indexing=true;m.indexHasTotal=true;m.indexDone=152;m.indexTotal=340;m.indexStage="Caching resources";
    dump(m.Render(),"refresh-known.draws");
    m.indexStage="Discovering RSDW paths";m.indexHasTotal=false;dump(m.Render(),"refresh-discovery.draws");
    m.indexStage="Caching items";m.indexHasTotal=true;m.cloneSourcePicker=true;dump(m.Render(),"source-picker.draws");
    return 0;
}
