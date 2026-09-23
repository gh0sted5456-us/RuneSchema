#pragma once
// Engine-independent interaction and layout for the in-game Canvas renderer.
// No ImGui, browser process, raw UObject pointers, file IO or game mutations.
#include "Generator/AuthoringPolicy.h"
#include "Generator/ClonePresentation.h"
#include "Generator/HelpyPropertyValue.h"
#include "Generator/HelpyHotkeys.h"
#include "Generator/HelpyItemFilters.h"
#include "Generator/QuickMenuDecorations.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PS::QuickUI {
// One coordinate space from layout through hit testing and Canvas projection.
// The old 780/876 split made the renderer centre and scale a 876 px frame as
// though it were 780 px wide, which caused overflow and pointer drift.
inline constexpr float NavRailWidth=84, ContentWidth=876, DesignWidth=NavRailWidth+ContentWidth, Width=DesignWidth, Height=720;
inline constexpr float HorizontalFit=1.0f;
inline constexpr int Columns=3, ItemsPerPage=12, MaxSelection=64, MaxDrops=16;
inline constexpr float GridX=20, GridY=194, CardWidth=266, CardHeight=92, CardPitchX=278, CardPitchY=98;
enum class Tab { Items=0, Enemies=1, Resources=2 };
struct Entry { std::string id,name,path,icon,search; double power=-1; bool cooked=false; std::string assetClass; bool available=true; std::string detail; std::string appearanceGroup; bool runtimeClone=false,masterwork=false,consumable=false,quest=false; std::string categoryIcon; std::string itemTags{}; bool runeSchemaManaged=false,declaredModded=false,declaredCooked=false; bool cloneEligible=false; std::string cloneReason{}; std::string nodeKind{},resourceFamily{}; bool temporaryAllowed=false,permanentAllowed=false; std::string temporaryReason{},permanentReason{}; bool npcVendor=false,npcQuestGiver=false,npcLore=false,npcDialogue=false; };
struct CatalogIssue {std::string path,reason;};
struct Player { std::string id,name; bool self=false; };
struct Catalog { std::array<std::vector<Entry>,3> entries; std::vector<Player> players; bool authority=false; std::vector<Entry> visuals; std::vector<CatalogIssue> issues; };
struct Drop { std::string item,name,min="1",max="1",chance="100",icon; };
struct Ingredient {std::string path,name,icon,count="1";};
struct Choice {std::string id,name,path,row,array,category;bool grouped=false;std::string icon;};
struct Grant { std::string item; int count=1; };
struct Loot { std::string item; int min=1,max=1; double chance=100; };
enum class Effect { Inherit=0, None=1, Ghost=2 };
struct Command {
    enum class Kind { Refresh, Index, Cancel, Give, Spawn, Players, InspectClone, InspectDetails, ExportRecipe, ExportItemOverrides, CreateClone, UpdateReference, SetHotkey, DismissNpcs } kind=Kind::Refresh;
    std::string player,definition,name,classPath;
    bool resource=false,permanent=false,giveClone=true,acknowledgeExperimental=false;
    int powerLevel=-1;
    std::string iconPath,modTag,persistenceId,appearanceSource,meshField,meshPath;
    bool inspectAppearance=false;
    std::string iconMode="Auto",journalText,journalTitle,journalGroup,journalGroupName;
    bool makeJournal=false,makeRecipe=false,unlockRecipe=false;
    Choice journalTarget,recipeStation;
    std::string recipeCategory,recipeOutput="1";
    std::vector<Ingredient> ingredients;
    std::map<std::string,std::string> overrides;
    int count=1;
    double scale=1;
    Effect effect=Effect::Inherit;
    std::vector<Grant> grants;
    std::vector<Loot> loot;
    bool npc=false; int durationSeconds=0;
    std::string placementMode="Radius";double placementRadius=500,gridX=0,gridY=0,gridSize=100;
};
struct Rect {
    float x=0,y=0,w=0,h=0;
    bool Contains(float px,float py) const { return px>=x && py>=y && px<x+w && py<y+h; }
};
struct Draw {
    enum class Kind { Rectangle, Text, Icon, Badge } kind=Kind::Rectangle;
    Rect box; std::string text; std::array<float,4> color{1,1,1,1}; float font=18; bool centreX=false; std::string fallback;
};
struct Hit { Rect box; std::string action,arg; };
struct Frame { std::vector<Draw> draws; std::vector<Hit> hits,rightHits; };
inline std::string Lower(std::string value) {
    for(auto& c:value) if(c>='A' && c<='Z')c=static_cast<char>(c+('a'-'A'));
    return value;
}
inline bool Matches(std::string_view haystack,std::string_view query) {
    const auto lower=Lower(std::string(haystack)), needle=Lower(std::string(query));
    size_t first=0;
    while(first<needle.size()) {
        while(first<needle.size() && (needle[first]==' ' || needle[first]=='\t'))++first;
        if(first==needle.size())break;
        const auto last=needle.find_first_of(" \t",first);
        const auto token=needle.substr(first,last==std::string::npos?last:last-first);
        if(lower.find(token)==std::string::npos)return false;
        if(last==std::string::npos)break;
        first=last+1;
    }
    return true;
}
inline size_t Previous(std::string_view text,size_t at) {
    if(at==0)return 0;
    --at; while(at && (static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;
    return at;
}
inline size_t Next(std::string_view text,size_t at) {
    if(at>=text.size())return text.size();
    ++at; while(at<text.size() && (static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;
    return at;
}
inline std::string Shorten(std::string_view text,size_t maxBytes) {
    if(text.size()<=maxBytes)return std::string(text);
    if(maxBytes<3)return {};
    size_t end=maxBytes-3;
    while(end && (static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;
    return std::string(text.substr(0,end))+"...";
}
inline std::string UTF8(uint32_t cp) {
    std::string out;
    if(cp<32 || cp==127 || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return out;
    if(cp<0x80)out+=static_cast<char>(cp);
    else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
    else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
    else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
    return out;
}
inline int Integer(std::string_view value,int lo,int hi,const char* label) {
    int out=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),out);
    if(value.empty() || r.ec!=std::errc{} || r.ptr!=value.data()+value.size() || out<lo || out>hi)
        throw std::runtime_error(std::string(label)+" must be "+std::to_string(lo)+".."+std::to_string(hi)+".");
    return out;
}
inline double Number(std::string_view value,double lo,double hi,const char* label) {
    double out=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),out);
    if(value.empty() || r.ec!=std::errc{} || r.ptr!=value.data()+value.size() || !std::isfinite(out) || out<lo || out>hi)
        throw std::runtime_error(std::string(label)+" is outside the supported range.");
    return out;
}
inline void ValidateGrants(const std::vector<Grant>& grants) {
    if(grants.empty() || grants.size()>MaxSelection)throw std::runtime_error("Select 1..64 distinct items.");
    std::set<std::string> unique;
    for(const auto& g:grants) if(g.item.empty() || g.item.front()!='/' || g.item.size()>2048 || g.count<1 || g.count>10000 || !unique.insert(g.item).second)
        throw std::runtime_error("Invalid, duplicate or excessive item request.");
}
// A grant may already have mutated the inventory when confirmation fails.
// Stop on the first error and never retry implicitly or claim atomic rollback.
struct BatchRow { Grant grant; enum class State { Confirmed, Unconfirmed, NotAttempted } state; std::string message; };
template<class Give> std::vector<BatchRow> ExecuteBatch(const std::vector<Grant>& grants,Give&& give) {
    ValidateGrants(grants);std::vector<BatchRow> results;bool stop=false;
    for(const auto& grant:grants) {
        if(stop){results.push_back({grant,BatchRow::State::NotAttempted,"Not attempted after the previous error."});continue;}
        try {give(grant);results.push_back({grant,BatchRow::State::Confirmed,"Inventory delta confirmed."});}
        catch(const std::exception& e){results.push_back({grant,BatchRow::State::Unconfirmed,e.what()});stop=true;}
        catch(...){results.push_back({grant,BatchRow::State::Unconfirmed,"Unknown grant error; inspect inventory before retrying."});stop=true;}
    }
    return results;
}

class Model {
public:
    Catalog catalog;
    Tab tab=Tab::Items;
    std::array<std::string,3> filters;
    std::array<int,3> scroll{};
    int npcTab=0,resourceTab=0; bool includeOtherResources=false;std::string npcRole="All roles";
    std::string npcDuration="300";
    std::set<std::string> selection;
    std::map<std::string,std::string> grantQuantities;
    bool cartOpen=false;int cartPage=0;
    std::string recipient,recipientName,quantity="1",status="Open a world and refresh the game catalog.";
    bool busy=false,indexing=false,closeRequested=false,selectedOnly=false,advancedRuntime=false;
    std::string itemType="All types",itemSource="All sources",pickerType="All types",pickerSource="All sources";
    std::string itemFilterMenu;bool filterForPicker=false;
    bool ItemPasses(const Entry& e,bool picker)const {
        return HelpyItemFilters::Type(picker?pickerType:itemType,e.itemTags,e.assetClass,e.appearanceGroup,e.consumable,e.quest)
            &&HelpyItemFilters::Source(picker?pickerSource:itemSource,e.path,e.cooked,e.runtimeClone,e.runeSchemaManaged,e.declaredModded,e.declaredCooked);
    }
    static bool Placeholder(const Entry& e) {
        const auto name=Lower(e.name);
        return name.starts_with("[ph]")||name.starts_with("ph ")||name.starts_with("ph_");
    }
    bool favoritesTab=false,pickerFavoritesOnly=false,favoritesLoaded=false,favoritesDirty=false,favoritesWritable=true;
    QuickDecorations::Favorites favorites;
    uint64_t favoritesRevision=1;
    std::string favoriteStatus;
    std::optional<Entry> node;
    std::string name,scale="1",count="1",lootFilter,power="1";
    bool npcGridPlacement=false;std::string npcRadius="500",npcGridX="0",npcGridY="0",npcGridSize="100";
    int observedPower=-1;
    struct CloneField {std::string name,type,value,reason;bool editable=false,hasValue=false;std::string visualType{},kind{},scope{},nativeName{},table{},row{};};
    struct DetailRecipe {std::string name,path,output="1";std::vector<std::string> ingredientLabels,stationLabels;std::vector<Ingredient> ingredients;Choice station;};
    std::vector<CloneField> itemDetailFields;std::vector<DetailRecipe> itemDetailRecipes;
    std::map<std::string,std::string> itemDetailEdits;
    std::vector<std::string> itemDetailJournals;bool itemDetailsLoaded=false;
    bool cloneTab=false,cloneReady=false,cloneSourcePicker=false,cloneFieldOpen=false;
    bool cloneGive=true,cloneAcknowledged=false;
    enum class Appearance { Source, Copy, Mesh } appearanceMode=Appearance::Source;
    bool cloneAppearancePicker=false,cloneAppearanceReady=false,cloneAdvancedOpen=false,cloneModeOpen=false;
    bool cloneMeshPicker=false,cloneMeshFieldPicker=false,coverageOpen=false,cloneCreated=false;
    std::string cloneAppearance,cloneModTag="RuneSchema",cloneId,cloneMeshField,cloneMesh,meshFilter;
    Entry cloneSourceInfo,cloneAppearanceInfo;
    int meshScroll=0,coverageScroll=0;
    std::string coverageSummary;

    std::string cloneSource,cloneTitle,cloneName,cloneIcon,cloneFilter,cloneField,cloneValue,cloneSoftDelete;
    std::vector<CloneField> cloneFields;
    std::map<std::string,std::string> cloneEdits;
    int cloneScroll=0;
    bool helpySettingsOpen=false,helpyAboutOpen=false,cloneEditorOpen=false,cloneFieldPicker=false,cloneRawFields=false,companionsOpen=false;
    bool itemDetailsOpen=false,itemDetailFieldMode=false;std::string itemDetailsPath;int itemDetailsTab=0,itemDetailsPage=0;
    bool recipeOpen=false,recipeDetailsMode=false,ingredientPicker=false,choicePicker=false,iconPicker=false;
    std::string recipeTargetPath,recipeTargetName;
    std::string helpyKey="F2",selectedHotkey="F2",cloneKind="JSON",choiceKind,choiceFilter;
    std::string cloneFlavour,clonePower;
    enum class IconMode { Auto, Source, Appearance, Override } iconMode=IconMode::Auto;
    bool makeJournal=false,makeRecipe=false,unlockRecipe=false,journalTextEdited=false;
    std::string journalText,journalTitle,journalGroup,journalGroupName,recipeCategory="Helpy",recipeOutput="1";
    Choice journalTarget,recipeStation;
    std::vector<Choice> journalChoices,stationChoices;
    std::vector<Ingredient> ingredients;
    int draftPage=0,choicePage=0,ingredientPage=0;
    struct Draft {
        std::string source,title,name,icon,mod,id,meshField,mesh,appearance,soft,flavour,power;
        Entry sourceInfo,appearanceInfo;std::vector<CloneField> fields;
        std::map<std::string,std::string> edits;
        Appearance mode=Appearance::Source;IconMode iconMode=IconMode::Auto;
        bool ready=false,appearanceReady=false,created=false,permanent=false,give=true;
        bool journal=false,recipe=false,unlock=false,journalTextEdited=false;
        std::string quantity="1",journalText,journalTitle,journalGroup,journalGroupName,category,output="1";
        Choice journalTarget,station;std::vector<Ingredient> ingredients;
    };
    std::vector<Draft> drafts;int activeDraft=-1;
    const CloneField* CurrentCloneField()const {
        const auto& fields=itemDetailFieldMode?itemDetailFields:cloneFields;
        for(const auto& f:fields)if(f.name==cloneField)return &f;
        return nullptr;
    }
    std::string EffectiveIcon()const {
        if(iconMode==IconMode::Override)return cloneIcon;
        if(iconMode==IconMode::Appearance)return cloneAppearanceReady?cloneAppearanceInfo.icon:std::string{};
        if(iconMode==IconMode::Auto&&appearanceMode==Appearance::Copy&&cloneAppearanceReady&&!cloneAppearanceInfo.icon.empty())return cloneAppearanceInfo.icon;
        return cloneSourceInfo.icon;
    }
    void SaveDraft() {
        if(activeDraft<0||static_cast<std::size_t>(activeDraft)>=drafts.size())return;
        auto& d=drafts[static_cast<std::size_t>(activeDraft)];
        d.source=cloneSource;d.title=cloneTitle;d.name=cloneName;d.icon=cloneIcon;d.mod=cloneModTag;d.id=cloneId;
        d.meshField=cloneMeshField;d.mesh=cloneMesh;d.appearance=cloneAppearance;d.soft=cloneSoftDelete;
        d.flavour=cloneFlavour;d.power=clonePower;d.sourceInfo=cloneSourceInfo;d.appearanceInfo=cloneAppearanceInfo;
        d.fields=cloneFields;d.edits=cloneEdits;d.mode=appearanceMode;d.iconMode=iconMode;d.ready=cloneReady;
        d.appearanceReady=cloneAppearanceReady;d.created=cloneCreated;d.permanent=Authoring::PermanentAsset.load();d.give=cloneGive;d.quantity=quantity;
        d.journal=makeJournal;d.recipe=makeRecipe;d.unlock=unlockRecipe;d.journalTextEdited=journalTextEdited;
        d.journalText=journalText;d.journalTitle=journalTitle;d.journalGroup=journalGroup;d.journalGroupName=journalGroupName;
        d.category=recipeCategory;d.output=recipeOutput;d.journalTarget=journalTarget;d.station=recipeStation;d.ingredients=ingredients;
    }
    void OpenDraft(int index) {
        if(busy)throw std::runtime_error("Wait for the pending item operation.");
        if(index<0||static_cast<std::size_t>(index)>=drafts.size())return;
        SaveDraft();activeDraft=index;const auto& d=drafts[static_cast<std::size_t>(index)];
        cloneSource=d.source;cloneTitle=d.title;cloneName=d.name;cloneIcon=d.icon;cloneModTag=d.mod;cloneId=d.id;
        cloneMeshField=d.meshField;cloneMesh=d.mesh;cloneAppearance=d.appearance;cloneSoftDelete=d.soft;
        cloneFlavour=d.flavour;clonePower=d.power;cloneSourceInfo=d.sourceInfo;cloneAppearanceInfo=d.appearanceInfo;
        cloneFields=d.fields;cloneEdits=d.edits;appearanceMode=d.mode;iconMode=d.iconMode;cloneReady=d.ready;
        cloneAppearanceReady=d.appearanceReady;cloneCreated=d.created;Authoring::PermanentAsset=d.permanent;cloneGive=d.give;quantity=d.quantity;
        makeJournal=d.journal;makeRecipe=d.recipe;unlockRecipe=d.unlock;journalTextEdited=d.journalTextEdited;
        journalText=d.journalText;journalTitle=d.journalTitle;journalGroup=d.journalGroup;journalGroupName=d.journalGroupName;
        recipeCategory=d.category;recipeOutput=d.output;journalTarget=d.journalTarget;recipeStation=d.station;ingredients=d.ingredients;
        cloneAcknowledged=false;cloneFilter.clear();cloneScroll=0;cloneEditorOpen=true;focus.clear();
    }
    void NewDraft() {
        if(drafts.size()>=32)throw std::runtime_error("At most 32 item drafts per session.");
        if(busy)throw std::runtime_error("Wait for the pending item operation.");
        SaveDraft();Draft d;d.mod=cloneModTag;d.id=ClonePresentation::NewId(d.mod);d.permanent=Authoring::PermanentAsset.load();
        drafts.push_back(std::move(d));OpenDraft(static_cast<int>(drafts.size()-1));
    }
    std::vector<std::size_t> Choices()const {
        const auto& choices=choiceKind=="journal"?journalChoices:stationChoices;
        std::vector<std::size_t> out;for(std::size_t i=0;i<choices.size();++i)
            if(Matches(choices[i].name+" "+choices[i].path+" "+choices[i].row,choiceFilter))out.push_back(i);
        return out;
    }
    std::string indexStage="Not indexed",indexDetail;
    std::size_t indexDone=0,indexTotal=0;
    bool indexHasTotal=false,indexFinished=false;
    unsigned progressFrame=0;
    std::string catalogStatus,actionTitle,actionMessage;
    bool actionReportOpen=false,actionSuccess=false;
    Effect effect=Effect::Inherit;
    std::vector<Drop> drops;
    struct GrantReportLine {std::string name,state,message;};
    std::vector<GrantReportLine> grantReport;
    bool reportOpen=false;
    int reportScroll=0;
    bool lootPicker=false,playerPicker=false;
    int lootScroll=0,dropScroll=0,playerScroll=0;
    std::string focus;
    size_t caret=0;
    bool selectAll=false;

    void Reset() {
        auto retained=std::move(favorites);const auto loaded=favoritesLoaded,writable=favoritesWritable,dirty=favoritesDirty;
        const auto message=favoriteStatus;*this=Model{};
        favorites=std::move(retained);favoritesLoaded=loaded;favoritesWritable=writable;favoritesDirty=dirty;favoriteStatus=message;
    }
    void SetFavorites(QuickDecorations::Favorites value) {favorites=std::move(value);++favoritesRevision;}
    bool IsFavorite(const std::string& path)const {return favorites.contains(path);}
    void ToggleFavorite(const std::string& path) {
        if(favorites.contains(path))favorites.erase(path);
        else {
            const auto* item=FindItem(path);
            if(!item||!QuickDecorations::ValidFavoritePath(path))return;
            if(favorites.size()>=QuickDecorations::MaxFavorites)throw std::runtime_error("Favorite limit reached (4096 items).");
            favorites[path]=Shorten(item->name,512);
        }
        ++favoritesRevision;favoritesDirty=true;
        favoriteStatus=favoritesWritable?"Favorites changed; saving locally.":"Favorites changed for this session only; Favorites file is unavailable.";
    }
    HelpyNodes::Kind NodeKind(const Entry& e,int slot)const {return HelpyNodes::EntryKind(slot,e.nodeKind,e.path,e.resourceFamily);}
    std::string NodeFavoriteKey(const Entry& e,int slot)const {return HelpyNodes::FavoriteKey(NodeKind(e,slot),e.id);}
    bool NodeFavorites(int slot)const {return slot==1?npcTab==2:slot==2&&resourceTab==2;}
    mutable std::array<std::vector<Entry>,2> nodeFavoriteEntries;
    mutable std::array<uint64_t,2> nodeFavoriteCatalog{},nodeFavoriteRevision{};
    void ToggleNodeFavorite(const std::string& key) {
        if(!HelpyNodes::ValidFavoriteKey(key))return;
        if(favorites.contains(key))favorites.erase(key);
        else {
            const Entry* selected=nullptr;
            for(int slot=1;slot<3;++slot)for(const auto& e:catalog.entries[slot])if(NodeFavoriteKey(e,slot)==key)selected=&e;
            if(!selected)return;
            if(favorites.size()>=QuickDecorations::MaxFavorites)throw std::runtime_error("Favorite limit reached (4096 entries).");
            favorites[key]=Shorten(selected->name,512);
        }
        ++favoritesRevision;favoritesDirty=true;
        favoriteStatus=favoritesWritable?"Favorites changed; saving locally.":"Favorites changed for this session only; file unavailable.";
    }
    const std::vector<Entry>& BrowserEntries(int index)const {
        if(NodeFavorites(index)) {
            const auto k=static_cast<std::size_t>(index-1);
            if(nodeFavoriteCatalog[k]==catalogRevision&&nodeFavoriteRevision[k]==favoritesRevision)return nodeFavoriteEntries[k];
            auto& result=nodeFavoriteEntries[k];result.clear();
            std::map<std::string,const Entry*> loaded;
            for(const auto& e:catalog.entries[index])loaded[NodeFavoriteKey(e,index)]=&e;
            for(const auto& [key,label]:favorites)if(HelpyNodes::FavoriteIn(index,key)) {
                if(const auto at=loaded.find(key);at!=loaded.end())result.push_back(*at->second);
                else {Entry missing;missing.id=std::string(HelpyNodes::FavoriteId(key));missing.name=label;missing.available=false;
                    missing.nodeKind=key.starts_with("@npc:")?"NPC":key.starts_with("@ai:")?"AI":"Resource";
                    const auto colon=missing.id.find(':');if(colon!=std::string::npos&&missing.id.substr(colon+1).starts_with('/'))missing.path=missing.id.substr(colon+1);
                    missing.detail="Unavailable: refresh or restore its source mod. Favorites do not respawn definitions.";
                    result.push_back(std::move(missing));}
            }
            std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){const auto an=Lower(a.name),bn=Lower(b.name);return an==bn?a.id<b.id:an<bn;});
            nodeFavoriteCatalog[k]=catalogRevision;nodeFavoriteRevision[k]=favoritesRevision;return result;
        }
        if(index!=0||!favoritesTab)return catalog.entries.at(static_cast<std::size_t>(index));
        if(favoriteCatalogRevision==catalogRevision&&favoriteListRevision==favoritesRevision)return favoriteEntries;
        favoriteEntries.clear();std::map<std::string,const Entry*> loaded;
        for(const auto& e:catalog.entries[0])loaded[e.path]=&e;
        for(const auto& [path,favoriteName]:favorites) {
            if(!QuickDecorations::ValidFavoritePath(path))continue;
            if(const auto found=loaded.find(path);found!=loaded.end())favoriteEntries.push_back(*found->second);
            else {Entry e;e.id=path;e.path=path;e.name=favoriteName;e.available=false;e.detail="Unavailable: scan the catalogue or restore its mod. Favorites do not recreate runtime clones.";favoriteEntries.push_back(std::move(e));}
        }
        std::sort(favoriteEntries.begin(),favoriteEntries.end(),[](const auto& a,const auto& b){
            const auto an=Lower(a.name),bn=Lower(b.name);return an==bn?a.path<b.path:an<bn;
        });
        favoriteCatalogRevision=catalogRevision;favoriteListRevision=favoritesRevision;return favoriteEntries;
    }
    std::string ActiveSearch()const {
        if(itemDetailsOpen||!itemFilterMenu.empty()||actionReportOpen||helpySettingsOpen||helpyAboutOpen||choicePicker||coverageOpen||reportOpen||playerPicker||cloneModeOpen||cloneMeshFieldPicker||cloneFieldOpen)return {};
        if(ItemPickerOpen())return lootFilter;
        if(companionsOpen||recipeOpen)return {};
        if(cloneMeshPicker)return meshFilter;
        if(node||(tab==Tab::Items&&cloneTab))return {};
        return filters[static_cast<std::size_t>(tab)];
    }
    std::vector<std::size_t> CloneMatches()const {
        std::vector<std::size_t> out;
        for(std::size_t i=0;i<cloneFields.size();++i)
            if(!Authoring::IdentityField(cloneFields[i].name)&&!Authoring::SoftDeleteName(cloneFields[i].name)&&Matches(cloneFields[i].name+" "+cloneFields[i].type+" "+cloneFields[i].scope+" "+cloneFields[i].nativeName,cloneFilter)
                &&(cloneRawFields?!cloneFields[i].scope.empty():cloneFields[i].scope.empty())
                &&(cloneFieldPicker||cloneEdits.contains(cloneFields[i].name)))out.push_back(i);
        return out;
    }
    void AcceptCloneSource(std::string source,std::vector<CloneField> fields,std::string softDelete) {
        cloneSource=std::move(source);cloneFields=std::move(fields);cloneSoftDelete=std::move(softDelete);
        cloneEdits.clear();cloneFilter.clear();cloneScroll=0;cloneAcknowledged=false;cloneReady=true;
        const auto* item=FindItem(cloneSource);cloneTitle=item?item->name:cloneSource;
        cloneFlavour.clear();clonePower.clear();
        for(const auto& f:cloneFields) {
            if(f.name=="FlavourText"&&f.hasValue)try{cloneFlavour=HelpyPropertyValue::Decode(f.value,"Text");}catch(...){}
            if(f.name=="PowerLevel"&&f.hasValue)clonePower=f.value;
        }
        journalText=cloneFlavour;journalTitle.clear();journalTextEdited=false;iconMode=IconMode::Auto;
        makeJournal=false;makeRecipe=false;unlockRecipe=false;ingredients.clear();journalTarget={};recipeStation={};
        journalGroup.clear();journalGroupName.clear();recipeCategory="Helpy";recipeOutput="1";
        cloneName=cloneTitle+" Clone";if(cloneName.size()>256)cloneName.clear();cloneIcon.clear();
        cloneSourceInfo=item?*item:Entry{};
        if(!item){cloneSourceInfo.id=cloneSource;cloneSourceInfo.path=cloneSource;cloneSourceInfo.name=cloneTitle;}
        cloneAppearance.clear();cloneAppearanceInfo={};cloneAppearanceReady=false;appearanceMode=Appearance::Source;
        cloneMesh.clear();cloneMeshField.clear();cloneAdvancedOpen=false;cloneCreated=false;
        cloneId=ClonePresentation::NewId(cloneModTag);
        for(const auto& field:cloneFields)if(!field.visualType.empty()){cloneMeshField=field.name;break;}
    }
    const CloneField* MeshSlot() const {
        for(const auto& field:cloneFields)if(field.name==cloneMeshField&&!field.visualType.empty())return &field;
        return nullptr;
    }
    const std::vector<std::size_t>& MeshMatches() const {
        auto& cache=matchCache[4];const auto* field=MeshSlot();
        const auto query=meshFilter+"\n"+(field?field->visualType:std::string{});
        if(cache.revision==catalogRevision&&cache.query==query)return cache.indices;
        cache.indices.clear();cache.revision=catalogRevision;cache.query=query;
        for(std::size_t i=0;i<catalog.visuals.size();++i) {
            const auto& v=catalog.visuals[i];
            if(field&&ClonePresentation::CompatibleVisual(field->visualType,v.assetClass)&&Matches(v.name+" "+v.path,meshFilter))cache.indices.push_back(i);
        }
        return cache.indices;
    }
    bool ItemPickerOpen()const {return lootPicker||cloneSourcePicker||cloneAppearancePicker||ingredientPicker||iconPicker;}
    Command InspectCloneCommand(const std::string& source,bool appearance=false)const {
        Authoring::ValidateObjectPath(source);
        Command c;c.kind=Command::Kind::InspectClone;c.player=recipient;c.definition=source;c.inspectAppearance=appearance;return c;
    }
    Command RecipeExportCommand()const {
        if(!recipeDetailsMode||recipeTargetPath.empty())throw std::runtime_error("Open an item recipe from Item Details first.");
        Authoring::ValidateObjectPath(recipeTargetPath);
        if(recipeStation.path.empty()||ingredients.empty())throw std::runtime_error("Select a crafting station and at least one ingredient.");
        Integer(recipeOutput,1,10000,"Recipe output count");
        for(const auto& i:ingredients){Authoring::ValidateObjectPath(i.path);Integer(i.count,1,10000,"Ingredient count");}
        if(recipeStation.array.empty()&&recipeCategory.empty())throw std::runtime_error("Enter a recipe category label.");
        Command c;c.kind=Command::Kind::ExportRecipe;c.definition=recipeTargetPath;c.makeRecipe=true;c.unlockRecipe=unlockRecipe;
        c.recipeStation=recipeStation;c.recipeCategory=recipeCategory;c.recipeOutput=recipeOutput;c.ingredients=ingredients;return c;
    }
    Command ItemOverrideExportCommand()const {
        if(itemDetailsPath.empty()||itemDetailEdits.empty())throw std::runtime_error("Edit at least one item field before exporting overrides.");
        Authoring::ValidateObjectPath(itemDetailsPath);
        Command c;c.kind=Command::Kind::ExportItemOverrides;c.definition=itemDetailsPath;c.overrides=itemDetailEdits;return c;
    }
    Command CloneCommand()const {
        RequireAuthority();
        if(!cloneReady || cloneSource.empty())throw std::runtime_error("Inspect a loaded source item first.");
        if(cloneCreated)throw std::runtime_error("This clone was already created. Generate a new ID to create another.");
        if(!cloneAcknowledged)throw std::runtime_error("Acknowledge disposable-save testing before creating a clone.");
        if(!Authoring::PermanentAsset.load() && cloneSoftDelete.empty())
            throw std::runtime_error("This item has no verified soft-delete Boolean. Temporary cloning is blocked.");
        Command c;c.kind=Command::Kind::CreateClone;c.player=recipient;c.definition=cloneSource;
        if(!ClonePresentation::ValidId(cloneId,cloneModTag))throw std::runtime_error("Generate a valid clone ID first.");
        c.modTag=cloneModTag;c.persistenceId=cloneId;
        if(appearanceMode==Appearance::Copy) {
            if(!cloneAppearanceReady||cloneAppearance.empty())throw std::runtime_error("Select and inspect a cooked appearance donor first.");
            c.appearanceSource=cloneAppearance;
        }else if(appearanceMode==Appearance::Mesh) {
            if(!MeshSlot()||cloneMesh.empty())throw std::runtime_error("Choose a compatible cooked mesh for a supported visual slot.");
            c.meshField=cloneMeshField;c.meshPath=cloneMesh;
        }
        c.name=cloneName;c.iconPath=EffectiveIcon();c.overrides=cloneEdits;
        const char* iconModes[]{"Auto","Source","Appearance","Override"};c.iconMode=iconModes[static_cast<int>(iconMode)];
        if(iconMode==IconMode::Appearance&&(!cloneAppearanceReady||appearanceMode!=Appearance::Copy))throw std::runtime_error("Choose a cooked appearance donor before using its icon.");
        if(iconMode==IconMode::Override&&cloneIcon.empty())throw std::runtime_error("Enter or select an icon override.");
        c.makeJournal=makeJournal;c.makeRecipe=makeRecipe;c.unlockRecipe=unlockRecipe;
        c.journalText=journalTextEdited?journalText:cloneFlavour;c.journalTitle=journalTitle.empty()?cloneName:journalTitle;
        c.journalTarget=journalTarget;c.journalGroup=journalGroup;c.journalGroupName=journalGroupName;
        c.recipeStation=recipeStation;c.recipeCategory=recipeCategory;c.recipeOutput=recipeOutput;c.ingredients=ingredients;
        if((makeJournal||makeRecipe)&&!Authoring::PermanentAsset.load())throw std::runtime_error("Journal/recipe companions require a permanent item definition.");
        if(makeJournal&&journalTarget.path.empty())throw std::runtime_error("Select a journal subcategory.");
        if(makeJournal&&journalTarget.grouped&&(journalGroup.empty()||journalGroupName.empty()))throw std::runtime_error("Enter a group ID and display name for this journal category.");
        if(makeRecipe) {
            if(recipeStation.path.empty()||ingredients.empty())throw std::runtime_error("Select a crafting station and at least one ingredient.");
            Integer(recipeOutput,1,10000,"Recipe output count");
            for(const auto& i:ingredients){Authoring::ValidateObjectPath(i.path);Integer(i.count,1,10000,"Ingredient count");}
            if(recipeStation.array.empty()&&recipeCategory.empty())throw std::runtime_error("Enter a recipe category label.");
        }
        c.count=Integer(quantity,1,10000,"Quantity");
        c.permanent=Authoring::PermanentAsset.load();c.giveClone=cloneGive;c.acknowledgeExperimental=cloneAcknowledged;
        if(c.name.size()>256)throw std::runtime_error("Clone name exceeds 256 bytes.");
        if(!c.iconPath.empty())Authoring::ValidateObjectPath(c.iconPath);
        return c;
    }
    void Update(Catalog value) {
        if(const auto* selected=Recipient())recipientName=selected->name;
        catalog=std::move(value);++catalogRevision;
        // Controller object paths can be replaced by travel, reconnect or a roster
        // refresh. Keep an explicit recipient when that path remains live; otherwise
        // rebind only when the remembered display identity has one unambiguous match.
        if(!recipient.empty()&&!Recipient()&&!recipientName.empty()) {
            const Player* match=nullptr;
            for(const auto& p:catalog.players)if(p.name==recipientName){if(match){match=nullptr;break;}match=&p;}
            if(match)recipient=match->id;
        }
        if(recipient.empty())for(const auto& p:catalog.players)if(p.self){recipient=p.id;recipientName=p.name;break;}
        if(const auto* selected=Recipient())recipientName=selected->name;
        // An unloaded/replaced definition is not a valid outstanding selection.
        if(node && !FindNode(node->id)) {node.reset();lootPicker=false;focus.clear();status="The selected definition changed; choose it again.";}
        ClampScroll();
    }
    const Player* Recipient() const {
        for(const auto& p:catalog.players)if(p.id==recipient)return &p;
        return nullptr;
    }
    const Entry* FindItem(const std::string& path) const {
        for(const auto& e:catalog.entries[0])if(e.path==path)return &e;
        return nullptr;
    }
    const Entry* FindNode(const std::string& id) const {
        for(size_t t=1;t<3;++t)for(const auto& e:catalog.entries[t])if(e.id==id)return &e;
        return nullptr;
    }
    const std::vector<size_t>& Filtered(int index) const {
        const auto slot=static_cast<size_t>(index);
        auto& cache=matchCache.at(slot);
        const bool only=index==0&&selectedOnly;
        const auto query=filters.at(slot)+"\n"+(index==0&&favoritesTab?"favorites":"all")+(index==0?"\n"+itemType+"\n"+itemSource:"\n"+std::to_string(index==1?npcTab:resourceTab)+std::to_string(includeOtherResources)+(index==1?"\n"+npcRole:""));
        if(cache.revision==catalogRevision&&cache.query==query&&cache.favoriteRevision==favoritesRevision&&cache.onlySelected==only
            &&(!only||cache.selected==selection))return cache.indices;
        cache.indices.clear();cache.query=query;cache.favoriteRevision=favoritesRevision;cache.onlySelected=only;
        cache.revision=catalogRevision;cache.selected=only?selection:std::set<std::string>{};
        const auto& entries=BrowserEntries(index);
        for(size_t i=0;i<entries.size();++i) {
            const auto& e=entries[i];
            if((only&&!selection.contains(e.path))||(index==0&&(Placeholder(e)||!ItemPasses(e,false))))continue;
            if(index!=0&&!HelpyNodes::InTab(NodeKind(e,index),index==1?npcTab:resourceTab,includeOtherResources))continue;
            if(index==1&&npcTab==0&&((npcRole=="Vendors"&&!e.npcVendor)||(npcRole=="Quest givers"&&!e.npcQuestGiver)
                ||(npcRole=="Dialogue"&&!e.npcDialogue)||(npcRole=="Lore"&&!e.npcLore)))continue;
            if(Matches(e.name+" "+e.path+" "+e.search,filters.at(slot)))cache.indices.push_back(i);
        }
        return cache.indices;
    }
    const std::vector<size_t>& LootMatches() const {
        auto& cache=matchCache[3];
        const bool cooked=cloneAppearancePicker;
        const auto targetClass=cloneAppearancePicker?cloneSourceInfo.assetClass:std::string{};
        const auto query=lootFilter+"\n"+(cooked?"cooked":"any")+targetClass+"\n"+(cloneAppearancePicker?cloneSourceInfo.appearanceGroup:std::string{})+"\n"+pickerType+"\n"+pickerSource+"\n"+(cloneSourcePicker?"clone-source":"other");
        if(cache.revision==catalogRevision&&cache.query==query&&cache.favoriteRevision==favoritesRevision&&cache.onlyFavorites==pickerFavoritesOnly)return cache.indices;
        cache.indices.clear();cache.query=query;cache.revision=catalogRevision;cache.favoriteRevision=favoritesRevision;cache.onlyFavorites=pickerFavoritesOnly;
        for(size_t i=0;i<catalog.entries[0].size();++i) {
            const auto& e=catalog.entries[0][i];
            if(Placeholder(e)||!e.available||(cooked&&!e.cooked)||((cloneSourcePicker||cloneAppearancePicker)&&!e.cloneEligible)
                ||(pickerFavoritesOnly&&!IsFavorite(e.path))||!ItemPasses(e,true))continue;
            if(!targetClass.empty()&&!ClonePresentation::CompatibleAppearance(targetClass,e.assetClass,cloneSourceInfo.appearanceGroup,e.appearanceGroup))continue;
            if(Matches(e.name+" "+e.path+" "+e.search,lootFilter))cache.indices.push_back(i);
        }
        return cache.indices;
    }
    // Legacy *Scroll members now hold zero-based page numbers for catalogue browsers.
    void ClampScroll() {
        for(int t=0;t<3;++t)scroll[t]=QuickDecorations::ClampPage(scroll[t],Filtered(t).size(),ItemsPerPage);
        lootScroll=QuickDecorations::ClampPage(lootScroll,LootMatches().size(),ItemsPerPage);
        dropScroll=std::clamp(dropScroll,0,std::max(0,static_cast<int>(drops.size())-4));
        reportScroll=QuickDecorations::ClampPage(reportScroll,grantReport.size(),7);
        cloneScroll=QuickDecorations::ClampPage(cloneScroll,CloneMatches().size(),cloneFieldPicker?7:5);
        const std::size_t detailCount=itemDetailsTab==0?itemDetailFields.size():itemDetailsTab==1?itemDetailRecipes.size():itemDetailJournals.size();
        itemDetailsPage=QuickDecorations::ClampPage(itemDetailsPage,detailCount,itemDetailsTab==1?3:7);
        draftPage=QuickDecorations::ClampPage(draftPage,drafts.size(),6);
        choicePage=QuickDecorations::ClampPage(choicePage,Choices().size(),8);
        ingredientPage=QuickDecorations::ClampPage(ingredientPage,ingredients.size(),5);
        meshScroll=QuickDecorations::ClampPage(meshScroll,MeshMatches().size(),9);
        coverageScroll=QuickDecorations::ClampPage(coverageScroll,catalog.issues.size(),7);
        playerScroll=QuickDecorations::ClampPage(playerScroll,catalog.players.size(),9);
        cartPage=QuickDecorations::ClampPage(cartPage,selection.size(),7);
    }
    std::string* Field(const std::string& id) {
        if(id=="npc-duration")return &npcDuration;
        if(id=="clone-flavour")return &cloneFlavour;
        if(id=="clone-power")return &clonePower;
        if(id=="journal-text")return &journalText;
        if(id=="journal-title")return &journalTitle;
        if(id=="journal-group")return &journalGroup;
        if(id=="journal-group-name")return &journalGroupName;
        if(id=="recipe-category")return &recipeCategory;
        if(id=="recipe-output")return &recipeOutput;
        if(id=="choice-filter")return &choiceFilter;
        for(auto& i:ingredients)if(id=="ingredient:"+i.path)return &i.count;
        if(id=="clone-mod")return &cloneModTag;
        if(id=="mesh-filter")return &meshFilter;
        if(id=="filter")return &filters[static_cast<int>(tab)];
        if(id=="quantity")return &quantity;
        if(id.starts_with("grant-count:")) {
            auto found=grantQuantities.find(id.substr(12));
            return found==grantQuantities.end()?nullptr:&found->second;
        }
        if(id=="power")return &power;
        if(id=="clone-name")return &cloneName;
        if(id=="clone-icon")return &cloneIcon;
        if(id=="clone-filter")return &cloneFilter;
        if(id=="clone-value")return &cloneValue;
        if(id=="name")return &name;
        if(id=="scale")return &scale;
        if(id=="count")return &count;
        if(id=="npc-radius")return &npcRadius;
        if(id=="npc-grid-x")return &npcGridX;
        if(id=="npc-grid-y")return &npcGridY;
        if(id=="npc-grid-size")return &npcGridSize;
        if(id=="loot-filter")return &lootFilter;
        for(auto& d:drops) {
            if(id=="min:"+d.item)return &d.min;
            if(id=="max:"+d.item)return &d.max;
            if(id=="chance:"+d.item)return &d.chance;
        }
        return nullptr;
    }
    void Focus(std::string id) {focus=std::move(id);selectAll=false;auto* f=Field(focus);caret=f?f->size():0;}
    void Edited() {
        if(focus=="clone-mod"){cloneId=ClonePresentation::NewId(cloneModTag);cloneCreated=false;cloneAcknowledged=false;}
        if(focus=="clone-name")cloneEdits.erase("Name");
        if(focus=="clone-flavour"){cloneEdits["FlavourText"]=HelpyPropertyValue::Quote(cloneFlavour);if(!journalTextEdited)journalText=cloneFlavour;}
        if(focus=="clone-power"){if(clonePower.empty())cloneEdits.erase("PowerLevel");else cloneEdits["PowerLevel"]=clonePower;}
        if(focus=="journal-text")journalTextEdited=true;
        if(focus=="choice-filter")choicePage=0;
        if(focus=="clone-icon"){cloneEdits.erase("Icon");iconMode=IconMode::Override;}
        if(focus=="mesh-filter")meshScroll=0;
        if(focus=="filter")scroll[static_cast<int>(tab)]=0;
        if(focus=="loot-filter")lootScroll=0;
        if(focus=="clone-filter")cloneScroll=0;
        if(focus=="power")try {observedPower=Integer(power,Authoring::MinPower,Authoring::MaxPower,"Power level");Authoring::EnemyPower=observedPower;}catch(...){}
    }
    void Character(uint32_t cp) {
        if(!itemFilterMenu.empty()||reportOpen||actionReportOpen||helpySettingsOpen||helpyAboutOpen||playerPicker||coverageOpen||cloneModeOpen||cloneMeshFieldPicker)return;
        auto* value=Field(focus);if(!value)return;
        if(busy&&focus!="filter"&&focus!="loot-filter"&&focus!="mesh-filter"&&focus!="choice-filter")return;
        const bool multiline=focus=="clone-flavour"||focus=="journal-text"||(focus=="clone-value"&&(cloneKind=="Text"||cloneKind=="JSON"));
        const auto text=(cp==13||cp==10)&&multiline?std::string("\n"):UTF8(cp);if(text.empty())return;
        const size_t limit=(focus=="clone-value"||focus=="clone-flavour"||focus=="journal-text")?Authoring::MaxCloneValueBytes:
            focus=="clone-mod"?64:focus=="clone-icon"?2048:focus=="name"||focus=="clone-name"?256:
            focus=="filter"||focus=="loot-filter"||focus=="mesh-filter"||focus=="choice-filter"?2048:focus=="clone-filter"||focus=="journal-group"||focus=="journal-group-name"||focus=="recipe-category"?128:focus=="journal-title"?256:12;
        if(selectAll){value->clear();caret=0;selectAll=false;}
        if(value->size()+text.size()>limit)return;
        caret=std::min(caret,value->size());value->insert(caret,text);caret+=text.size();Edited();
    }
    // Virtual-key values are intentionally isolated here; the native adapter supplies them.
    void Key(int key,bool control=false) {
        if(key==27){Back();return;}
        if(key==33||key==34){Page(key==33?1:-1);return;}
        if(!itemFilterMenu.empty()||reportOpen||actionReportOpen||helpySettingsOpen||helpyAboutOpen||playerPicker||coverageOpen||cloneModeOpen||cloneMeshFieldPicker)return;
        if(key==9) {
            std::vector<std::string> ids=cloneFieldOpen?std::vector<std::string>{"clone-value"}:
                choicePicker?std::vector<std::string>{"choice-filter"}:
                ItemPickerOpen()?std::vector<std::string>{"loot-filter"}:
                cloneMeshPicker?std::vector<std::string>{"mesh-filter"}:
                recipeOpen?std::vector<std::string>{"recipe-category","recipe-output"}:
                companionsOpen?std::vector<std::string>{"journal-title","journal-text","journal-group","journal-group-name"}:
                node?(node->nodeKind=="NPC"?(npcGridPlacement?std::vector<std::string>{"name","scale","npc-grid-x","npc-grid-y","npc-grid-size"}:std::vector<std::string>{"name","scale","npc-radius"}):tab==Tab::Enemies&&Authoring::EnemyPower.load()!=-1?std::vector<std::string>{"name","scale","count","power"}:std::vector<std::string>{"name","scale","count"}):
                tab==Tab::Items&&cloneTab?cloneAdvancedOpen?std::vector<std::string>{cloneFieldPicker?"clone-filter":"clone-mod"}:
                cloneEditorOpen?std::vector<std::string>{"clone-name","clone-power","clone-flavour","clone-icon","quantity"}:std::vector<std::string>{"filter"}:
                std::vector<std::string>{"filter","quantity"};
            if(recipeOpen&&!choicePicker&&!ItemPickerOpen())for(const auto& i:ingredients)ids.push_back("ingredient:"+i.path);
            auto it=std::find(ids.begin(),ids.end(),focus);
            Focus(it==ids.end()||++it==ids.end()?ids.front():*it);return;
        }
        auto* value=Field(focus);if(!value)return;
        if(busy&&focus!="filter"&&focus!="loot-filter"&&focus!="mesh-filter"&&focus!="choice-filter")return;
        caret=std::min(caret,value->size());
        if(control && key=='A'){selectAll=true;return;}
        if(key==36){caret=0;selectAll=false;return;}
        if(key==35){caret=value->size();selectAll=false;return;}
        if(key==37){caret=Previous(*value,caret);selectAll=false;return;}
        if(key==39){caret=Next(*value,caret);selectAll=false;return;}
        if(key==8||key==46) {
            if(selectAll){value->clear();caret=0;selectAll=false;}
            else if(key==8 && caret){const auto prev=Previous(*value,caret);value->erase(prev,caret-prev);caret=prev;}
            else if(key==46 && caret<value->size())value->erase(caret,Next(*value,caret)-caret);
            Edited();
        }
    }
    void Back() {
        focus.clear();selectAll=false;
        if(!itemFilterMenu.empty()){itemFilterMenu.clear();return;}
        if(actionReportOpen){actionReportOpen=false;return;}
        if(helpySettingsOpen){helpySettingsOpen=false;return;}
        if(helpyAboutOpen){helpyAboutOpen=false;return;}
        if(choicePicker){choicePicker=false;return;}
        if(ingredientPicker){ingredientPicker=false;return;}
        if(iconPicker){iconPicker=false;return;}
        if(coverageOpen){coverageOpen=false;return;}
        if(reportOpen){reportOpen=false;return;}
        if(cloneFieldOpen){cloneFieldOpen=false;if(itemDetailFieldMode){itemDetailFieldMode=false;itemDetailsOpen=true;}return;}
        if(itemDetailsOpen){itemDetailsOpen=false;itemDetailsPath.clear();itemDetailEdits.clear();return;}
        if(playerPicker){playerPicker=false;return;}
        if(cloneMeshFieldPicker){cloneMeshFieldPicker=false;return;}
        if(cloneModeOpen){cloneModeOpen=false;return;}
        if(cloneMeshPicker){cloneMeshPicker=false;return;}
        if(lootPicker){lootPicker=false;return;}
        if(cloneSourcePicker){cloneSourcePicker=false;return;}
        if(cloneAppearancePicker){cloneAppearancePicker=false;return;}
        if(cloneFieldPicker){cloneFieldPicker=false;cloneFilter.clear();cloneScroll=0;return;}
        if(cloneAdvancedOpen){cloneAdvancedOpen=false;return;}
        if(recipeOpen){recipeOpen=false;if(recipeDetailsMode){recipeDetailsMode=false;itemDetailsOpen=true;}return;}
        if(companionsOpen){companionsOpen=false;return;}
        if(cartOpen){cartOpen=false;return;}
        if(cloneEditorOpen){SaveDraft();cloneEditorOpen=false;return;}
        if(node){node.reset();drops.clear();return;}
        closeRequested=true;
    }
    void Page(int steps) {
        if(!itemFilterMenu.empty()||actionReportOpen||cloneModeOpen||cloneMeshFieldPicker||cloneFieldOpen)return;
        if(helpySettingsOpen||helpyAboutOpen)return;
        if(cartOpen)cartPage-=steps;
        else if(itemDetailsOpen)itemDetailsPage-=steps;
        else if(choicePicker)choicePage-=steps;
        else if(coverageOpen)coverageScroll-=steps;
        else if(cloneMeshPicker)meshScroll-=steps;
        else if(reportOpen)reportScroll-=steps;
        else if(playerPicker)playerScroll-=steps;
        else if(ItemPickerOpen())lootScroll-=steps;
        else if(cloneFieldOpen)return;
        else if(node)dropScroll-=steps;
        else if(recipeOpen)ingredientPage-=steps;
        else if(tab==Tab::Items&&cloneTab&&cloneAdvancedOpen)cloneScroll-=steps;
        else if(tab==Tab::Items&&cloneTab&&!cloneEditorOpen)draftPage-=steps;
        else if(tab==Tab::Items&&cloneTab)return;
        else scroll[static_cast<int>(tab)]-=steps;
        ClampScroll();
    }
    void Wheel(int steps) {
        // Paged catalogues have no hidden scroll. Only additional-loot overflow scrolls.
        if(node&&!ItemPickerOpen()&&!playerPicker&&!reportOpen&&!actionReportOpen&&!coverageOpen) {
            dropScroll-=steps;ClampScroll();
        }
    }
    std::optional<Command> TakeCommand() {auto out=std::move(pending);pending.reset();return out;}
    void Queue(Command value) {if(busy||pending){status="Another command is pending; wait for its result.";return;}pending=std::move(value);busy=true;status="Queued for the game thread.";}
    Command GiveCommand() const {
        RequireAuthority();Command out;out.kind=Command::Kind::Give;out.player=recipient;
        for(const auto& path:selection) {
            const auto* current=FindItem(path);
            if(!current||!current->available)throw std::runtime_error("A selected item is unavailable. Refresh, clear it, or select a loaded item.");
            const auto configured=grantQuantities.find(path);
            const int n=configured==grantQuantities.end()?Integer(quantity,1,10000,"Quantity"):
                Integer(configured->second,1,10000,"Item quantity");
            out.grants.push_back({path,n});
        }
        ValidateGrants(out.grants);return out;
    }
    Command QuickGiveCommand(const std::string& path) const {
        RequireAuthority();const auto* current=FindItem(path);
        if(!current||!current->available)throw std::runtime_error("This item is unavailable. Refresh the catalogue before granting it.");
        Command out;out.kind=Command::Kind::Give;out.player=recipient;out.grants.push_back({path,1});
        ValidateGrants(out.grants);return out;
    }
    Command SpawnCommand() const {
        RequireAuthority();if(!node || !FindNode(node->id))throw std::runtime_error("Select a current NPC, AI or resource first.");
        const auto* current=FindNode(node->id);
        if(!node->available||!current->available)throw std::runtime_error(current->detail.empty()?"This definition is browse-only until its class can be resolved.":current->detail);
        if(FindNode(node->id)->path!=node->path)throw std::runtime_error("The selected class changed; select it again before spawning.");
        Command out;out.kind=Command::Kind::Spawn;out.player=recipient;out.definition=node->id;out.classPath=node->path;
        out.resource=tab==Tab::Resources;out.npc=current->nodeKind=="NPC";out.name=name;out.scale=Number(scale,std::numeric_limits<double>::min(),std::numeric_limits<double>::max(),"Scale");
        out.count=out.npc?1:Integer(count,1,20,"Total count");out.effect=effect;
        out.permanent=Authoring::PermanentSpawn.load();
        if(out.npc) {
            const bool allowed=out.permanent?current->permanentAllowed:current->temporaryAllowed;
            const auto& reason=out.permanent?current->permanentReason:current->temporaryReason;
            if(!allowed)throw std::runtime_error(reason.empty()?"This NPC mode is unavailable.":reason);
            if(name.size()>256||name.find('\0')!=std::string::npos)throw std::runtime_error("NPC name is invalid.");
            out.count=1;out.effect=Effect::Inherit;
            out.durationSeconds=out.permanent?0:Integer(npcDuration,HelpyNodes::MinDuration,HelpyNodes::MaxDuration,"Lifetime");
            out.placementMode=npcGridPlacement?"Grid":"Radius";
            if(npcGridPlacement){out.gridX=Number(npcGridX,-100,100,"Grid X");out.gridY=Number(npcGridY,-100,100,"Grid Y");out.gridSize=Number(npcGridSize,10,1000,"Grid size");}
            else out.placementRadius=Number(npcRadius,0,10000,"Spawn radius");
            HelpyNodes::ValidateDuration(out.permanent,out.durationSeconds);return out;
        }
        out.placementMode=npcGridPlacement?"Grid":"Radius";
        if(npcGridPlacement){out.gridX=Number(npcGridX,-100,100,"Grid X");out.gridY=Number(npcGridY,-100,100,"Grid Y");out.gridSize=Number(npcGridSize,10,1000,"Grid size");}
        else out.placementRadius=Number(npcRadius,0,10000,"Spawn radius");
        if(!out.resource && Authoring::EnemyPower.load()!=-1)out.powerLevel=Integer(power,Authoring::MinPower,Authoring::MaxPower,"Power level");
        if(name.size()>256 || name.find('\0')!=std::string::npos)throw std::runtime_error("Name is invalid or too long.");
        if(drops.size()>MaxDrops)throw std::runtime_error("At most 16 additional loot entries are supported.");
        for(const auto& d:drops) {
            if(!FindItem(d.item))throw std::runtime_error("An additional loot item is no longer available.");
            Loot value{d.item,Integer(d.min,1,10000,"Minimum loot"),Integer(d.max,1,10000,"Maximum loot"),Number(d.chance,0,100,"Chance (0..100%)")};
            if(value.min>value.max)throw std::runtime_error("Minimum loot cannot exceed maximum loot.");
            out.loot.push_back(std::move(value));
        }return out;
    }
    void Activate(const std::string& action,const std::string& arg={}) {
        try {
            if(action=="close"){Back();return;}
            if(busy&&(action=="item-subtab"||action=="tab"||action=="permanent-asset"||action=="make-journal"||action=="make-recipe"||action.starts_with("clone-")||action.starts_with("draft-")||action.starts_with("appearance-")||action.starts_with("recipe-")||action.starts_with("journal-")))throw std::runtime_error("Wait for the pending item operation.");
            if(action=="focus"){if(busy&&arg!="filter"&&arg!="loot-filter"&&arg!="mesh-filter"&&arg!="choice-filter")throw std::runtime_error("Wait for the pending item operation.");Focus(arg);return;}
            focus.clear();selectAll=false;
            if(action=="rail") {
                if(arg=="items"){SaveDraft();cloneEditorOpen=false;tab=Tab::Items;cloneTab=false;favoritesTab=false;}
                else if(arg=="ai")tab=Tab::Enemies;
                else if(arg=="resources")tab=Tab::Resources;
                else if(arg=="clone"){SaveDraft();cloneEditorOpen=false;tab=Tab::Items;cloneTab=true;favoritesTab=false;}
                else if(arg=="settings"){helpySettingsOpen=true;selectedHotkey=helpyKey;}
                else if(arg=="about")helpyAboutOpen=true;
                scroll[static_cast<int>(tab)]=0;focus.clear();
            }
            else if(action=="tab" && !node && !lootPicker && !playerPicker)tab=static_cast<Tab>(Integer(arg,0,2,"Tab"));
            else if(action=="node-subtab") {const int sub=Integer(arg,0,2,"Tab");if(tab==Tab::Enemies)npcTab=sub;else if(tab==Tab::Resources)resourceTab=sub;scroll[static_cast<int>(tab)]=0;focus.clear();}
            else if(action=="npc-role-filter") {static constexpr std::array<const char*,5> roles{"All roles","Vendors","Quest givers","Dialogue","Lore"};auto at=std::find(roles.begin(),roles.end(),npcRole);npcRole=roles[(at==roles.end()?0:(std::distance(roles.begin(),at)+1)%roles.size())];scroll[1]=0;}
            else if(action=="node-favorite"){ToggleNodeFavorite(arg);ClampScroll();}
            else if(action=="other-resources"){includeOtherResources=!includeOtherResources;scroll[2]=0;}
            else if(action=="dismiss-npcs"){RequireAuthority();Command c;c.kind=Command::Kind::DismissNpcs;c.player=recipient;Queue(std::move(c));}
            else if(action=="item-subtab") {SaveDraft();cloneEditorOpen=false;cloneTab=arg=="clone";favoritesTab=arg=="favorites";selectedOnly=false;scroll[0]=0;focus.clear();}
            else if(action=="item-details") {
                if(!FindItem(arg))return;itemDetailsPath=arg;itemDetailsOpen=true;itemDetailsLoaded=false;itemDetailsTab=0;itemDetailsPage=0;
                itemDetailFields.clear();itemDetailRecipes.clear();itemDetailJournals.clear();itemDetailEdits.clear();itemDetailFieldMode=false;recipeDetailsMode=false;
                Command c;c.kind=Command::Kind::InspectDetails;c.definition=arg;Queue(std::move(c));
            }
            else if(action=="item-details-tab"){itemDetailsTab=Integer(arg,0,2,"Details tab");itemDetailsPage=0;}
            else if(action=="item-detail-field") {
                const auto it=std::find_if(itemDetailFields.begin(),itemDetailFields.end(),[&](const auto& f){return f.name==arg;});
                if(it==itemDetailFields.end()||!it->editable)return;
                cloneField=arg;cloneKind=it->kind.empty()?HelpyPropertyValue::Kind(it->type):it->kind;
                const auto encoded=itemDetailEdits.contains(arg)?itemDetailEdits.at(arg):it->hasValue?it->value:std::string{};
                cloneValue=encoded.empty()?std::string{}:HelpyPropertyValue::Decode(encoded,cloneKind);
                if(cloneKind=="Boolean"&&cloneValue.empty())cloneValue="false";
                itemDetailFieldMode=true;itemDetailsOpen=false;cloneFieldOpen=true;Focus("clone-value");
            }
            else if(action=="item-overrides-export")Queue(ItemOverrideExportCommand());
            else if(action=="item-recipe-new") {
                recipeTargetPath=itemDetailsPath;recipeTargetName=FindItem(itemDetailsPath)?FindItem(itemDetailsPath)->name:itemDetailsPath;
                recipeDetailsMode=true;makeRecipe=true;unlockRecipe=false;recipeStation={};ingredients.clear();recipeCategory="Helpy";recipeOutput="1";
                itemDetailsOpen=false;recipeOpen=true;
            }
            else if(action=="item-recipe-edit") {
                const auto at=static_cast<std::size_t>(Integer(arg,0,511,"Recipe"));if(at>=itemDetailRecipes.size())return;const auto& recipe=itemDetailRecipes[at];
                recipeTargetPath=itemDetailsPath;recipeTargetName=FindItem(itemDetailsPath)?FindItem(itemDetailsPath)->name:itemDetailsPath;
                recipeDetailsMode=true;makeRecipe=true;unlockRecipe=false;recipeStation=recipe.station;ingredients=recipe.ingredients;recipeCategory="Helpy";recipeOutput=recipe.output;
                itemDetailsOpen=false;recipeOpen=true;
            }
            else if(action=="item-quick-give")Queue(QuickGiveCommand(arg));
            else if(action=="favorite"){ToggleFavorite(arg);}
            else if(action=="item-filter-type"||action=="item-filter-source") {filterForPicker=ItemPickerOpen();itemFilterMenu=action=="item-filter-type"?"type":"source";}
            else if(action=="item-filter-chip") {itemType=arg;scroll[0]=0;}
            else if(action=="item-filter-value") {
                const auto& choices=itemFilterMenu=="type"?HelpyItemFilters::Types():HelpyItemFilters::Sources();
                if(std::find(choices.begin(),choices.end(),arg)==choices.end())throw std::runtime_error("Invalid item filter");
                auto& field=itemFilterMenu=="type"?(filterForPicker?pickerType:itemType):(filterForPicker?pickerSource:itemSource);
                field=arg;itemFilterMenu.clear();if(filterForPicker)lootScroll=0;else scroll[0]=0;
            }
            else if(action=="picker-favorites"){pickerFavoritesOnly=!pickerFavoritesOnly;lootScroll=0;}
            else if(action=="permanent-spawn")Authoring::PermanentSpawn=!Authoring::PermanentSpawn.load();
            else if(action=="npc-placement-mode")npcGridPlacement=!npcGridPlacement;
            else if(action=="power-mode") {
                if(Authoring::EnemyPower.load()==-1){observedPower=Integer(power,1,100,"Power level");Authoring::EnemyPower=observedPower;}
                else {Authoring::EnemyPower=-1;observedPower=-1;}
            }
            else if(action=="permanent-asset") {Authoring::PermanentAsset=!Authoring::PermanentAsset.load();cloneAcknowledged=false;}
            else if(action=="coverage"){coverageOpen=true;coverageScroll=0;}
            else if(action=="helpy-settings"){helpySettingsOpen=true;selectedHotkey=helpyKey;}
            else if(action=="helpy-about"){helpyAboutOpen=true;}
            else if(action=="hotkey-next"){const auto& keys=HelpyHotkeys::Choices();std::size_t at=0;while(at<keys.size()&&keys[at].name!=selectedHotkey)++at;selectedHotkey=keys[(at+1)%keys.size()].name;}
            else if(action=="hotkey-prev"){const auto& keys=HelpyHotkeys::Choices();std::size_t at=0;while(at<keys.size()&&keys[at].name!=selectedHotkey)++at;selectedHotkey=keys[(at+keys.size()-1)%keys.size()].name;}
            else if(action=="hotkey-save"){Command c;c.kind=Command::Kind::SetHotkey;c.name=selectedHotkey;Queue(std::move(c));}
            else if(action=="update-rsdw"){Command c;c.kind=Command::Kind::UpdateReference;Queue(std::move(c));}
            else if(action=="draft-add")NewDraft();
            else if(action=="draft-open")OpenDraft(Integer(arg,0,31,"Draft"));
            else if(action=="draft-remove") {SaveDraft();const int at=Integer(arg,0,31,"Draft");if(static_cast<std::size_t>(at)<drafts.size()){drafts.erase(drafts.begin()+at);activeDraft=-1;}status="Draft removed only; installed files and created items are unchanged.";}
            else if(action=="clone-advanced"){cloneAdvancedOpen=true;cloneFieldPicker=false;cloneRawFields=true;cloneFilter.clear();cloneScroll=0;}
            else if(action=="clone-scope"){cloneRawFields=arg=="raw";cloneFieldPicker=false;cloneFilter.clear();cloneScroll=0;}
            else if(action=="clone-add-field"){cloneFieldPicker=true;cloneFilter.clear();cloneScroll=0;}
            else if(action=="clone-quick-field") {
                const auto found=std::find_if(cloneFields.begin(),cloneFields.end(),[&](const auto& field){
                    if(!field.editable||field.scope.empty())return false;
                    const auto name=Lower(field.nativeName.empty()?field.name:field.nativeName);
                    if(arg=="Defense")return name=="defense"||name=="defence"||name=="armor"||name=="armour"||name.find("defense")!=name.npos||name.find("armour")!=name.npos;
                    if(arg!="PackDrops")return field.nativeName==arg||field.name==arg;
                    return field.type.find("TArray")!=std::string::npos&&(name.find("drop")!=name.npos||name.find("loot")!=name.npos||name.find("grant")!=name.npos||name.find("content")!=name.npos);
                });
                if(found==cloneFields.end())throw std::runtime_error(arg+" is not available on this source item. Use All properties for its reflected fields.");
                cloneField=found->name;cloneKind=found->kind.empty()?HelpyPropertyValue::Kind(found->type):found->kind;
                const auto encoded=cloneEdits.contains(found->name)?cloneEdits.at(found->name):found->hasValue?found->value:std::string{};
                cloneValue=encoded.empty()?std::string{}:HelpyPropertyValue::Decode(encoded,cloneKind);
                if(cloneKind=="Boolean"&&cloneValue.empty())cloneValue="false";cloneFieldOpen=true;Focus("clone-value");
            }
            else if(action=="clone-icon-mode"){iconMode=static_cast<IconMode>((static_cast<int>(iconMode)+1)%4);cloneEdits.erase("Icon");}
            else if(action=="clone-icon-picker"){iconPicker=true;pickerFavoritesOnly=false;lootFilter.clear();lootScroll=0;}
            else if(action=="icon-item"){const auto* e=FindItem(arg);if(e&&!e->icon.empty()){cloneIcon=e->icon;iconMode=IconMode::Override;cloneEdits.erase("Icon");iconPicker=false;}}
            else if(action=="clone-companions"||action=="clone-journal")companionsOpen=true;
            else if(action=="clone-recipe")recipeOpen=true;
            else if(action=="make-journal"){makeJournal=!makeJournal;if(makeJournal&&!journalTextEdited)journalText=cloneFlavour;}
            else if(action=="make-recipe")makeRecipe=!makeRecipe;
            else if(action=="recipe-edit")recipeOpen=true;
            else if(action=="recipe-unlock")unlockRecipe=!unlockRecipe;
            else if(action=="journal-reset"){journalTextEdited=false;journalText=cloneFlavour;}
            else if(action=="choose-journal"||action=="choose-station"){choiceKind=action=="choose-journal"?"journal":"station";choicePicker=true;choiceFilter.clear();choicePage=0;}
            else if(action=="choose-target"){const auto& list=choiceKind=="journal"?journalChoices:stationChoices;for(const auto& v:list)if(v.id==arg){if(choiceKind=="journal")journalTarget=v;else recipeStation=v;break;}choicePicker=false;}
            else if(action=="recipe-add"){ingredientPicker=true;pickerFavoritesOnly=false;lootFilter.clear();lootScroll=0;}
            else if(action=="ingredient-item"){const auto* e=FindItem(arg);if(!e||!e->available)return;if(ingredients.size()>=16)throw std::runtime_error("At most 16 ingredient rows.");if(std::none_of(ingredients.begin(),ingredients.end(),[&](const auto& v){return v.path==arg;}))ingredients.push_back({arg,e->name,e->icon,"1"});ingredientPicker=false;}
            else if(action=="recipe-remove")std::erase_if(ingredients,[&](const auto& i){return i.path==arg;});
            else if(action=="recipe-export")Queue(RecipeExportCommand());
            else if(action=="clone-new-id"){cloneId=ClonePresentation::NewId(cloneModTag);cloneCreated=false;cloneAcknowledged=false;}
            else if(action=="appearance-mode")cloneModeOpen=true;
            else if(action=="appearance-set"){appearanceMode=static_cast<Appearance>(Integer(arg,0,2,"Appearance"));cloneModeOpen=false;cloneAcknowledged=false;}
            else if(action=="clone-appearance"){cloneAppearancePicker=true;pickerFavoritesOnly=false;lootFilter.clear();lootScroll=0;Focus("loot-filter");}
            else if(action=="appearance-clone") {
                const auto* item=FindItem(arg);if(!item||!item->cooked||!item->cloneEligible)return;
                if(busy)throw std::runtime_error("Finish the pending command first.");
                cloneAppearanceReady=false;cloneAppearance=arg;cloneAppearanceInfo=*item;cloneAppearancePicker=false;appearanceMode=Appearance::Copy;cloneAcknowledged=false;
                Queue(InspectCloneCommand(arg,true));
            }
            else if(action=="mesh-slots")cloneMeshFieldPicker=true;
            else if(action=="mesh-slot"){cloneMeshField=arg;cloneMesh.clear();cloneMeshFieldPicker=false;}
            else if(action=="mesh-picker"){cloneMeshPicker=true;meshFilter.clear();meshScroll=0;Focus("mesh-filter");}
            else if(action=="mesh-select"){cloneMesh=arg;cloneMeshPicker=false;cloneAcknowledged=false;}
            else if(action=="clone-give")cloneGive=!cloneGive;
            else if(action=="clone-ack")cloneAcknowledged=!cloneAcknowledged;
            else if(action=="clone-source") {cloneSourcePicker=true;pickerFavoritesOnly=false;lootFilter.clear();lootScroll=0;Focus("loot-filter");}
            else if(action=="source-clone") {
                const auto* item=FindItem(arg);if(!item||!item->cloneEligible)return;
                if(busy)throw std::runtime_error("Finish the pending command first.");
                auto c=InspectCloneCommand(arg);cloneReady=false;cloneFields.clear();cloneEdits.clear();
                cloneSource=arg;cloneTitle=item->name;cloneSourceInfo=*item;cloneSourcePicker=false;
                cloneId.clear();cloneAcknowledged=false;cloneSoftDelete.clear();cloneCreated=false;
                cloneAppearance.clear();cloneAppearanceInfo={};cloneAppearanceReady=false;appearanceMode=Appearance::Source;
                cloneMesh.clear();cloneMeshField.clear();cloneName.clear();cloneIcon.clear();Queue(std::move(c));
            }
            else if(action=="inspect-clone")Queue(InspectCloneCommand(cloneSource));
            else if(action=="clone-field") {
                const auto it=std::find_if(cloneFields.begin(),cloneFields.end(),[&](const auto& f){return f.name==arg;});
                if(it==cloneFields.end()||!it->editable)return;
                cloneField=arg;cloneKind=it->kind.empty()?HelpyPropertyValue::Kind(it->type):it->kind;
                const auto encoded=cloneEdits.contains(arg)?cloneEdits.at(arg):it->hasValue?it->value:std::string{};
                cloneValue=encoded.empty()?std::string{}:HelpyPropertyValue::Decode(encoded,cloneKind);
                if(cloneKind=="Boolean"&&cloneValue.empty())cloneValue="false";
                cloneFieldOpen=true;Focus("clone-value");
            }
            else if(action=="clone-apply") {
                const auto* active=CurrentCloneField();
                if(!active||!active->editable||!cloneFieldOpen)throw std::runtime_error("Select a supported property before applying an override.");
                auto& edits=itemDetailFieldMode?itemDetailEdits:cloneEdits;
                edits[cloneField]=HelpyPropertyValue::Encode(cloneValue,cloneKind);
                if(itemDetailFieldMode){cloneFieldOpen=false;itemDetailFieldMode=false;itemDetailsOpen=true;return;}
                if(cloneField=="FlavourText"){cloneFlavour=cloneValue;if(!journalTextEdited)journalText=cloneFlavour;}
                if(cloneField=="PowerLevel")clonePower=cloneValue;
                if(cloneField=="Name"){cloneName=cloneValue;cloneEdits.erase("Name");}
                if(cloneField=="Icon"){cloneIcon=cloneValue;iconMode=IconMode::Override;cloneEdits.erase("Icon");}
                cloneFieldOpen=false;cloneFieldPicker=false;cloneFilter.clear();cloneScroll=0;
            }
            else if(action=="clone-bool"){cloneValue=cloneValue=="true"?"false":"true";}
            else if(action=="clone-inherit") {
                auto& edits=itemDetailFieldMode?itemDetailEdits:cloneEdits;edits.erase(cloneField);
                if(itemDetailFieldMode){cloneFieldOpen=false;itemDetailFieldMode=false;itemDetailsOpen=true;return;}
                if(const auto* f=CurrentCloneField();f&&f->hasValue){if(cloneField=="PowerLevel")clonePower=f->value;if(cloneField=="FlavourText"){cloneFlavour=HelpyPropertyValue::Decode(f->value,"Text");if(!journalTextEdited)journalText=cloneFlavour;}}
                if(cloneField=="Name")cloneName=cloneTitle.size()<=250?cloneTitle+" Clone":std::string{};
                if(cloneField=="Icon"){cloneIcon.clear();iconMode=IconMode::Auto;}
                cloneFieldOpen=false;
            }
            else if(action=="create-clone")Queue(CloneCommand());
            else if(action=="toggle-item") {
                if(!FindItem(arg))return;
                if(selection.contains(arg)){selection.erase(arg);grantQuantities.erase(arg);}
                else if(selection.size()<MaxSelection){selection.insert(arg);grantQuantities.try_emplace(arg,"1");}
                else status="At most 64 different items can be selected per grant.";
            }
            else if(action=="report")reportOpen=true;
            else if(action=="clear"){selection.clear();grantQuantities.clear();cartPage=0;}
            else if(action=="cart-open"){cartOpen=true;cartPage=0;}
            else if(action=="cart-close")cartOpen=false;
            else if(action=="cart-remove") {selection.erase(arg);grantQuantities.erase(arg);cartPage=std::min(cartPage,std::max(0,static_cast<int>((selection.size()+6)/7)-1));}
            else if(action=="selected-only"){selectedOnly=!selectedOnly;scroll[0]=0;}
            else if(action=="player-picker"){playerPicker=true;playerScroll=0;Command c;c.kind=Command::Kind::Players;Queue(c);}
            else if(action=="player"){recipient=arg;if(const auto* selected=Recipient())recipientName=selected->name;playerPicker=false;}
            else if(action=="refresh"){Command c;c.kind=Command::Kind::Refresh;Queue(c);}
            else if(action=="index"){Command c;c.kind=Command::Kind::Index;Queue(c);indexing=busy;}
            else if(action=="cancel"&&indexing){Command c;c.kind=Command::Kind::Cancel;pending=c;}
            else if(action=="give")Queue(GiveCommand());
            else if(action=="node") {
                if(const auto* e=FindNode(arg)){node=*e;name="";scale="1";count="1";effect=Effect::Inherit;drops.clear();dropScroll=0;npcGridPlacement=false;npcRadius="500";npcGridX="0";npcGridY="0";npcGridSize="100";}
                else status="This favorite is unavailable; refresh or restore its source mod. It was not spawned.";
            }
            else if(action=="effect")effect=static_cast<Effect>((static_cast<int>(effect)+1)%3);
            else if(action=="loot-picker"){lootPicker=true;pickerFavoritesOnly=false;lootFilter.clear();lootScroll=0;Focus("loot-filter");}
            else if(action=="add-loot") {
                if(drops.size()>=MaxDrops)throw std::runtime_error("At most 16 additional loot entries are supported.");
                const auto* e=FindItem(arg);if(!e)return;
                if(std::none_of(drops.begin(),drops.end(),[&](const auto& d){return d.item==arg;}))drops.push_back({e->path,e->name,"1","1","100",e->icon});
                lootPicker=false;
            }
            else if(action=="remove-loot")std::erase_if(drops,[&](const auto& d){return d.item==arg;});
            else if(action=="spawn")Queue(SpawnCommand());
            else if(action=="scroll-up"||action=="page-prev")Page(1);
            else if(action=="scroll-down"||action=="page-next")Page(-1);
            else if(action=="scroll-start"){if(choicePicker)choicePage=0;else if(recipeOpen)ingredientPage=0;else if(tab==Tab::Items&&cloneTab&&!cloneEditorOpen)draftPage=0;else if(coverageOpen)coverageScroll=0;else if(cloneMeshPicker)meshScroll=0;else if(reportOpen)reportScroll=0;else if(playerPicker)playerScroll=0;else if(ItemPickerOpen())lootScroll=0;else if(node)dropScroll=0;else if(tab==Tab::Items&&cloneTab)cloneScroll=0;else scroll[static_cast<int>(tab)]=0;}
            else if(action=="scroll-end"){if(choicePicker)choicePage=2147483647;else if(recipeOpen)ingredientPage=2147483647;else if(tab==Tab::Items&&cloneTab&&!cloneEditorOpen)draftPage=2147483647;else if(coverageOpen)coverageScroll=2147483647;else if(cloneMeshPicker)meshScroll=2147483647;else if(reportOpen)reportScroll=2147483647;else if(playerPicker)playerScroll=2147483647;else if(ItemPickerOpen())lootScroll=2147483647;else if(node)dropScroll=2147483647;else if(tab==Tab::Items&&cloneTab)cloneScroll=2147483647;else scroll[static_cast<int>(tab)]=2147483647;}
            ClampScroll();
        }catch(const std::exception& e){status=e.what();if(action=="spawn"||action=="create-clone")ShowActionResult("Action not submitted",status,false);}
    }
    void Click(const Frame& frame,float x,float y) {
        for(auto it=frame.hits.rbegin();it!=frame.hits.rend();++it)if(it->box.Contains(x,y)){Activate(it->action,it->arg);return;}
        focus.clear();
    }
    void RightClick(const Frame& frame,float x,float y) {
        for(auto it=frame.rightHits.rbegin();it!=frame.rightHits.rend();++it)if(it->box.Contains(x,y)){Activate(it->action,it->arg);return;}
    }
    void ShowActionResult(std::string title,std::string message,bool success) {
        actionTitle=std::move(title);actionMessage=std::move(message);actionSuccess=success;
        actionReportOpen=true;focus.clear();selectAll=false;
    }
    Frame Render(float mouseX=-1,float mouseY=-1) {
        const int currentPower=Authoring::EnemyPower.load();
        if(currentPower!=observedPower&&focus!="power"){observedPower=currentPower;if(currentPower!=-1)power=std::to_string(currentPower);}
        const float shellMouseX=mouseX/HorizontalFit;
        const float navMouseX=shellMouseX;
        mouseX=shellMouseX-NavRailWidth;
        ClampScroll();Frame f;using Tint=std::array<float,4>;
        const Tint ash{.055f,.059f,.063f,1},gold{.77f,.79f,.81f,1},muted{.57f,.59f,.61f,1},ink{.91f,.92f,.93f,1};
        std::string hoveredPath;
        const auto rect=[&](Rect r,Tint c){f.draws.push_back({Draw::Kind::Rectangle,r,{},c,18,false,{}});};
        // Canvas primitives cross the reflection boundary once per draw. The
        // former faux-rounded rectangle emitted three overlapping calls for
        // every surface (six for a bordered panel). A crisp single-fill design
        // is both visually consistent and dramatically cheaper in PostRender.
        const auto soft=[&](Rect r,Tint c,float=3.f){rect(r,c);};
        const auto panel=[&](Rect r,Tint fill){rect(r,fill);};
        const auto text=[&](std::string s,float x,float y,float size=18,Tint c=Tint{.94f,.9f,.82f,1},bool centre=false){
            f.draws.push_back({Draw::Kind::Text,{x,y,0,0},std::move(s),c,size,centre,{}});
        };
        const auto wrapped=[&](std::string value,float x,float y,size_t width,int lines,float font=17,bool centre=false){
            width=std::max<size_t>(3,static_cast<size_t>(width*HorizontalFit));
            for(int line=0;line<lines&&!value.empty();++line){
                size_t end=std::min(width,value.size());
                if(end<value.size()){
                    const auto space=value.rfind(' ',end);if(space!=std::string::npos&&space>width/3)end=space;
                    while(end&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;
                }
                text(line==lines-1?Shorten(value,width):value.substr(0,end),x,y+line*(font+4),font,ink,centre);
                value.erase(0,end);while(!value.empty()&&value.front()==' ')value.erase(0,1);
            }
        };
        const auto button=[&](Rect r,std::string label,std::string action,std::string arg="",bool enabled=true,bool active=false,float left=12.f){
            const bool hover=enabled&&r.Contains(mouseX,mouseY);
            soft(r,active?Tint{.20f,.22f,.24f,1}:hover?Tint{.13f,.15f,.17f,1}:Tint{.085f,.09f,.095f,1});
            text(Shorten(label,static_cast<size_t>(std::max(3.f,((r.w-left-6)*HorizontalFit)/8.2f))),r.x+left,r.y+(r.h-18)/2,18,enabled?ink:muted);
            if(enabled)f.hits.push_back({r,std::move(action),std::move(arg)});
        };
        const auto field=[&](Rect r,const std::string& id,std::string placeholder=""){
            auto* value=Field(id);const bool active=focus==id;
            soft(r,active?Tint{.22f,.25f,.28f,1}:Tint{.09f,.10f,.11f,1},2);
            std::string shown=value?*value:std::string{};
            if(id=="clone-value"&&r.h>90&&(cloneKind=="Text"||cloneKind=="JSON")) {
                const std::size_t columns=static_cast<std::size_t>(std::max(8.f,(r.w*HorizontalFit-28)/10.f));
                const std::size_t visible=static_cast<std::size_t>(std::max(1.f,(r.h-18)/23));
                const auto position=std::min(caret,shown.size());
                if(active)shown.insert(position,"|");
                std::vector<std::string> lines(1);std::size_t column=0,caretLine=0;
                for(std::size_t at=0;at<shown.size();) {
                    if(column>=columns){lines.emplace_back();column=0;}
                    if(active&&at==position)caretLine=lines.size()-1;
                    if(shown[at]=='\n'){lines.emplace_back();column=0;++at;continue;}
                    if(shown[at]=='\r'){++at;continue;}
                    const auto end=Next(shown,at);
                    lines.back()+=shown[at]=='\t'?std::string(" "):shown.substr(at,end-at);
                    ++column;at=end;
                }
                const auto start=active&&caretLine>=visible?caretLine-visible+1:0;
                for(std::size_t line=0;line<visible&&start+line<lines.size();++line)
                    text(lines[start+line],r.x+10,r.y+8+23*static_cast<float>(line),18,active?ink:muted);
                f.hits.push_back({r,"focus",id});return;
            }
            const bool numeric=id=="npc-duration"||id=="npc-radius"||id=="npc-grid-x"||id=="npc-grid-y"||id=="npc-grid-size"||id=="clone-power"||id=="recipe-output"||id.starts_with("ingredient:")||(id=="clone-value"&&(cloneKind=="Number"||cloneKind=="Integer"||cloneKind=="Unsigned"))||id=="quantity"||id=="power"||id=="count"||id=="scale"
                ||id.starts_with("min:")||id.starts_with("max:")||id.starts_with("chance:");
            const auto cap=static_cast<size_t>(std::max(3.f,(r.w*HorizontalFit-(numeric?12:22))/(numeric?8.f:9.f)));
            if(active&&value){auto pos=std::min(caret,shown.size());shown.insert(pos,"|");if(pos>cap-2){auto begin=pos-(cap-2);while(begin&&((static_cast<unsigned char>(shown[begin])&0xc0)==0x80))--begin;shown.erase(0,begin);}}
            if(shown.empty())shown=std::move(placeholder);
            std::string preview;preview.reserve(shown.size());
            for(const char c:shown){if(c=='\n')preview+=" \xC2\xB7 ";else if(c!='\r')preview+=c=='\t'?' ':c;}
            shown=std::move(preview);
            text(Shorten(shown,cap),numeric?r.x+r.w/2:r.x+10,r.y+(r.h-18)/2,18,active?ink:muted,numeric);
            f.hits.push_back({r,"focus",id});
        };
        const auto icon=[&](const std::string& path,Rect r){
            if(!path.empty())f.draws.push_back({Draw::Kind::Icon,r,path,{1,1,1,1},18,false,{}});
            else {soft(r,{.09f,.10f,.11f,1});text("?",r.x+r.w/2,r.y+r.h*.28f,26,muted,true);}
        };
        const auto powerLabel=[](double value){
            if(!std::isfinite(value)||value<0)return std::string("--");
            auto n=std::to_string(value);while(n.size()>1&&n.back()=='0')n.pop_back();if(n.back()=='.')n.pop_back();
            return n;
        };
        const auto badge=[&](const std::string&,Rect r,const std::string& fallback,float alpha=1.f){
            // UI chrome is DLL-owned vector geometry. Only catalogue item artwork
            // resolves live game textures; shell badges never require a cooked pak.
            (void)alpha;
            text(Shorten(fallback,3),r.x+r.w/2,r.y+r.h*.22f,std::max(10.f,r.h*.42f),ink,true);
        };
        const auto boldText=[&](const std::string& value,float x,float y,float size,Tint color){text(value,x,y,size,color);};
        const auto powerMark=[&](double value,float x,float y,float size=15.f){boldText("PL",x,y,size,{.88f,.66f,.25f,1});boldText(" "+powerLabel(value),x+20,y,size,{1,1,1,1});};
        const auto itemTypeLabel=[&](const Entry& e){
            for(const auto& type:HelpyItemFilters::Types())
                if(type!="All types"&&type!="Other"&&HelpyItemFilters::Type(type,e.itemTags,e.assetClass,e.appearanceGroup,e.consumable,e.quest))return type;
            return std::string("Item");
        };
        const auto iconButton=[&](Rect r,const std::string& texture,const std::string& fallback,const std::string& tooltip,const std::string& action,bool enabled=true){
            const bool hover=enabled&&r.Contains(mouseX,mouseY);soft(r,hover?Tint{.16f,.18f,.20f,1}:Tint{.085f,.09f,.095f,1},2);
            badge(texture,{r.x+6,r.y+6,r.w-12,r.h-12},fallback,enabled?1.f:.35f);if(enabled)f.hits.push_back({r,action,{}});if(hover)hoveredPath=tooltip;
        };
        const auto placard=[&](Rect r,const Entry& e,const std::string& action,const std::string& arg,bool selected=false,bool allowFavorite=true,bool allowDetails=false){
            const bool hover=r.Contains(mouseX,mouseY);
            const Tint masterwork{1.f,.73f,.12f,1};
            const Tint edge=e.masterwork?masterwork:selected?Tint{.20f,.22f,.24f,1}:hover?Tint{.12f,.13f,.14f,1}:Tint{.075f,.08f,.085f,1};
            soft(r,edge);
            const bool compact=r.h<100;
            if(compact) {
                // Cards disappear into the ash workspace until hovered. This
                // preserves the fixed 3x4 geometry without a bright tile wall.
                const Tint card=ash,cardHover{.085f,.095f,.105f,1},cardInk=ink,cardMuted=muted;
                if(selected){soft(r,{.88f,.72f,.18f,1});soft({r.x+2,r.y+2,r.w-4,r.h-4},{.075f,.071f,.055f,1});}
                else soft(r,hover?cardHover:card);
                icon(e.icon,{r.x+10,r.y+10,54,54});
                const auto name=e.name.empty()?std::string("Choose item"):e.name;
                text(Shorten(name,28),r.x+74,r.y+10,16,e.available?cardInk:cardMuted);
                const auto type=itemTypeLabel(e);
                text(Shorten(type,28),r.x+74,r.y+35,14,cardMuted);
                const bool hasPower=e.available&&std::isfinite(e.power)&&e.power>=0;
                if(hasPower)text("Power Level: "+powerLabel(e.power),r.x+74,r.y+58,14,e.masterwork?Tint{.62f,.38f,.02f,1}:cardMuted);
                if(!action.empty()&&e.available)f.hits.push_back({r,action,arg});
                if(allowDetails&&e.available)f.rightHits.push_back({r,"item-details",e.path});
                if(selected)text("+",r.x+48,r.y+60,19,{.55f,.38f,.02f,1});
                if(!e.path.empty()) {
                    if(allowFavorite) {
                        const Rect hit{r.x+r.w-28,r.y+1,27,27};const bool favorite=IsFavorite(e.path),overStar=hit.Contains(mouseX,mouseY);
                        text(favorite?"*":"+",hit.x+hit.w/2,hit.y+3,17,favorite?Tint{.55f,.38f,.02f,1}:cardMuted,true);
                        f.hits.push_back({hit,"favorite",e.path});
                        if(overStar)hoveredPath=favorite?"Remove from Favorites":"Add to Favorites";
                        else if(hover)hoveredPath=e.available?e.path:e.detail;
                    }else if(hover)hoveredPath=e.path;
                }
                return;
            }
            // Placard chrome has fixed semantic corners: source at upper-left,
            // Favorite at upper-right, and item traits along the lower-right.
            // The item artwork and label retain the full centered content area.
            const float centre=r.x+r.w/2;
            const float image=std::min(68.f,r.h-62.f);
            icon(e.icon,{centre-image/2,r.y+8,image,image});
            wrapped(e.name.empty()?"Choose an item":e.name,centre,r.y+image+12,static_cast<size_t>((r.w-18)/8.1f),2,16,true);
            if(e.available&&std::isfinite(e.power)&&e.power>=0)powerMark(e.power,r.x+7,r.y+r.h-20,15);
            else text("Unavailable",r.x+8,r.y+r.h-18,14,muted);
            if(!action.empty()&&e.available)f.hits.push_back({r,action,arg});
            if(allowDetails&&e.available)f.rightHits.push_back({r,"item-details",e.path});
            if(selected)text("+",r.x+34,r.y+7,21);
            if(!e.path.empty()) {
                const auto origin=e.runtimeClone||e.runeSchemaManaged?QuickDecorations::Origin::RuneSchema:e.declaredModded?QuickDecorations::Origin::Modded:QuickDecorations::ItemOrigin(e.path,e.cooked,false);
                badge(QuickDecorations::Texture(origin),{r.x+5,r.y+5,24,24},QuickDecorations::Label(origin));
                const bool extraMod=(e.runtimeClone||e.runeSchemaManaged)&&e.cooked&&(e.declaredModded||e.declaredCooked||QuickDecorations::ModPath(e.path));
                float traitX=r.x+r.w-29;
                if(extraMod){badge(QuickDecorations::ModdedBadge,{traitX,r.y+r.h-29,24,24},"MOD");traitX-=28;}
                if(e.consumable){badge(e.categoryIcon,{traitX,r.y+r.h-29,24,24},"C");traitX-=28;}
                if(e.quest)badge(e.consumable?std::string{}:e.categoryIcon,{traitX,r.y+r.h-29,24,24},"Q");
                if(allowFavorite) {
                const float hitWidth=QuickDecorations::FavoriteHitSize/HorizontalFit;
                const Rect hit{r.x+r.w-hitWidth-2,r.y+2,hitWidth,36};
                const bool favorite=IsFavorite(e.path),overStar=hit.Contains(mouseX,mouseY);
                badge(QuickDecorations::FavoriteBadge,{hit.x+hit.w/2-12,hit.y+6,24,24},"*",favorite?1.f:overStar?.8f:.38f);
                f.hits.push_back({hit,"favorite",e.path}); // higher hit priority than card selection
                if(overStar)hoveredPath=favorite?"Remove from Favorites":"Add to Favorites";
                else if(hover)hoveredPath=e.available?e.path:e.detail;
                }else if(hover)hoveredPath=e.path;
            }
        };
        const auto pager=[&](float y,int page,std::size_t count,int pageSize){
            const int last=QuickDecorations::LastPage(count,pageSize);
            button({24,y,106,28},"Previous","page-prev","",page>0);
            button({142,y,50,28},"<<","scroll-start","",page>0,false,12);
            button({676,y,50,28},">>","scroll-end","",page<last,false,12);
            button({738,y,106,28},"Next","page-next","",page<last);
            const auto first=count?static_cast<std::size_t>(page)*pageSize+1:0;
            const auto end=std::min(count,static_cast<std::size_t>(page+1)*pageSize);
            text("Page "+std::to_string(page+1)+" / "+std::to_string(last+1)+"  |  "+std::to_string(first)+"-"+std::to_string(end)+" of "+std::to_string(count),390,y+5,16,muted,true);
        };
        const auto rail=[&](float x,float top,float bottom,int current,int total,int visible){
            button({x,top,28,28},"^","scroll-up","",current>0,false,8);
            button({x,bottom-28,28,28},"v","scroll-down","",current+visible<total,false,8);
            if(total<=visible)return;
            const float track=bottom-top-64,thumb=std::max(18.f,track*static_cast<float>(visible)/static_cast<float>(total));
            rect({x+10,top+32,8,track},{.035f,.04f,.04f,1});
            const float fraction=static_cast<float>(current)/static_cast<float>(std::max(1,total-visible));
            soft({x+9,top+32+(track-thumb)*fraction,10,thumb},gold,2);
        };
        const auto overlay=[&](const std::string& title){
            f.hits.clear();f.rightHits.clear();hoveredPath.clear();rect({0,0,ContentWidth,Height},{0,0,0,.84f});panel({16,28,748,664},ash);
            text(title,36,47,23,{.84f,.68f,.36f,1});iconButton({710,38,38,38},QuickDecorations::CloseBadge,"X","Back / close","close");
        };
        panel({0,0,ContentWidth,Height},ash);
        text("HELPY",20,20,22,ink);
        text("RuneSchema",102,24,15,muted);
        button({552,12,82,34},"Refresh","refresh","",!busy&&!indexing);
        if(indexing)button({642,12,112,34},"Stop scan","cancel");
        else if(advancedRuntime)button({642,12,112,34},"Full scan","index","",!busy);
        else button({642,12,112,34},"Full scan","index","",false);
        button({764,12,82,34},"Close","close");
        const char* names[]{"ITEMS","NPCs & AI","RESOURCES"};
        text(names[static_cast<int>(tab)],20,68,26,gold);
        if(tab==Tab::Items) {
            field({20,108,410,38},"filter","Search items by name or path...");
            button({442,108,160,38},itemSource,"item-filter-source");
            button({614,108,146,38},"To: "+(Recipient()?Recipient()->name:"Select player"),"player-picker");
        }else {
            field({20,108,472,38},"filter","Search name, variant or path...");
            button({504,108,254,38},"To: "+(Recipient()?Recipient()->name:"Select player"),"player-picker");
        }
        const int t=static_cast<int>(tab);const auto& matches=Filtered(t);const auto& entries=BrowserEntries(t);
        if(t!=0){const char* labelsNpc[]{"NPCS","AI / ENEMIES","FAVORITES"};const char* labelsResource[]{"TREES","STONE & ORE","FAVORITES"};for(int i=0;i<3;++i)button({20+i*168.f,154,158,30},t==1?labelsNpc[i]:labelsResource[i],"node-subtab",std::to_string(i),true,(t==1?npcTab:resourceTab)==i);if(t==1&&npcTab==0)button({530,154,228,30},npcRole,"npc-role-filter");}
        if(t==0){
            button({20,154,220,30},"Type: "+itemType,"item-filter-type");
        }
        if(t==2||t==1&&npcTab!=0)text(std::to_string(matches.size())+" entries",566,160,17,muted);
        if(t==0&&!cloneTab) {
            const int first=scroll[0]*ItemsPerPage;
            for(int slot=0;slot<ItemsPerPage;++slot){
                const auto index=static_cast<size_t>(first+slot);if(index>=matches.size())break;
                const auto& e=entries[matches[index]];
                placard({GridX+(slot%Columns)*CardPitchX,GridY+(slot/Columns)*CardPitchY,CardWidth,CardHeight},e,"toggle-item",e.path,selection.contains(e.path),true,true);
            }
            pager(598,scroll[0],matches.size(),ItemsPerPage);
            button({20,634,158,34},selectedOnly?"All items":"Selected","selected-only");button({190,634,76,34},"Clear","clear");
            text(std::to_string(selection.size())+" selected",278,642,17,muted);
            button({404,634,160,34},"Cart / quantities","cart-open","",!selection.empty());
            button({580,634,264,34},"Give selected","give","",!busy&&catalog.authority&&Recipient()&&!selection.empty());
        }else if(t==0&&!cloneEditorOpen) {
            text("ITEM DRAFTS",24,200,20);button({568,192,190,38},"+ Create item","draft-add","",!busy);
            text("Each row has its own identity, stats, appearance and optional companions.",24,242,16,muted);
            SaveDraft();
            for(int i=0;i<6;++i){const auto at=static_cast<std::size_t>(draftPage*6+i);if(at>=drafts.size())break;const auto& d=drafts[at];const float y=276+i*48.f;
                soft({24,y,622,42},{.035f,.04f,.04f,1});icon(d.sourceInfo.icon,{30,y+4,34,34});
                text(Shorten(d.name.empty()?"New item - choose a source":d.name,48),76,y+2,18);
                text(d.created?"Created / source draft retained":d.ready?d.id:"Not inspected",76,y+24,15,muted);
                if(!busy)f.hits.push_back({{24,y,622,42},"draft-open",std::to_string(at)});
                button({658,y,100,42},"Remove","draft-remove",std::to_string(at),!busy);
            }
            if(drafts.empty())wrapped("Create an item row, then open it to choose the source and edit its properties. Draft rows are session-only; permanent outputs are written when you create the item.",24,310,72,4,18);
            pager(590,draftPage,drafts.size(),6);
            text("Creating one draft never automatically creates the other rows.",24,638,17,muted);
        }else if(t==0) {
            text("1  SOURCE ITEM",20,196,17,muted);text("2  APPEARANCE",270,196,17,muted);
            const char* modes[]{"Keep source","Copy appearance","Cooked mesh"};
            button({520,190,238,34},modes[static_cast<int>(appearanceMode)],"appearance-mode","",cloneReady&&!busy);
            Entry source=cloneSourceInfo;if(source.path.empty())source.name="Choose source";
            placard({20,228,230,150},source,!busy?"clone-source":"","",false);
            Entry donor=appearanceMode==Appearance::Copy?cloneAppearanceInfo:source;std::string action="clone-appearance";
            if(appearanceMode==Appearance::Copy&&donor.path.empty())donor.name="Choose cooked item";
            if(appearanceMode==Appearance::Mesh){donor={};donor.name="Choose cooked mesh";donor.path=cloneMesh;for(const auto& m:catalog.visuals)if(m.path==cloneMesh){donor=m;break;}action="mesh-picker";}
            placard({270,228,230,150},donor,!busy&&cloneReady?action:std::string{},"",false,appearanceMode!=Appearance::Mesh);
            const char* icons[]{"Auto: skin / source","Source icon","Appearance icon","Direct override"};
            text("3  INVENTORY ICON",520,236,17,muted);icon(EffectiveIcon(),{592,260,72,72});
            button({520,338,238,34},icons[static_cast<int>(iconMode)],"clone-icon-mode","",cloneReady&&!busy);
            if(appearanceMode==Appearance::Mesh)button({270,384,230,30},cloneMeshField.empty()?"Choose visual slot":cloneMeshField,"mesh-slots");
            else button({270,384,230,30},"Back to item rows","close");
            button({20,384,230,30},"RAW stats / properties","clone-advanced","",cloneReady&&!busy);
            button({520,380,238,30},"Choose icon from item","clone-icon-picker","",cloneReady&&!busy);
            text("4  ITEM DETAILS",20,424,16,muted);text("POWER LEVEL",520,424,16,muted);
            field({20,446,480,36},"clone-name","New item name");field({520,446,238,36},"clone-power","Inherit");
            text("FLAVOUR TEXT",20,488,16,muted);field({20,510,738,36},"clone-flavour","Inherited description; editable");
            text("COOKED ICON PATH",20,551,16,muted);field({20,573,480,34},"clone-icon",EffectiveIcon());
            button({520,573,110,34},std::string(makeJournal?"[x] ":"[ ] ")+"Journal","clone-journal","",cloneReady&&!busy);
            button({642,573,116,34},std::string(makeRecipe?"[x] ":"[ ] ")+"Recipe","clone-recipe","",cloneReady&&!busy);
            button({20,616,204,34},std::string(Authoring::PermanentAsset.load()?"[x] ":"[ ] ")+"Permanent","permanent-asset");
            button({236,616,134,34},std::string(cloneGive?"[x] ":"[ ] ")+"Give","clone-give");field({382,616,64,34},"quantity");
            button({458,616,126,34},std::string(cloneAcknowledged?"[x] ":"[ ] ")+"Test save","clone-ack");
            const bool temporaryBlocked=cloneReady&&!Authoring::PermanentAsset.load()&&cloneSoftDelete.empty();
            button({596,616,162,34},cloneCreated?"Created":temporaryBlocked?"Temp blocked":"Create item","create-clone","",!busy&&!cloneCreated&&cloneReady&&cloneAcknowledged&&!temporaryBlocked);
            if(Rect{458,616,126,34}.Contains(mouseX,mouseY))hoveredPath="Experimental single-player test: use a disposable or restorable save.";
        }else{
            const Tint card=ash,cardHover{.085f,.095f,.105f,1},cardInk=ink,cardMuted=muted;
            for(int slot=0;slot<ItemsPerPage;++slot){
                const size_t index=static_cast<size_t>(scroll[t]*ItemsPerPage+slot);if(index>=matches.size())break;
                const auto& e=entries[matches[index]];
                Rect r{GridX+(slot%Columns)*CardPitchX,GridY+(slot/Columns)*CardPitchY,CardWidth,CardHeight};
                soft(r,r.Contains(mouseX,mouseY)?cardHover:card);
                const auto kind=NodeKind(e,t);const auto favoriteKey=NodeFavoriteKey(e,t);
                std::string role;
                if(t==1) {
                    if(e.nodeKind=="NPC")role=e.npcVendor?"Merchant":e.npcQuestGiver?"Quest giver":"NPC";
                    else role="AI Enemy";
                }else role=!e.resourceFamily.empty()?e.resourceFamily:
                    kind==HelpyNodes::Kind::OtherResource?"World resource":"Resource";
                text(Shorten(e.name,31),r.x+14,r.y+12,17,e.available?cardInk:cardMuted);
                text(Shorten(role,31),r.x+14,r.y+39,14,cardMuted);
                text(Shorten(e.path.empty()?e.id:e.path,38),r.x+14,r.y+64,12,cardMuted);
                const Rect add{r.x+r.w-34,r.y+2,32,32};
                text("+",add.x+add.w/2,add.y+5,19,cardMuted,true);
                const Rect fav{r.x+r.w-34,r.y+r.h-30,32,28};
                text(IsFavorite(favoriteKey)?"*":"o",fav.x+fav.w/2,fav.y+4,15,IsFavorite(favoriteKey)?Tint{.55f,.38f,.02f,1}:cardMuted,true);
                if(e.available){f.hits.push_back({r,"node",e.id});f.hits.push_back({add,"node",e.id});}
                f.hits.push_back({fav,"node-favorite",favoriteKey});
                if(r.Contains(mouseX,mouseY))hoveredPath=(kind==HelpyNodes::Kind::OtherResource?"Other resource: ":"")+(e.path.empty()?e.detail:e.path);
            }
            pager(598,scroll[t],matches.size(),ItemsPerPage);
            if(t==1)button({20,634,348,34},"Dismiss temporary NPCs","dismiss-npcs","",!busy&&catalog.authority);
            else button({20,634,416,34},std::string(includeOtherResources?"[x] ":"[ ] ")+"Other resources in Stone & Ore","other-resources");
            
        }
        if(matches.empty()&&!(t==0&&cloneTab))text(NodeFavorites(t)?"No favorite definitions match this search.":t==0&&favoritesTab?
            favorites.empty()?"No favorites yet. Use the star on an item placard.":"No favorites match this search.":
            indexing?"Scanning game assets...":"No matches. Clear the filter or run a full scan.",30,250,18,muted);
        text(Shorten(hoveredPath.empty()?status:hoveredPath,87),20,674,16);
        if(node){
            const bool isNpc=node->nodeKind=="NPC";
            overlay(isNpc?"CONFIGURE NPC":tab==Tab::Resources?"CONFIGURE RESOURCE":"CONFIGURE AI");
            if(isNpc) {
                wrapped(node->name,36,94,74,1,19);text(Shorten(node->path,84),36,122,15,muted);
                text("Name (blank inherits)",36,145,17,muted);field({36,169,708,36},"name");
                text("Scale: 0.1 - 10",36,214,17,muted);field({36,238,180,36},"scale");
                button({236,238,508,36},std::string(Authoring::PermanentSpawn.load()?"[x] ":"[ ] ")+"Permanent NPC placement","permanent-spawn");
                button({36,292,708,36},npcGridPlacement?"PLACEMENT: PRECISE WORLD GRID":"PLACEMENT: RANDOM RADIUS AROUND PLAYER","npc-placement-mode");
                if(npcGridPlacement) {
                    text("X cells",36,337,15,muted);field({36,357,150,34},"npc-grid-x");text("Y cells",206,337,15,muted);field({206,357,150,34},"npc-grid-y");text("Grid size (cm)",376,337,15,muted);field({376,357,180,34},"npc-grid-size");
                    text("Snaps the player position to the grid, then applies X/Y cell offsets.",36,398,15,muted);
                }else {text("Radius around player (cm): 0 - 10000",36,337,15,muted);field({36,357,220,34},"npc-radius");text("Each request chooses a bounded point inside this radius.",278,365,15,muted);}
                if(!Authoring::PermanentSpawn.load()) {
                    text("Lifetime seconds",36,420,15,muted);field({180,412,160,34},"npc-duration");
                    wrapped("Timed NPCs are session-only. Their vendor, dialogue, quest and lore links remain owned by the source loaders.",36,458,72,3,17);
                }else wrapped("Writes X/Y from this placement and keeps Z as $+100 so the shared ground resolver places the NPC on terrain.",36,438,72,3,17);
                const bool allowed=Authoring::PermanentSpawn.load()?node->permanentAllowed:node->temporaryAllowed;
                const auto reason=Authoring::PermanentSpawn.load()?node->permanentReason:node->temporaryReason;
                if(!allowed)wrapped(reason.empty()?"This NPC mode is unavailable. Refresh to revalidate.":reason,36,520,76,2,16);
                button({36,606,330,40},"At: "+(Recipient()?Recipient()->name:"Select player"),"player-picker");
                button({446,606,298,40},Authoring::PermanentSpawn.load()?"Spawn + save NPC":"Spawn timed NPC","spawn","",!busy&&node->available&&allowed);
                text(Shorten(status,84),36,664,16,muted);
            }else {
            wrapped(node->name,36,94,74,1,19);text(Shorten(node->path,84),36,122,15,muted);
            text("Name (blank inherits)",36,151,17,muted);field({36,176,708,38},"name");
            text("Scale: 0.1 - 10",36,229,17,muted);field({36,254,174,38},"scale");
            text("Count: 1 - 20",226,229,17,muted);field({226,254,174,38},"count");
            const char* effects[]{"Original / inherit","None","Ghost Glow"};text("Visual effect",416,229,17,muted);button({416,254,328,38},effects[static_cast<int>(effect)],"effect");
            button({36,310,286,36},std::string(Authoring::PermanentSpawn.load()?"[x] ":"[ ] ")+"Permanent spawn","permanent-spawn");
            if(tab==Tab::Enemies){button({338,310,264,36},Authoring::EnemyPower.load()==-1?"Power: Native/default":"Power: Override","power-mode");if(Authoring::EnemyPower.load()!=-1)field({618,310,126,36},"power");}
            button({36,358,708,34},npcGridPlacement?"PLACEMENT: PRECISE WORLD GRID":"PLACEMENT: RADIUS AROUND PLAYER","npc-placement-mode");
            if(npcGridPlacement){text("X cells",36,400,14,muted);field({36,418,116,32},"npc-grid-x");text("Y cells",168,400,14,muted);field({168,418,116,32},"npc-grid-y");text("Grid cm",300,400,14,muted);field({300,418,130,32},"npc-grid-size");}
            else {text("Radius cm",36,400,14,muted);field({36,418,180,32},"npc-radius");text("Multiple spawns distribute around the radius.",236,425,14,muted);}
            text("ADDITIONAL LOOT  /  base drops retained",36,464,16,muted);button({602,456,142,32},"+ Add loot","loot-picker","",drops.size()<MaxDrops);
            for(int i=0;i<3;++i){const auto k=static_cast<size_t>(dropScroll+i);if(k>=drops.size())break;const auto& d=drops[k];const float y=494+i*35.f;
                const auto* item=FindItem(d.item);icon(item&&!item->icon.empty()?item->icon:d.icon,{36,y+2,30,30});text(Shorten(d.name,31),78,y+8,17);
                field({360,y,58,32},"min:"+d.item);field({426,y,58,32},"max:"+d.item);field({492,y,82,32},"chance:"+d.item);button({586,y,112,32},"Remove","remove-loot",d.item);
            }
            if(drops.empty())text("No additional loot.",36,510,16,muted);
            button({36,606,330,40},"At: "+(Recipient()?Recipient()->name:"Select player"),"player-picker");
            button({446,606,298,40},Authoring::PermanentSpawn.load()?"Spawn + save JSON":"Spawn selection","spawn","",!busy&&node->available);
            text(Shorten(node->available?status:node->detail,84),36,664,16,muted);
            }
        }
        if(cloneAdvancedOpen){
            overlay(cloneFieldPicker?(cloneRawFields?"ITEM LAB / RAW STAT FIELDS":"ITEM LAB / ASSET FIELDS"):(cloneRawFields?"ITEM LAB / RAW STATS":"ITEM LAB / ASSET PROPERTIES"));
            if(cloneFieldPicker)field({36,104,666,38},"clone-filter",cloneRawFields?"Find a defense or character stat...":"Find a direct item property...");
            else {
                button({36,98,344,36},"RAW STATS","clone-scope","raw",true,cloneRawFields);
                button({392,98,352,36},"ASSET FIELDS","clone-scope","asset",true,!cloneRawFields);
                if(cloneRawFields) {
                    button({36,142,220,36},"Defense / armour","clone-quick-field","Defense",cloneReady&&!busy);
                    button({272,142,220,36},"Melee resistance","clone-quick-field","MeleeResistance",cloneReady&&!busy);
                    button({508,142,236,36},"Ranged resistance","clone-quick-field","RangedResistance",cloneReady&&!busy);
                    button({36,186,220,36},"Magic resistance","clone-quick-field","MagicResistance",cloneReady&&!busy);
                    button({272,186,220,36},"Granted effects","clone-quick-field","GrantedEffects",cloneReady&&!busy);
                    button({508,186,236,36},"Pack / drop arrays","clone-quick-field","PackDrops",cloneReady&&!busy);
                }
                else {field({36,142,224,34},"clone-mod","Mod tag");text(Shorten(cloneId,26),274,150,17);button({544,142,200,34},"New identity","clone-new-id","",!busy);}
            }
            text(cloneFieldPicker?(cloneRawFields?"These values become a clone-owned DataTable row and a permanent /raw companion.":"These values are written directly into the cloned /assets record."):(cloneRawFields?"Defense and character values stay isolated from vanilla rows and write through /raw.":"Direct item fields write into the cloned /assets definition."),36,cloneFieldPicker?158.f:230.f,16,muted);
            const auto found=CloneMatches();const int visible=cloneFieldPicker?7:5;const float listTop=cloneFieldPicker?196.f:264.f;
            for(int i=0;i<visible;++i){const auto at=static_cast<std::size_t>(cloneScroll*visible+i);if(at>=found.size())break;const auto& v=cloneFields[found[at]];const bool edited=cloneEdits.contains(v.name);const float y=listTop+i*50.f;
                const Rect row{36,y,708,44};soft(row,edited||row.Contains(mouseX,mouseY)?gold:Tint{.15f,.16f,.16f,1});soft({37,y+1,706,42},{.018f,.021f,.021f,1});
                text(Shorten(v.scope.empty()?v.name:v.scope+" / "+v.nativeName,70),48,y+3,18,v.editable?ink:muted);
                text(Shorten(v.editable?(edited?cloneEdits.at(v.name):v.hasValue?v.value:"Inherited / "+v.type):v.reason,76),48,y+25,15,muted);
                if(v.editable&&!busy)f.hits.push_back({row,"clone-field",v.name});
            }
            if(found.empty())text(cloneFieldPicker?"No matching properties.":"No property overrides yet. Choose Add property below.",36,240,18,muted);
            pager(554,cloneScroll,found.size(),visible);
            if(!cloneFieldPicker)button({36,604,708,40},cloneRawFields?"Browse basic characteristics and all RAW stats":"Browse direct asset properties","clone-add-field","",cloneReady&&!busy);
            text(Shorten(status,84),36,658,16,muted);
        }
        if(companionsOpen){
            overlay("JOURNAL ENTRY FOR THIS ITEM");
            button({36,102,708,38},std::string(makeJournal?"[x] ":"[ ] ")+"Create and link journal entry","make-journal");
            text("TITLE  /  blank uses item name",36,160,16,muted);field({36,183,708,38},"journal-title",cloneName);
            text("DESCRIPTION  /  starts from item flavour text",36,238,16,muted);field({36,261,708,62},"journal-text",cloneFlavour);
            button({36,337,708,34},"Reset description from current flavour text","journal-reset");
            text("JOURNAL LOCATION",36,392,16,muted);
            button({36,416,708,40},journalTarget.path.empty()?"Choose journal subcategory":journalTarget.name,"choose-journal","",makeJournal);
            if(journalTarget.grouped){text("GROUP ID AND DISPLAY NAME",36,476,16,muted);field({36,500,330,36},"journal-group","Group ID");field({384,500,360,36},"journal-group-name","Group display name");}
            wrapped("Journal output requires Permanent. The item and entry keep separate identities and are linked through the existing journal loader. Restart to verify placement and discovery behavior.",36,576,82,4,17);
        }
        if(recipeOpen){
            overlay(recipeDetailsMode?"EDIT / EXPORT RECIPE FOR "+Shorten(recipeTargetName,32):"RECIPE FOR THIS ITEM");
            if(recipeDetailsMode)wrapped("This creates a RuneSchema recipe for the selected item. The vanilla recipe remains unchanged.",36,98,78,2,17);
            else button({36,98,708,38},std::string(makeRecipe?"[x] ":"[ ] ")+"Create and link recipe","make-recipe");
            text("CRAFTING STATION",36,151,16,muted);
            if(!recipeStation.icon.empty())icon(recipeStation.icon,{40,176,34,34});
            button({recipeStation.icon.empty()?36.f:82.f,174,recipeStation.icon.empty()?708.f:662.f,38},recipeStation.path.empty()?"Choose a loaded crafting station":recipeStation.name,"choose-station","",makeRecipe);
            text("CATEGORY",36,226,16,muted);field({36,249,326,34},"recipe-category","Helpy");
            text("OUTPUT",390,226,16,muted);field({390,249,110,34},"recipe-output");
            button({520,249,224,34},std::string(unlockRecipe?"[x] ":"[ ] ")+"Auto-unlock","recipe-unlock","",makeRecipe);
            text("INGREDIENTS",36,303,18);button({544,299,200,34},"+ Add item","recipe-add","",makeRecipe&&ingredients.size()<16);
            for(int i=0;i<5;++i){const auto at=static_cast<std::size_t>(ingredientPage*5+i);if(at>=ingredients.size())break;const auto& v=ingredients[at];const float y=349+i*43.f;icon(v.icon,{36,y,34,34});text(Shorten(v.name,41),82,y+9,17);field({500,y,88,34},"ingredient:"+v.path);button({608,y,136,34},"Remove","recipe-remove",v.path);}
            pager(530,ingredientPage,ingredients.size(),5);
            if(recipeDetailsMode)button({544,580,200,40},"Export recipe JSON","recipe-export","",!busy);
            wrapped(recipeDetailsMode?"The export is installed in /recipes and is available after loader reload or restart.":"Recipe output references the new item's stable identity. Permanent is required. Station choices come from loaded tables; auto-unlock is opt-in and never silently enabled.",36,recipeDetailsMode?620.f:594.f,80,3,17);
        }
        if(ItemPickerOpen()){
            overlay(cloneSourcePicker?"SELECT ITEM SOURCE":cloneAppearancePicker?"COPY COOKED APPEARANCE":ingredientPicker?"SELECT RECIPE INGREDIENT":iconPicker?"USE AN ITEM ICON":"SELECT ADDITIONAL LOOT");
            field({36,104,492,40},"loot-filter","Search item name or path...");
            button({540,104,204,40},pickerFavoritesOnly?"Favorites only":"All items","picker-favorites","",true,pickerFavoritesOnly);
            button({36,152,342,32},pickerType,"item-filter-type");button({390,152,354,32},pickerSource,"item-filter-source");
            const auto& found=LootMatches();const int first=lootScroll*ItemsPerPage;
            for(int slot=0;slot<ItemsPerPage;++slot){const auto at=static_cast<size_t>(first+slot);if(at>=found.size())break;const auto& e=catalog.entries[0][found[at]];
                placard({GridX+(slot%Columns)*CardPitchX,GridY+(slot/Columns)*CardPitchY,CardWidth,CardHeight},e,cloneSourcePicker?"source-clone":cloneAppearancePicker?"appearance-clone":ingredientPicker?"ingredient-item":iconPicker?"icon-item":"add-loot",e.path);
            }
            pager(598,lootScroll,found.size(),ItemsPerPage);
            if(found.empty())text(indexing?"Item catalogue is still loading...":pickerFavoritesOnly?
                "No compatible favorites match this search.":cloneSourcePicker||cloneAppearancePicker?
                "No eligible items match these filters. Check Coverage or clear filters.":"No matching items. Clear the filter or scan.",36,254,18,muted);
            text(Shorten(hoveredPath.empty()?status:hoveredPath,84),36,655,16,muted);
            if(!favoritesWritable)text("Favorites are session-only; Favorites file unavailable (see log).",36,682,15,muted);
        }
        if(cloneMeshPicker){
            overlay("SELECT COOKED MESH / MESH DATA");button({36,98,666,38},"Slot: "+(cloneMeshField.empty()?"Select a slot":cloneMeshField),"mesh-slots");
            field({36,148,666,38},"mesh-filter","Search compatible cooked assets...");
            const auto& found=MeshMatches();
            for(int i=0;i<9;++i){const auto at=static_cast<size_t>(meshScroll*9+i);if(at>=found.size())break;const auto& e=catalog.visuals[found[at]];const float y=206+i*44.f;
                button({36,y,666,40},e.name,"mesh-select",e.path);if(Rect{36,y,666,40}.Contains(mouseX,mouseY))hoveredPath=e.path;
            }
            pager(610,meshScroll,found.size(),9);
            text(cloneSourceInfo.appearanceGroup.starts_with("held:")?"This slot only. Use Copy Appearance for the equipped held actor.":"Skeleton follows the cooked skeletal mesh or mesh-data asset.",36,647,16,muted);
            text(Shorten(hoveredPath.empty()?"Shared mesh/skeleton assets are never edited in place.":hoveredPath,84),36,672,15,muted);
        }
        if(cloneModeOpen){
            overlay("APPEARANCE MODE");
            button({60,152,660,64},"Keep source appearance","appearance-set","0",true,appearanceMode==Appearance::Source);
            button({60,240,660,64},"Copy appearance from a cooked item","appearance-set","1",true,appearanceMode==Appearance::Copy);
            button({60,328,660,64},"Choose a cooked mesh / mesh-data slot","appearance-set","2",true,appearanceMode==Appearance::Mesh);
            wrapped("Copy Appearance carries the donor's supported visual references, including male/female mesh data where present. Auto icon uses the skin icon, with source fallback. Direct override remains available.",60,440,71,4,18);
        }
        if(cloneMeshFieldPicker){
            overlay("SELECT A VISUAL SLOT");int row=0;
            for(const auto& v:cloneFields)if(!v.visualType.empty()){
                if(row>=9)break;
                button({36,120+row*48.f,666,42},v.name+" / "+v.visualType,"mesh-slot",v.name);++row;
            }
            if(row==0)wrapped("No direct mesh slots exist on this item. Use Copy Appearance when the item stores its visuals indirectly.",36,145,74,3,18);
            text("Only reflected, compatible cooked reference fields are shown.",36,640,17,muted);
        }
        if(playerPicker){
            overlay("SELECT CONNECTED PLAYER");text("Your selected recipient is never silently replaced.",36,108,17,muted);
            for(int i=0;i<9;++i){const auto at=static_cast<size_t>(playerScroll*9+i);if(at>=catalog.players.size())break;const auto& v=catalog.players[at];button({36,154+i*48.f,666,42},v.name+(v.self?" (you)":""),"player",v.id);}
            pager(610,playerScroll,catalog.players.size(),9);
        }
        if(choicePicker){
            overlay(choiceKind=="journal"?"SELECT JOURNAL SUBCATEGORY":"SELECT CRAFTING STATION");field({36,104,708,40},"choice-filter","Search loaded destination...");
            const auto& list=choiceKind=="journal"?journalChoices:stationChoices;const auto found=Choices();
            for(int i=0;i<8;++i){const auto at=static_cast<std::size_t>(choicePage*8+i);if(at>=found.size())break;const auto& v=list[found[at]];const float y=174+i*48.f;if(!v.icon.empty())icon(v.icon,{40,y+4,34,34});button({v.icon.empty()?36.f:82.f,y,v.icon.empty()?708.f:662.f,42},v.name,"choose-target",v.id);}
            if(found.empty())wrapped("No loaded destinations match. Inspect a source again after loading the game catalogue. No destination is guessed.",36,220,78,3,18);
            pager(580,choicePage,found.size(),8);
        }
        if(cloneFieldOpen){
            overlay(itemDetailFieldMode?"EDIT ITEM OVERRIDE":"EDIT ITEM PROPERTY");const auto* current=CurrentCloneField();
            wrapped(current&&!current->scope.empty()?current->scope+" / "+current->nativeName:cloneField,36,108,73,2,20);
            text("Value type: "+cloneKind,36,180,18,muted);
            if(cloneKind=="Text"||cloneKind=="JSON")text("Enter adds a line break; Save value applies this row.",36,208,16,muted);
            if(cloneKind=="Boolean")button({36,238,708,50},cloneValue=="true"?"Enabled / true":"Disabled / false","clone-bool");
            else field({36,238,708,(cloneKind=="Text"||cloneKind=="JSON")?202.f:50.f},"clone-value",HelpyPropertyValue::IsText(cloneKind)?"Type text without JSON quotes":"Enter value");
            wrapped(itemDetailFieldMode?"This override is exported without changing the live vanilla record. Linked DataTable fields route to /raw; direct ItemData fields route to /assets.":cloneKind=="JSON"?"Composite values use JSON. Only supported reflected fields are accepted. Shared referenced objects cannot be changed inline.":"Only the new item (or its own stat-table row) is changed. Inherit source removes this override.",36,466,79,4,18);
            button({36,582,270,44},itemDetailFieldMode?"Remove override":"Inherit source","clone-inherit");button({474,582,270,44},"Save value","clone-apply");
            text(Shorten(status,84),36,654,16,muted);
        }
        if(helpySettingsOpen){
            overlay("RUNESCHEMA HELPY");text("Open / close hotkey",36,110,20);
            button({36,152,110,42},"Previous","hotkey-prev");text(selectedHotkey,360,162,24,ink,true);button({612,152,132,42},"Next","hotkey-next");
            button({36,220,708,42},"Save hotkey (active: "+helpyKey+")","hotkey-save","",!busy);
            wrapped("Choose F1-F24, Insert, Home, End, Pause or ScrollLock. Avoid a key already used by another mod. Escape still closes the active panel.",36,292,72,3,18);
            text("REFERENCE INDEX",36,392,20);
            wrapped("Refresh and Full scan reuse saved RSDW paths. A successful index does not expire. Only this button requests a replacement download. Game objects are still validated locally.",36,436,72,4,18);
            button({36,568,708,42},indexing?"Wait for the current scan":"Update RSDW reference index","update-rsdw","",!busy&&!indexing);
            text(Shorten(status,84),36,648,16,muted);
        }
        if(helpyAboutOpen){
            overlay("ABOUT HELPY");
            badge(QuickDecorations::RuneSchemaBadge,{244,94,78,78},"RS");
            badge(QuickDecorations::Ue4ssBadge,{458,94,78,78},"U4");
            text("Helpy for RuneSchema 0.7.5.9",390,194,27,ink,true);
            text("In-game browser and authoring tool",390,234,20,gold,true);
            wrapped("Helpy browses the live item, NPC, AI and resource catalogues on demand. Grants and spawns use RuneSchema authority services, while Item Lab exports mod-owned item and recipe definitions.",62,286,68,5,18);
            wrapped("The interface and its navigation are supplied by the Helpy plugin. RuneSchema provides the loader API, permissions and runtime bridge; Helpy does not replace the underlying loaders.",62,408,68,5,18);
            text("Optional plugin  /  capability-based compatibility",390,548,17,muted,true);
            text("Maintained by the RSDW Modding Community",390,586,17,muted,true);
            text("Select the RS logo at any time to open this page.",390,625,16,muted,true);
        }
        if(reportOpen){
            overlay("ITEM GRANT RESULTS");wrapped("Unconfirmed may mean items were granted. Check inventory before retrying.",36,104,78,2,17);
            for(int i=0;i<7;++i){const auto at=static_cast<size_t>(reportScroll*7+i);if(at>=grantReport.size())break;const auto& row=grantReport[at];const float y=170+i*62.f;text(Shorten(row.name+" / "+row.state,70),36,y,18);text(Shorten(row.message,82),36,y+27,16,muted);}
            pager(610,reportScroll,grantReport.size(),7);
            text("No automatic retries. Confirmed items leave the selection.",36,643,17,muted);
        }
        if(coverageOpen){
            overlay("CATALOGUE COVERAGE");wrapped(coverageSummary.empty()?catalogStatus:coverageSummary,36,98,81,3,17);
            for(int i=0;i<7;++i){const auto at=static_cast<size_t>(coverageScroll*7+i);if(at>=catalog.issues.size())break;const auto& issue=catalog.issues[at];const float y=205+i*56.f;text(Shorten(issue.path,83),36,y,16);text(Shorten(issue.reason,83),36,y+24,16,muted);}
            pager(606,coverageScroll,catalog.issues.size(),7);
            if(catalog.issues.empty())text(indexing?"Scan is still running.":"No retained scan issues. Runtime coverage still needs verification.",36,246,17,muted);
            text("Definitions are discovered from the game, not a fixed zone list.",36,638,17,muted);
        }
        if(itemDetailsOpen) {
            overlay("ITEM DETAILS");const auto* e=FindItem(itemDetailsPath);
            if(!e)wrapped("This item is no longer in the current catalogue. Close details and refresh.",36,126,76,3,18);
            else {
                icon(e->icon,{36,92,82,82});wrapped(e->name,136,96,54,2,22);
                powerMark(e->power,136,153,18);
                const auto origin=e->runtimeClone||e->runeSchemaManaged?"RuneSchema":e->declaredModded?"Modded":e->cooked?"Dragonwilds":"Unknown source";
                text(std::string(origin)+"  /  "+Shorten(e->assetClass,44),310,153,16,muted);
                wrapped(e->path,36,181,84,2,15);
                button({36,220,220,34},"STATS / EFFECTS","item-details-tab","0",true,itemDetailsTab==0);
                button({280,220,220,34},"RECIPES / STATIONS","item-details-tab","1",true,itemDetailsTab==1);
                button({524,220,220,34},"JOURNAL ENTRIES","item-details-tab","2",true,itemDetailsTab==2);
                if(!itemDetailsLoaded)wrapped(busy?"Reading the loaded item, its stat rows, recipes, stations and journal links...":status,36,300,76,4,19);
                else if(itemDetailsTab==0) {
                    const int pageSize=7;for(int i=0;i<pageSize;++i){const auto at=static_cast<std::size_t>(itemDetailsPage*pageSize+i);if(at>=itemDetailFields.size())break;
                        const auto& v=itemDetailFields[at];const float y=274+i*43.f;const Rect row{32,y-4,712,38};const bool edited=itemDetailEdits.contains(v.name),hover=v.editable&&row.Contains(mouseX,mouseY);
                        if(edited||hover)soft(row,edited?gold:Tint{.15f,.16f,.16f,1},2);
                        text(Shorten(v.scope.empty()?v.nativeName:v.scope+" / "+v.nativeName,47),36,y,16);
                        text(Shorten(edited?itemDetailEdits.at(v.name):v.hasValue?v.value:v.reason,55),382,y,15,edited?ink:muted);
                        if(v.editable&&!busy)f.hits.push_back({row,"item-detail-field",v.name});
                    }
                    if(itemDetailFields.empty())text("No safely readable reflected fields were returned.",36,304,18,muted);
                    pager(588,itemDetailsPage,itemDetailFields.size(),pageSize);
                    button({500,632,244,38},itemDetailEdits.empty()?"Edit a field to export":"Export "+std::to_string(itemDetailEdits.size())+" override(s)","item-overrides-export","",!busy&&!itemDetailEdits.empty());
                }else if(itemDetailsTab==1) {
                    const int pageSize=3;for(int i=0;i<pageSize;++i){const auto at=static_cast<std::size_t>(itemDetailsPage*pageSize+i);if(at>=itemDetailRecipes.size())break;
                        const auto& recipe=itemDetailRecipes[at];const float y=276+i*98.f;soft({32,y-7,716,91},{.025f,.03f,.03f,1},3);text(Shorten(recipe.name,46),44,y,18);button({610,y-3,126,27},"Edit / export","item-recipe-edit",std::to_string(at),!busy);
                        icon(e->icon,{44,y+30,40,40});text("x"+recipe.output,88,y+43,15,muted);text("<-",126,y+40,20,gold);
                        float ix=166;for(std::size_t n=0;n<recipe.ingredients.size()&&n<5;++n){const auto& ingredient=recipe.ingredients[n];icon(ingredient.icon,{ix,y+30,36,36});text("x"+ingredient.count,ix,y+67,13,muted,true);ix+=52;}
                        if(recipe.ingredients.size()>5)text("+"+std::to_string(recipe.ingredients.size()-5),ix,y+42,15,muted);
                        text("@",500,y+40,18,gold);if(!recipe.station.icon.empty())icon(recipe.station.icon,{528,y+30,36,36});
                        text(Shorten(recipe.station.name.empty()?"Unknown station":recipe.station.name,19),568,y+42,14,muted);
                    }
                    if(itemDetailRecipes.empty())text("No loaded recipe creates this item.",36,304,18,muted);
                    pager(588,itemDetailsPage,itemDetailRecipes.size(),pageSize);
                    button({500,632,244,38},"Create recipe export","item-recipe-new","",!busy);
                }else {
                    const int pageSize=7;for(int i=0;i<pageSize;++i){const auto at=static_cast<std::size_t>(itemDetailsPage*pageSize+i);if(at>=itemDetailJournals.size())break;
                        text(Shorten(itemDetailJournals[at],82),36,282+i*43.f,17);
                    }
                    if(itemDetailJournals.empty())text("No loaded journal entry links this item or its recipes.",36,304,18,muted);
                    pager(588,itemDetailsPage,itemDetailJournals.size(),pageSize);
                }
                if(itemDetailsTab!=0&&itemDetailsTab!=1)button({36,632,330,38},selection.contains(e->path)?"Remove from selection":"Add to selection","toggle-item",e->path,!busy);
                if(itemDetailsTab!=0&&itemDetailsTab!=1)button({394,632,350,38},"Quick give x1 to "+(Recipient()?Recipient()->name:"player"),"item-quick-give",e->path,!busy&&catalog.authority&&Recipient());
            }
        }
        if(!itemFilterMenu.empty()) {
            overlay(itemFilterMenu=="type"?"FILTER BY ITEM TYPE":"FILTER BY SOURCE");
            const auto& values=itemFilterMenu=="type"?HelpyItemFilters::Types():HelpyItemFilters::Sources();
            const auto current=itemFilterMenu=="type"?(filterForPicker?pickerType:itemType):(filterForPicker?pickerSource:itemSource);
            for(std::size_t i=0;i<values.size();++i)button({36+static_cast<float>(i%2)*360,118+static_cast<float>(i/2)*62,348,48},values[i],"item-filter-value",values[i],true,current==values[i]);
            wrapped("Filters combine with search and Favorites before pagination. Combined mods can match both RuneSchema and Cooked mods. Item creation eligibility is checked separately.",36,550,72,4,18);
        }
        if(actionReportOpen){
            overlay(Shorten(actionTitle,47));wrapped(actionMessage,36,137,76,14,18);
        }
        if(cartOpen) {
            f.hits.clear();f.rightHits.clear();hoveredPath.clear();
            rect({0,0,ContentWidth,Height},{0,0,0,.84f});panel({16,28,748,664},ash);
            text("ITEM CART",36,47,23,{.84f,.68f,.36f,1});
            button({656,38,88,38},"Back","cart-close");
            text("Choose a quantity for each item. Grants are sent as one bounded batch.",36,92,17,muted);
            std::vector<std::string> cart(selection.begin(),selection.end());
            constexpr int CartRows=7;const int first=cartPage*CartRows;
            for(int row=0;row<CartRows;++row) {
                const auto at=static_cast<std::size_t>(first+row);if(at>=cart.size())break;
                const auto* entry=FindItem(cart[at]);if(!entry)continue;
                const float y=132+row*58.f;soft({36,y,708,50},{.035f,.04f,.04f,1});
                icon(entry->icon,{44,y+7,36,36});
                text(Shorten(entry->name,42),92,y+15,17,entry->available?ink:muted);
                field({474,y+8,112,34},"grant-count:"+entry->path,"Qty");
                button({602,y+8,132,34},"Remove","cart-remove",entry->path,!busy);
            }
            if(cart.empty())text("Your cart is empty. Select items to add them.",36,174,18,muted);
            pager(548,cartPage,cart.size(),CartRows);
            button({36,612,708,46},"Give "+std::to_string(cart.size())+" item types to "+(Recipient()?Recipient()->name:"select a player"),"give","",!busy&&catalog.authority&&Recipient()&&!cart.empty());
            if(!catalog.authority)text("Item grants are available only to the authoritative host.",36,668,15,muted);
        }
        const std::string summary=indexing?indexStage:!status.empty()?status:
            "Items "+std::to_string(catalog.entries[0].size())+"  /  NPC + AI "+std::to_string(catalog.entries[1].size())+
            "  /  Resources "+std::to_string(catalog.entries[2].size());
        text(Shorten(summary,92),20,692,14,muted);
        if(indexing){
            const Rect progress{20,713,ContentWidth-40,3};rect(progress,{.10f,.11f,.12f,1});
            if(indexHasTotal){const double fraction=indexTotal?std::min(1.0,static_cast<double>(indexDone)/indexTotal):0.0;rect({progress.x,progress.y,progress.w*static_cast<float>(fraction),progress.h},gold);}
            else {const float at=static_cast<float>((progressFrame++/3)%81)/100.f;rect({progress.x+progress.w*at,progress.y,progress.w*.19f,progress.h},gold);}
        }
        // Move the existing content surface beside a persistent DLL-drawn rail.
        for(auto& draw:f.draws)draw.box.x+=NavRailWidth;
        for(auto& hit:f.hits)hit.box.x+=NavRailWidth;
        for(auto& hit:f.rightHits)hit.box.x+=NavRailWidth;

        const bool navBlocked=node.has_value()||ItemPickerOpen()||playerPicker||choicePicker||cloneFieldOpen||cloneAdvancedOpen||
            cloneMeshPicker||cloneMeshFieldPicker||cloneModeOpen||coverageOpen||helpySettingsOpen||helpyAboutOpen||
            reportOpen||actionReportOpen||itemDetailsOpen||recipeOpen||cartOpen||!itemFilterMenu.empty();
        const auto railRect=[&](Rect r,Tint c){f.draws.push_back({Draw::Kind::Rectangle,r,{},c,18,false,{}});};
        const auto railText=[&](std::string value,float x,float y,float size,Tint color,bool centre=false){f.draws.push_back({Draw::Kind::Text,{x,y,0,0},std::move(value),color,size,centre,{}});};
        const auto railButton=[&](float y,const char* mark,const char* label,const char* arg,bool active){
            const Rect r{8,y,NavRailWidth-16,62};const bool hover=!navBlocked&&!busy&&r.Contains(navMouseX,mouseY);
            if(active||hover)railRect({r.x,r.y,r.w,r.h},active?Tint{.075f,.085f,.095f,1}:Tint{.04f,.047f,.052f,1});
            if(active)railRect({r.x,r.y,3,r.h},ink);
            railText(mark,r.x+r.w/2,r.y+6,21,active?ink:muted,true);
            railText(label,r.x+r.w/2,r.y+36,11,active?ink:muted,true);
            if(!navBlocked&&!busy)f.hits.push_back({r,"rail",arg});
        };
        railRect({0,0,NavRailWidth,Height},{.018f,.02f,.022f,1});
        railRect({NavRailWidth-1,0,1,Height},{.11f,.12f,.13f,1});
        railText("RS",NavRailWidth/2,20,24,ink,true);
        railText("HELPY",NavRailWidth/2,50,11,muted,true);
        railButton(92,"I","Items","items",tab==Tab::Items&&!cloneTab);
        railButton(164,"A","AI","ai",tab==Tab::Enemies);
        railButton(236,"R","Resources","resources",tab==Tab::Resources);
        railButton(308,"+","Item Lab","clone",tab==Tab::Items&&cloneTab);
        railButton(566,"S","Settings","settings",helpySettingsOpen);
        railText(helpyKey,NavRailWidth/2,654,12,muted,true);

        // Narrow the complete shell while preserving square item artwork.
        for(auto& draw:f.draws) {
            auto& box=draw.box;
            if(draw.kind==Draw::Kind::Icon||draw.kind==Draw::Kind::Badge)box.x=(box.x+box.w/2)*HorizontalFit-box.w/2;
            else {box.x*=HorizontalFit;box.w*=HorizontalFit;}
        }
        for(auto& hit:f.hits){hit.box.x*=HorizontalFit;hit.box.w*=HorizontalFit;}
        for(auto& hit:f.rightHits){hit.box.x*=HorizontalFit;hit.box.w*=HorizontalFit;}
        return f;
    }

private:
    struct MatchCache {
        uint64_t revision=0,favoriteRevision=0;std::string query;bool onlySelected=false,onlyFavorites=false;
        std::set<std::string> selected;std::vector<size_t> indices;
    };
    uint64_t catalogRevision=1;
    mutable uint64_t favoriteCatalogRevision=0,favoriteListRevision=0;
    mutable std::vector<Entry> favoriteEntries;
    mutable std::array<MatchCache,5> matchCache;
    std::optional<Command> pending;
    void RequireAuthority() const {
        if(!catalog.authority)throw std::runtime_error("Run this action on the authoritative host. A client-side UI does not grant server permissions.");
        if(!Recipient())throw std::runtime_error("The selected player is unavailable. Refresh and select a connected player.");
    }
};
} // namespace PS::QuickUI
