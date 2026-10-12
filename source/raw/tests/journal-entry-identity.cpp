#include "Loader/JournalEntryIdentity.h"
#include <stdexcept>

using nlohmann::json;
using DragonWilds::JournalEntryIdentity::PersistenceId;

static void Require(bool condition) { if(!condition)throw std::runtime_error("journal identity contract failed"); }
static void Reject(const json& value) {
    bool rejected=false;
    try {(void)PersistenceId("Dye:journal_dye_workshop",value);}catch(const std::exception&) {rejected=true;}
    Require(rejected);
}
int main() {
    constexpr auto key="Dye:journal_dye_workshop";
    constexpr auto canonical="ZP7lWkrcu3afzZKyFWHiGg";
    Require(PersistenceId(key,json::object())==key); // No silent migration of existing saves.
    Require(PersistenceId(key,{{"PersistenceID",canonical}})==canonical);
    Reject({{"PersistenceID",key}});
    Reject({{"PersistenceID",42}});
    Reject({{"PersistenceID","AAAAAAAAAAAAAAAAAAAAAB"}});
}
