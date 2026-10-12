#include "Loader/DialogueGraphLifecycle.h"
#include "Loader/DialogueGraphIdentity.h"
#include "Loader/DialogueCompletionPresentation.h"
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <memory>

int main() {
    using namespace DragonWilds::DialogueCompletionPresentation;
    const auto kit=MakeRoute(0,"business"), unrelated=MakeRoute(1,"business");
    assert(kit.Response!=unrelated.Response);
    assert(kit.AuthoredNext=="text:business" && unrelated.AuthoredNext==kit.AuthoredNext);
    assert(kit.ContinueBranch!=kit.AuthoredNext && kit.ContinueOption!=kit.ContinueBranch);
    assert(std::string_view(Message(Outcome::Pending))==Message(Outcome::Failed));
    assert(std::string_view(Message(Outcome::Granted))==Message(Outcome::AlreadyComplete));
    assert(std::string_view(Message(Outcome::Granted))!=Message(Outcome::Pending));
    assert(std::string_view(Message(Outcome::NotReady))!=Message(Outcome::Granted));
    assert(std::string_view(Message(Outcome::Uncertain))!=Message(Outcome::Granted));
    // Every new session starts conservative, independent of the last outcome.
    const auto previous=Message(Outcome::Granted);
    const auto repeated=Message(Outcome::Pending);
    assert(std::string_view(previous)!=repeated);
    assert(std::string_view(repeated).find("cabbage")==std::string_view::npos);
    using namespace DragonWilds::DialogueIdentity;
    const nlohmann::json data={{"Entry","hello"},{"Nodes",{{"hello",{{"Text","Hi"}}}}}};
    const auto key=GraphKey("mod","npc","mod:story","player-one",false,data);
    const auto id=Node(key,"text:hello");
    assert(id==Node(GraphKey("mod","npc","mod:story","player-one",false,nlohmann::json::parse(data.dump())),"text:hello"));
    assert(id!=Node(key,"choice:hello:leave"));
    assert(key!=GraphKey("mod","npc","mod:story","player-two",false,data));
    assert(key!=GraphKey("mod","npc","mod:story","player-one",true,data));
    assert(key!=GraphKey("other","npc","mod:story","player-one",false,data));
    assert(key!=GraphKey("mod","other","mod:story","player-one",false,data));
    auto changed=data;changed["Nodes"]["hello"]["Text"]="Changed";
    assert(id!=Node(GraphKey("mod","npc","mod:story","player-one",false,changed),"text:hello"));
    bool rejected=false;try{GraphKey("mod","npc","story","",false,data);}catch(const std::exception&){rejected=true;}assert(rejected);
    using namespace DragonWilds::DialogueLifecycle;
    int first=0,other=0;
    assert(Matches(&first,&first,3,3,true,true));
    assert(!Matches(&first,&first,3,4,true,true));
    assert(!Matches(&first,&first,3,3,false,true));
    assert(!Matches(&first,&first,3,3,true,false));
    assert(!Matches(&first,&other,3,3,true,true));
    assert(!Matches(nullptr,nullptr,0,0,true,true));
    struct Lease {void* Graph;};
    struct Session {void* Graph=nullptr;void* Instance=nullptr;};
    std::unordered_map<int,Lease> graphs{{2,{&other}}},actions{{2,{&other}}};
    Session session;
    // Repeated reclamation clears owned callbacks/session without touching another graph.
    for(int i=0;i<100;++i) {
        graphs[1]={&first};actions[1]={&first};actions[3]={&first};
        session={&first,&first};
        Retire(graphs,graphs.find(1),actions,session);
        assert(graphs.size()==1 && graphs.at(2).Graph==&other);
        assert(actions.size()==1 && actions.at(2).Graph==&other);
        assert(!session.Graph && !session.Instance);
    }
    graphs[1]={&first};session={&other,&other};
    Retire(graphs,graphs.find(1),actions,session);
    assert(session.Graph==&other && session.Instance==&other);
    std::unordered_map<int,std::shared_ptr<Session>> sessions;
    auto firstPlayer=std::make_shared<Session>(Session{&first,&first});
    auto secondPlayer=std::make_shared<Session>(Session{&other,&other});
    sessions[1]=firstPlayer;sessions[2]=secondPlayer;
    graphs[1]={&first};
    actions[1]={&first};actions[2]={&other};
    RetireSessions(graphs,graphs.find(1),actions,sessions);
    assert(sessions.size()==1 && sessions.at(2)==secondPlayer);
    assert(actions.size()==1 && actions.at(2).Graph==&other);
    // An in-flight callback owns its session even if travel retires the map.
    assert(firstPlayer->Instance==&first);
    sessions.clear();assert(secondPlayer->Instance==&other);
}
