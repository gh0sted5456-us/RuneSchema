// Constructor bindings for authority-owned, quest-bound encounters.
m_events.BeginContract=[this](UObject* controller,const Events::Definition& event) {
    if(!event.Contract || !PS::PSConfig::Get()->GetSettings().persistence.quests)
        throw std::runtime_error("Quest saving must be enabled for contract encounters");
    ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())throw std::runtime_error("Quest contracts require world authority");
    PrepareQuests(controller);
    const auto& quest=m_quests.Find("_",event.Contract->Quest);
    Events::ValidateContractQuest(event,quest);
    m_events.RequireSpawn(event.Key,quest.Kill->SpawnKey);
    const QuestNative::Adapter native(controller,m_quests.Asset(quest.Key));native.ValidateAll();native.ValidateCounters();
    auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
    const auto character=DialogueCharacter(controller);
    const auto progress=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
    const Quests::NativeReceipt receipt(native,quest,m_quests.Document(quest.Key),progress,character);
    if(!native.IsInitialized() || state!="Given" || receipt.Phase()!=1 || receipt.Run()<=0)
        throw std::runtime_error("Accept this contract before activating its encounter");
    if(native.GetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add))!=0)
        throw std::runtime_error("This contract already has victory credit; claim its reward before repeating");
    const auto attempt=native.GetInt(FName(TEXT("RuneSchema.ContractAttempt"),FNAME_Add));
    if(attempt<=0)throw std::runtime_error("Re-accept this contract to create a current encounter attempt");
    return attempt;
};
m_events.ContractCurrent=[this](UObject* controller,const Events::Definition& event,int expectedRun) {
    if(!controller || !event.Contract || expectedRun<=0 || !PS::PSConfig::Get()->GetSettings().persistence.quests)return false;
    const auto& quest=m_quests.Find("_",event.Contract->Quest);
    const QuestNative::Adapter native(controller,m_quests.Asset(quest.Key));
    auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
    return native.IsInitialized() && state=="Given"
        && native.GetInt(FName(TEXT("RuneSchema.Phase"),FNAME_Add))==1
        && native.GetInt(FName(TEXT("RuneSchema.ContractAttempt"),FNAME_Add))==expectedRun
        && native.GetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add))==0;
};
m_events.CompleteContract=[this](UObject* controller,const Events::Definition& event,int expectedRun) {
    if(!m_events.ContractCurrent(controller,event,expectedRun))throw std::runtime_error("Accepted quest run changed during the encounter");
    const auto& quest=m_quests.Find("_",event.Contract->Quest);
    const QuestNative::Adapter native(controller,m_quests.Asset(quest.Key));
    const auto character=DialogueCharacter(controller);
    const auto progress=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
    const Quests::NativeReceipt receipt(native,quest,m_quests.Document(quest.Key),progress,character);
    native.SetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add),1);
    receipt.MarkObjectiveSatisfied();
    native.SetObjective(FName(TEXT("__ready"),FNAME_Add));
    QueueQuestRefresh(controller,quest.Key);
};
