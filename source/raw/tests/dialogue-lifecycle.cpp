#include "Loader/DialogueGraphLifecycle.h"
#include "Loader/DialogueGraphIdentity.h"
#include <cassert>
#include <unordered_map>
#include <memory>

int main() {
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
