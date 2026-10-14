#include "Loader/JournalPlayerSelection.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

static void Require(bool ok) {
    if(!ok)throw std::runtime_error("Journal player selection regression");
}
int main(int argc,char** argv) {
    using DragonWilds::JournalPlayerSelection::Selection;
    int players[]={1,2,3};
    std::array<int,3> order={0,1,2};
    do {
        Selection<int> selection;
        for(auto i:order)selection.Observe(&players[i],i==2);
        Require(selection.Result(false)==&players[2]);
        Require(selection.Result(true)==&players[2]);
    }while(std::next_permutation(order.begin(),order.end()));
    Selection<int> absent;
    absent.Observe(&players[0],false);
    Require(absent.Result(false)==nullptr); // A server must not guess a player.
    Require(absent.Result(true)==&players[0]); // Unique standalone fallback.
    absent.Observe(&players[1],false);
    Require(absent.Result(true)==nullptr);
    Selection<int> ambiguous;
    ambiguous.Observe(&players[0],true);
    ambiguous.Observe(&players[1],true);
    Require(ambiguous.Result(false)==nullptr);
    Require(ambiguous.Result(true)==nullptr);
    Selection<int> duplicates;
    duplicates.Observe(nullptr,false);
    duplicates.Observe(&players[0],true);
    duplicates.Observe(&players[0],true);
    Require(duplicates.Result(false)==&players[0]);
    Require(argc==2);
    std::ifstream input(argv[1],std::ios::binary);
    Require(input.good());
    std::ostringstream stream;stream<<input.rdbuf();
    const auto source=stream.str();
    Require(source.find("JournalPlayerSelection::Selection<UObject> selection")!=std::string::npos);
    Require(source.find("selection.Observe(object,owned)")!=std::string::npos);
    Require(source.find("return selection.Result(allowUniqueWorld)")!=std::string::npos);
    Require(source.find("context.Mode==PS::Network::Role::Standalone")!=std::string::npos);
    Require(source.find("(pawn && outer==pawn)")!=std::string::npos);
    Require(source.find("(state && outer==state)")!=std::string::npos);
    Require(source.find("JournalSave::SelectTransient(owned,m_saveTransientIds)")!=std::string::npos);
    Require(source.find("JournalSave::IsSaveTransient(m_createdEntries.contains(entry),automatic)")!=std::string::npos);
}
