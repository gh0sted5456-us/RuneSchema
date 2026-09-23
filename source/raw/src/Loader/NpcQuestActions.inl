#include "Loader/TimeOfDayRuntime.h"
std::string DragonWildsNpcLoader::RunQuestAction(const DialogueCompletionBinding& action,UObject* controller,const std::function<bool()>& current) {
    if(action.QuestAction!="Accept" && action.QuestAction!="TurnIn" && action.QuestAction!="Abandon")throw std::runtime_error("Unsupported quest action");
    if(!controller || !current())throw std::runtime_error("Quest action requires a current player");
    auto* player=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
    if(!player || !player->IsA<AActor>() || player->GetWorld()!=controller->GetWorld())throw std::runtime_error("Quest action pawn is unavailable");
    ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())throw std::runtime_error("Quest action requires authority");
    const auto character=DialogueCharacter(controller);
    PrepareQuests(controller);
    const auto& document=m_quests.Document(action.QuestKey);
    const auto mod=action.QuestKey.substr(0,action.QuestKey.find(':'));
    const auto& quest=m_quests.Find("_",action.QuestKey);
    auto* asset=m_quests.Asset(action.QuestKey);
    const QuestNative::Adapter native(controller,asset);native.ValidateAll();
    if(quest.Kill) {
        native.ValidateCounters();
        auto* aiBase=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        for(const auto& path:quest.Kill->Classes){auto* type=ActorHelper::ResolveClass(RC::to_generic_string(path));if(!type || !aiBase || !type->IsChildOf(aiBase))throw std::runtime_error("Kill objective AI class unavailable");}
        if(!quest.Kill->EventKey.empty())m_events.Require(quest.Kill->EventKey);
        if(!quest.Kill->SpawnKey.empty())m_events.RequireSpawn(quest.Kill->EventKey,quest.Kill->SpawnKey);
    }
    auto* itemClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
    std::vector<std::string> questItems{quest.Required.Item,quest.Reward.Item};
    if(quest.RepeatReward)questItems.push_back(quest.RepeatReward->Item);
    for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives) {
        if(!objective.Kill)questItems.push_back(objective.Required.Item);
        else {
            auto* aiBase=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
            for(const auto& path:objective.Kill->Classes){auto* type=ActorHelper::ResolveClass(RC::to_generic_string(path));if(!type || !aiBase || !type->IsChildOf(aiBase))throw std::runtime_error("Stage AI class unavailable");}
            if(!objective.Kill->EventKey.empty())m_events.Require(objective.Kill->EventKey);
            if(!objective.Kill->SpawnKey.empty())m_events.RequireSpawn(objective.Kill->EventKey,objective.Kill->SpawnKey);
        }
    }
    if(quest.StartCost)questItems.push_back(quest.StartCost->Item);
    for(const auto& entry:quest.EntryOptions)questItems.push_back(entry.Cost.Item);
    for(const auto& tier:quest.ResultTiers)questItems.push_back(tier.Reward.Item);
    for(const auto& path:questItems) {
        if(path.empty() && (quest.Kill || !quest.Stages.empty()))continue;
        auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path));
        if(!item || !itemClass || !item->IsA(itemClass))throw std::runtime_error("Quest item could not be resolved before interaction");
    }
    const auto state=[&]{auto name=RC::to_string(native.StateName());const auto colon=name.rfind("::");return colon==name.npos?name:name.substr(colon+2);};
    const auto progress=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
    const Quests::NativeReceipt receipt(native,quest,document,progress,character);
    if(current() && receipt.RecoverConfirmedExchange(state()))
        PS::Log<LogLevel::Verbose>(STR("Quest '{}' recovered a confirmed exchange without replaying inventory operations.\n"),RC::to_generic_string(quest.Key));
    if(action.QuestAction=="Abandon") {
        if(receipt.Phase()!=1 || state()!="Given")return "This quest is not active.";
        // Abandon is intentionally destructive only to the active RuneSchema
        // run. It never removes inventory or grants rewards. Clear every
        // authored counter before returning the owned native record to
        // Ungiven so no stage, marker, or hidden objective survives a retry.
        if(quest.Kill || quest.Acquire)
            native.SetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add),0);
        for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
            native.SetInt(Quests::StageCounter(objective),0);
        const auto write=[&](const char* key,int value){native.SetInt(FName(RC::to_generic_string(key).c_str(),FNAME_Add),value);};
        const auto clearTime=[&](const char* key){write((std::string(key)+"Lo").c_str(),0);write((std::string(key)+"Hi").c_str(),0);};
        write("RuneSchema.Entry",0);write("RuneSchema.Tier",0);write("RuneSchema.ExchangeStep",0);write("RuneSchema.ExchangeKind",0);
        clearTime("RuneSchema.Started");clearTime("RuneSchema.Completed");clearTime("RuneSchema.ObjectiveAt");clearTime("RuneSchema.ExchangeAt");
        write("RuneSchema.Run",0);
        native.SetObjective(FName(TEXT("__abandoned"),FNAME_Add));
        native.CancelForRecovery();
        write("RuneSchema.Phase",0);
        QueueQuestRefresh(controller,quest.Key);
        ReconcileQuestLocations(controller);
        return "Quest abandoned. Its active progress and locations were reset.";
    }
    QuestProgress::Result result;
    const auto selectedTier=[&]{const auto index=receipt.EntryIndex();return Quests::SelectRewardTier(quest,index?quest.EntryOptions.at(index-1).Id:std::string{},receipt.Elapsed(),[&](const std::string& id){
        for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
            if(objective.ObjectiveId==id)return native.GetInt(Quests::StageCounter(objective))==objective.Required.Count;
        return false;
    });};
    if(action.QuestAction=="Accept") {
        if(!TimeOfDay::Allows(controller,quest.Time))
            return std::string("This quest can only be accepted during the ")+(quest.Time==TimeOfDay::Requirement::Day?"day.":"night.");
        for(const auto& prerequisite:quest.Prerequisites) {
            const QuestNative::Adapter parent(controller,m_quests.Asset(prerequisite));
            auto name=RC::to_string(parent.StateName());const auto colon=name.rfind("::");if(colon!=name.npos)name=name.substr(colon+2);
            if(name!="Complete")return "Complete the prerequisite quests first.";
        }
        auto cost=quest.StartCost;int entryIndex=0;
        if(!quest.EntryOptions.empty()) {
            for(size_t i=0;i<quest.EntryOptions.size();++i)if(quest.EntryOptions[i].Id==action.QuestEntry){cost=quest.EntryOptions[i].Cost;entryIndex=static_cast<int>(i)+1;}
            if(!entryIndex)throw std::runtime_error("Choose a valid quest entry option");
        } else if(!action.QuestEntry.empty())throw std::runtime_error("Quest has no entry options");
        UObject* costItem=nullptr;
        if(cost) {
            costItem=ActorHelper::ResolveObject(RC::to_generic_string(cost->Item));
            auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
            if(!costItem || !itemType || !costItem->IsA(itemType))throw std::runtime_error("Quest start-cost item is unavailable");
        }
        if(current())receipt.RecoverUnchargedRepeat(state()=="Complete");
        result=receipt.Accept([&]{return current() && (!cost || DialogueInventory(controller,costItem).Count()>=cost->Count);},[&](bool repeating){
            if(!current() || (repeating?state()!="Complete":(state()=="Complete" || state()=="Given")))return false;
            const auto prepare=[&] {
                if(!current())throw std::runtime_error("World changed before quest preparation");
                if(quest.Kill || quest.Acquire)native.SetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add),0);
                ObserveQuestInventory(controller,false);
                if(!quest.Stages.empty()) {
                    auto staged=Quests::StageProgress(native,quest,receipt.Run()+1);staged.BeginRun();
                    Quests::SetStageText(native,quest,staged.ActiveStage());
                } else native.SetObjective(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add));
                if(!current())throw std::runtime_error("World changed during quest preparation");
            };
            if(repeating) {
                if(!native.RestartCompleted(prepare))return false;
            }
            else {prepare();native.SetGiven(false);}
            // Preserve the game's native first-accept notification. Adding a
            // second whole-registry RPC here races the native toast and can
            // suppress it. Repeat acceptance already refreshes inside
            // RestartCompleted; objective/stage updates use the queued path.
            return current() && native.IsInitialized() && state()=="Given";
        },[&]{return current() && (!cost || DialogueInventory(controller,costItem).Take(cost->Count,current));},entryIndex);
    } else if(!quest.Stages.empty()) {
        if(receipt.Phase()==2 || receipt.Phase()==3)result=QuestProgress::Result::Uncertain;
        else if(receipt.Phase()==4)result=QuestProgress::Result::AlreadyComplete;
        else if(receipt.Phase()!=1 || state()!="Given")result=QuestProgress::Result::NotReady;
        else {
            auto staged=Quests::StageProgress(native,quest,receipt.Run());
            const auto active=staged.ActiveStage();
            result=QuestProgress::Result::NotReady;
            if(active<quest.Stages.size()) {
                std::map<std::string,int> amounts;std::vector<const Quests::Definition*> handIns;
                const auto position=ActorHelper::GetActorLocation(static_cast<AActor*>(player));
                bool inArea=true;
                for(bool optional:{false,true})for(size_t i=0;i<quest.Stages[active].second.size();++i) {
                    const auto& objective=quest.Stages[active].second[i];
                    if(objective.Optional!=optional)continue;
                    if(objective.Kill || objective.Acquire || staged.Count(active,i)==objective.Required.Count)continue;
                    if(objective.Optional && DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(objective.Required.Item))).Count()<amounts[objective.Required.Item]+objective.Required.Count)continue;
                    if(objective.Marker && objective.Marker->RadiusMeters) {
                        const Quests::Stages::Area area{objective.Marker->Position,*objective.Marker->RadiusMeters};
                        if(!area.Contains({position.X(),position.Y(),position.Z()})) {
                            if(objective.Optional)continue;
                            inArea=false;
                        }
                    }
                    amounts[objective.Required.Item]+=objective.Required.Count;
                    handIns.push_back(&objective);
                }
                if(!handIns.empty())result=receipt.StageExchange([&]{
                    if(!inArea)return false;
                    for(const auto& [path,count]:amounts)if(DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(path))).Count()<count)return false;
                    return true;
                },[&]{
                    for(const auto& [path,count]:amounts)if(!DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(path))).Take(count,current))return false;
                    return true;
                },[&]{
                    for(const auto* objective:handIns) {
                        const auto counter=Quests::StageCounter(*objective);native.SetInt(counter,objective->Required.Count);
                        if(native.GetInt(counter)!=objective->Required.Count)return false;
                    }
                    Quests::SetStageText(native,quest,staged.ActiveStage());return current();
                },current);
            }
            if(receipt.Phase()==1 && staged.Complete()) {
                receipt.MarkObjectiveSatisfied();const auto tierIndex=selectedTier();
                const auto& spec=Quests::RewardForRun(quest,receipt.Run(),tierIndex);
                const DialogueInventory rewardInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(spec.Item)));
                result=receipt.TurnIn([&]{return current() && state()=="Given" && TimeOfDay::Allows(controller,spec.Time) && rewardInventory.Ready(spec.Count);},current,
                    [&]{return rewardInventory.Give(spec.Count,current);},
                    [&]{if(!current())return false;native.SetComplete(false);return current() && state()=="Complete";},tierIndex);
            }
        }
    } else if(quest.Kill || quest.Acquire) {
        const FName counter(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add);
        if(native.GetInt(counter)==quest.Required.Count)receipt.MarkObjectiveSatisfied();
        const auto tierIndex=selectedTier();
        const auto& rewardSpec=Quests::RewardForRun(quest,receipt.Run(),tierIndex);
        auto* reward=ActorHelper::ResolveObject(RC::to_generic_string(rewardSpec.Item));
        const DialogueInventory rewardInventory(controller,reward);
        if(receipt.Phase()==1 && state()!="Given")
            throw std::runtime_error("Native kill quest state did not match its receipt; refusing reward");
        result=receipt.TurnIn(
            [&]{return current() && state()=="Given" && native.GetInt(counter)==quest.Required.Count && TimeOfDay::Allows(controller,rewardSpec.Time) && rewardInventory.Ready(rewardSpec.Count);},
            [&]{return current();},
            [&]{return rewardInventory.Give(rewardSpec.Count,current);},
            [&]{if(!current())return false;native.SetComplete(false);return current() && state()=="Complete";},tierIndex);
    } else {
        auto* required=ActorHelper::ResolveObject(RC::to_generic_string(quest.Required.Item));
        const DialogueInventory inventory(controller,required);
        if(inventory.Count()>=quest.Required.Count)receipt.MarkObjectiveSatisfied();
        const auto tierIndex=selectedTier();
        const auto& rewardSpec=Quests::RewardForRun(quest,receipt.Run(),tierIndex);
        auto* reward=ActorHelper::ResolveObject(RC::to_generic_string(rewardSpec.Item));
        auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        if(!required || !reward || !itemType || !required->IsA(itemType) || !reward->IsA(itemType))throw std::runtime_error("Quest item could not be resolved");
        const DialogueInventory rewardInventory(controller,reward);
        if(receipt.Phase()==1 && state()!="Given")
            throw std::runtime_error("Native quest state did not match the active receipt after loading; refusing hand-in");
        result=receipt.TurnIn(
            [&]{return current() && state()=="Given" && inventory.Count()>=quest.Required.Count && TimeOfDay::Allows(controller,rewardSpec.Time) && rewardInventory.Ready(rewardSpec.Count);},
            [&]{return inventory.Take(quest.Required.Count,current);},
            [&]{return rewardInventory.Give(rewardSpec.Count,current);},
            [&]{if(!current())return false;native.SetComplete(false);return current() && state()=="Complete";},tierIndex);
    }
    if(!current())throw std::runtime_error("Player or world changed during quest action");
    if(action.QuestAction=="Accept" && (result==QuestProgress::Result::Accepted || result==QuestProgress::Result::AlreadyActive)) {
        // Give the owning client one deferred coherent refresh after the native
        // acceptance toast. This populates the initial objective and AOI on
        // first accept without racing the game's own notification.
        // Present the supplemental toast after the same deferred native refresh
        // that makes the first objective and AOI visible. During dialogue the
        // HUD can legitimately suppress a text notification sent synchronously.
        QueueQuestRefresh(controller,quest.Key,0.75,result==QuestProgress::Result::Accepted?"Quest accepted: "+quest.Title:std::string{});
        QueueQuestLocationRefresh(controller);
    }
    ReconcileQuestLocations(controller);
    if(!quest.Stages.empty() && receipt.Phase()==1) {
        auto staged=Quests::StageProgress(native,quest,receipt.Run());
        const auto active=staged.ActiveStage();std::string message;
        if(active==quest.Stages.size())message="Objectives complete. Leave room for your reward and speak to me again.";
        else for(size_t i=0;i<quest.Stages[active].second.size();++i) {
            const auto& objective=quest.Stages[active].second[i];
            if(objective.Hidden)continue;
            if(!message.empty())message+="\n";
            message+=objective.ObjectiveText+" ("+std::to_string(staged.Count(active,i))+"/"+std::to_string(objective.Required.Count)+")";
        }
        return message;
    }
    if(quest.Kill && (result==QuestProgress::Result::NotReady || result==QuestProgress::Result::AlreadyActive)) {
        const FName counter(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add);
        return "Confirmed kills: "+std::to_string(native.GetInt(counter))+"/"+std::to_string(quest.Required.Count)+". Accept the quest before hunting, and leave room for your reward.";
    }
    if(quest.Acquire && (result==QuestProgress::Result::NotReady || result==QuestProgress::Result::AlreadyActive)) {
        const FName counter(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add);
        return "Items acquired: "+std::to_string(native.GetInt(counter))+"/"+std::to_string(quest.Required.Count)+". Acquire items after accepting, inside any required area.";
    }
    if(result==QuestProgress::Result::Uncertain)
        PS::Log<LogLevel::Verbose>(STR("Quest recovery pending: quest='{}', phase={}, run={}, native='{}'. No inventory replay attempted.\n"),
            RC::to_generic_string(quest.Key),receipt.Phase(),receipt.Run(),RC::to_generic_string(state()));
    const auto text=result==QuestProgress::Result::NotReady?"Accept the quest first, bring the requested items, and make room for your reward.":
        result==QuestProgress::Result::Uncertain?"This quest exchange needs recovery checks before it can be repeated.":
        result==QuestProgress::Result::AlreadyComplete?(quest.Repeat.Enabled?"This run is complete. Accept again when its repeat schedule allows.":"You have already completed this quest and claimed its reward."):
        result==QuestProgress::Result::AlreadyActive?"You already have this quest. Bring the requested items when you are ready.":action.SuccessText;
    return text;
}
