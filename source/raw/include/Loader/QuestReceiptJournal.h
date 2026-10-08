#pragma once
#include "Loader/QuestProgress.h"
#include "Loader/QuestSaveOwnership.h"
#include <chrono>
#include <bit>
#include <functional>

namespace DragonWilds::Quests {
class ReceiptJournal {
    std::function<int(const std::string&)> reader;
    std::function<void(const std::string&,int)> writer;
    std::function<int64_t()> clock;
    const Definition& quest;
    const Json& document;
    int Read(const std::string& key) const {
        return reader(key);
    }
    void Write(const std::string& key,int value) const {
        writer(key,value);
        if(reader(key)!=value)throw std::runtime_error("Quest receipt write was not confirmed");
    }
    int64_t Time(const std::string& key) const {
        const auto low=std::bit_cast<uint32_t>(Read(key+"Lo"));
        const auto high=Read(key+"Hi");
        if(high<0)throw std::runtime_error("Invalid saved quest timestamp");
        return (int64_t(high)<<32)|low;
    }
    void Time(const std::string& key,int64_t value) const {
        if(value<0)throw std::runtime_error("Invalid quest timestamp");
        Write(key+"Lo",std::bit_cast<int32_t>(uint32_t(value)));
        Write(key+"Hi",int32_t(value>>32));
    }
    static uint64_t Fingerprint(const Json& value) {
        uint64_t hash=14695981039346656037ULL;
        for(unsigned char byte:value.dump()){hash^=byte;hash*=1099511628211ULL;}
        return hash;
    }
    uint64_t Fingerprint() const {return Fingerprint(document);}
    bool MatchesFingerprint(const Json& value) const {
        const auto hash=Fingerprint(value);
        return Read("RuneSchema.DefLo")==std::bit_cast<int32_t>(uint32_t(hash))
            && Read("RuneSchema.DefHi")==std::bit_cast<int32_t>(uint32_t(hash>>32));
    }
    void Stamp() const {
        const auto owner=quest.Key.substr(0,quest.Key.find(':'));
        for(const auto& variable:OwnershipVariables(owner,quest.PersistenceId))Write(variable.at("QuestVariableName").get<std::string>(),OwnershipVersion);
        if(quest.Marker)Write("RuneSchema.Location:"+quest.Marker->Name,OwnershipVersion);
        for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
            if(objective.Marker)Write("RuneSchema.Location:"+objective.Marker->Name,OwnershipVersion);
        const auto hash=Fingerprint();
        Write("RuneSchema.DefLo",std::bit_cast<int32_t>(uint32_t(hash)));
        Write("RuneSchema.DefHi",std::bit_cast<int32_t>(uint32_t(hash>>32)));
    }
    void ResetFresh() const {
        Stamp();
        for(const auto* key:{"RuneSchema.Run","RuneSchema.Phase","RuneSchema.Entry","RuneSchema.Tier",
            "RuneSchema.ExchangeStep","RuneSchema.ExchangeKind"})Write(key,0);
        Time("RuneSchema.Started",0);Time("RuneSchema.Completed",0);Time("RuneSchema.ObjectiveAt",0);Time("RuneSchema.ExchangeAt",0);
        Time("RuneSchema.Clock",Now());Write("RuneSchema.Version",OwnershipVersion);
    }
public:
    // 0 ungiven, 1 active, 2 pending acceptance, 3 pending exchange, 4 complete.
    ReceiptJournal(const Definition& definition,const Json& json,const std::filesystem::path& legacy,const std::string& character,
        std::function<int(const std::string&)> read,std::function<void(const std::string&,int)> write,
        std::function<bool()> initialized,std::function<void()> initialize,std::function<std::string()> nativeStateName,
        std::function<int64_t()> currentTime=EpochNow,std::function<void()> resetChanged={}):reader(std::move(read)),writer(std::move(write)),clock(std::move(currentTime)),quest(definition),document(json) {
        if(!initialized()) {
            const auto old=QuestProgress::Read(legacy,character);
            if(old.at("Quests").contains(quest.Key))throw std::runtime_error("Legacy receipt has no matching native quest; migration refused");
            initialize();
            if(!initialized())throw std::runtime_error("Quest initialization was not confirmed");
        }
        const int version=Read("RuneSchema.Version");
        if(!version) {
            auto old=QuestProgress::Read(legacy,character);
            if(old.at("Quests").contains(quest.Key)
                && old.at("Quests").at(quest.Key).at("Definition")!=document) {
                if(!resetChanged)throw std::runtime_error("Changed legacy quest requires an authoritative reset");
                resetChanged();
                ResetFresh();
                old["Quests"].erase(quest.Key);
                QuestProgress::Write(legacy,old);
                return;
            }
            const auto state=QuestProgress::Status(old,quest,document);
            auto nativeState=nativeStateName();
            const auto colon=nativeState.rfind("::");if(colon!=nativeState.npos)nativeState=nativeState.substr(colon+2);
            if((state==QuestProgress::State::Active && nativeState!="Given") || (state==QuestProgress::State::Complete && nativeState!="Complete"))
                throw std::runtime_error("Legacy/native quest states disagree; migration refused");
            int phase=0;
            if(state==QuestProgress::State::Active)phase=1;
            else if(state==QuestProgress::State::Complete)phase=4;
            else if(state==QuestProgress::State::Pending)phase=old.at("Quests").at(quest.Key).at("Operation")=="accept"?2:3;
            else if(nativeState=="Complete")phase=4;
            else if(nativeState=="Given")phase=1;
            // Import metadata only. Never replay inventory operations during migration.
            Stamp();Write("RuneSchema.Run",phase?1:0);
            Time("RuneSchema.Started",0);Time("RuneSchema.Completed",phase==4?Now():0);
            Time("RuneSchema.Clock",Now());Write("RuneSchema.Phase",phase);
            Write("RuneSchema.Version",OwnershipVersion);
        } else if(version!=OwnershipVersion)throw std::runtime_error("Unsupported native quest receipt version");
        if(!MatchesFingerprint(document)) {
            auto prior=document;prior.erase("Repeatable");
            auto explicitFalse=prior;explicitFalse["Repeatable"]=false;
            auto nativeState=nativeStateName();const auto colon=nativeState.rfind("::");
            if(colon!=nativeState.npos)nativeState=nativeState.substr(colon+2);
            const bool stableRun=(Phase()==1 && nativeState=="Given")
                || (Phase()==4 && nativeState=="Complete");
            if(quest.Repeat.Enabled && !document.contains("Repeat") && stableRun && Run()>=1
                && (MatchesFingerprint(prior) || MatchesFingerprint(explicitFalse)))Stamp();
            else if(resetChanged){resetChanged();ResetFresh();}
            else throw std::runtime_error("Saved quest definition changed; explicit migration required");
        }
        if(Phase()<0 || Phase()>4 || Run()<0)throw std::runtime_error("Malformed native quest receipt");
    }
    static int64_t EpochNow(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
    int64_t Now() const {return clock();}
    int Phase() const{return Read("RuneSchema.Phase");}
    int Run() const{return Read("RuneSchema.Run");}
    bool RecoverConfirmedExchange(const std::string& nativeState) const {
        if(Phase()!=3 || Run()<1 || Read("RuneSchema.ExchangeStep")!=1)return false;
        const int kind=Read("RuneSchema.ExchangeKind");
        if(kind==1 && nativeState=="Given") {
            Write("RuneSchema.Phase",1);
            return true;
        }
        if(kind!=2 || nativeState!="Complete")return false;
        const auto completed=Time("RuneSchema.ExchangeAt");
        if(completed<=0 || completed<Time("RuneSchema.Started") || completed<Time("RuneSchema.Clock"))
            throw std::runtime_error("Invalid confirmed exchange timestamp");
        Time("RuneSchema.Completed",completed);Time("RuneSchema.Clock",completed);
        Write("RuneSchema.Phase",4);
        return true;
    }
    bool RecoverUnchargedRepeat(bool nativeComplete) const {
        if(!nativeComplete || Phase()!=2 || !quest.Repeat.Enabled || quest.StartCost
            || !quest.EntryOptions.empty() || Read("RuneSchema.Entry")!=0 || Run()<1
            || Time("RuneSchema.Completed")==0)return false;
        Write("RuneSchema.Phase",4);
        return true;
    }
    int EntryIndex()const {
        const auto entry=Read("RuneSchema.Entry");
        if(entry<0 || entry>static_cast<int>(quest.EntryOptions.size()))throw std::runtime_error("Saved quest entry selection invalid");
        return entry;
    }
    void MarkObjectiveSatisfied()const {
        if(Phase()!=1 || Time("RuneSchema.ObjectiveAt"))return;
        const auto now=Now();if(now<Time("RuneSchema.Started") || now<Time("RuneSchema.Clock"))throw std::runtime_error("Quest clock moved backwards");
        Time("RuneSchema.ObjectiveAt",now);
    }
    std::optional<int64_t> Elapsed()const {
        const auto start=Time("RuneSchema.Started"),end=Time("RuneSchema.ObjectiveAt");
        if(!start || !end)return std::nullopt;
        if(end<start)throw std::runtime_error("Invalid objective timestamp");
        return end-start;
    }
    template<class Ready,class Start,class Pay>
    QuestProgress::Result Accept(Ready ready,Start start,Pay pay,int entryIndex=0) const {
        using R=QuestProgress::Result;
        const auto phase=Phase();
        if(phase==1)return R::AlreadyActive;
        if(phase==2 || phase==3)return R::Uncertain;
        const auto now=Now();
        if(now<Time("RuneSchema.Clock"))throw std::runtime_error("Quest clock moved backwards");
        if(phase==4 && !RepeatReady(quest.Repeat,Time("RuneSchema.Completed"),now,Time("RuneSchema.Clock")))return R::AlreadyComplete;
        if(!ready())return R::NotReady;
        if(entryIndex<0 || entryIndex>static_cast<int>(quest.EntryOptions.size()) || (!quest.EntryOptions.empty() && !entryIndex))throw std::runtime_error("Quest requires an explicit entry selection");
        if(Run()==std::numeric_limits<int>::max())throw std::runtime_error("Quest run limit reached");
        Write("RuneSchema.Phase",2);
        Write("RuneSchema.Entry",entryIndex);Write("RuneSchema.Tier",0);Time("RuneSchema.ObjectiveAt",0);
        // Verify native reacceptance before charging entry items. An uncertain transition
        // stays pending and cannot receive credit/rewards or be charged automatically again.
        if(!start(phase==4))return R::Uncertain;
        if(!pay())return R::Uncertain;
        Write("RuneSchema.Run",Run()+1);Time("RuneSchema.Started",now);Time("RuneSchema.Clock",now);
        Write("RuneSchema.Phase",1);return R::Accepted;
    }
    template<class Ready,class Take,class Give,class Complete>
    QuestProgress::Result StageExchange(Ready ready,Take take,Give commit,Complete current) const {
        using R=QuestProgress::Result;
        if(Phase()==2 || Phase()==3)return R::Uncertain;
        if(Phase()!=1 || !current() || !ready())return R::NotReady;
        Write("RuneSchema.ExchangeStep",0);Write("RuneSchema.ExchangeKind",1);
        Write("RuneSchema.Phase",3);
        if(!take() || !current() || !commit())return R::Uncertain;
        Write("RuneSchema.ExchangeStep",1);
        Write("RuneSchema.Phase",1);return R::Accepted;
    }
    template<class Ready,class Take,class Give,class Complete>
    QuestProgress::Result TurnIn(Ready ready,Take take,Give give,Complete complete,int tierIndex=0) const {
        using R=QuestProgress::Result;
        const auto phase=Phase();
        if(phase==4)return R::AlreadyComplete;
        if(phase==2 || phase==3)return R::Uncertain;
        if(phase!=1 || !ready())return R::NotReady;
        if(tierIndex<0 || tierIndex>static_cast<int>(quest.ResultTiers.size()))throw std::runtime_error("Invalid reward tier");
        const auto now=Now();
        if(now<Time("RuneSchema.Clock"))throw std::runtime_error("Quest clock moved backwards");
        Write("RuneSchema.Tier",tierIndex);
        Write("RuneSchema.ExchangeStep",0);Write("RuneSchema.ExchangeKind",2);
        Time("RuneSchema.ExchangeAt",now);Write("RuneSchema.Phase",3);
        if(!take() || !give())return R::Uncertain;
        Write("RuneSchema.ExchangeStep",1);
        if(!complete())return R::Uncertain;
        Time("RuneSchema.Completed",now);Time("RuneSchema.Clock",now);
        Write("RuneSchema.Phase",4);return R::Completed;
    }
};
}
