#include "Loader/DialogueProgress.h"
#include <cassert>
#include <chrono>
using namespace DragonWilds::DialogueProgress;
template<class F> bool Rejects(F call){try{call();}catch(const std::exception&){return true;}return false;}
int main() {
    const auto folder=std::filesystem::temp_directory_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=folder/"progress.json";
    const std::string character="11111111222222223333333344444444",flag="Test:heard";
    int grants=0;
    const auto give=[&]{++grants;return true;};
    assert(!HasFlag(path,character,flag));
    assert(Complete(path,character,flag,[]{return false;},give)==Result::NotReady);
    assert(grants==0 && !std::filesystem::exists(path));
    assert(Complete(path,character,flag,[]{return true;},[]{return false;})==Result::NotReady);
    assert(!HasFlag(path,character,flag));
    assert(Complete(path,character,flag,[]{return true;},give)==Result::Granted);
    assert(HasFlag(path,character,flag));
    assert(Complete(path,character,flag,[]{return true;},give)==Result::AlreadyComplete && grants==1);
    assert(Rejects([&]{Read(path,"99999999222222223333333344444444");}));
    assert(Rejects([&]{Complete(path,character,"Test:uncertain",[]{return true;},[]()->bool{throw std::runtime_error("native failure");});}));
    assert(Complete(path,character,"Test:uncertain",[]{return true;},give)==Result::Uncertain && grants==1);
    assert(!HasFlag(path,character,"Test:uncertain"));
    const auto other=folder/"other.json";
    assert(Complete(other,"99999999222222223333333344444444",flag,[]{return true;},give)==Result::Granted && grants==2);
    PS::ConfigFiles::Write(path,"corrupt");
    assert(Rejects([&]{Complete(path,character,flag,[]{return true;},give);}));
    assert(PS::ConfigFiles::Read(path)=="corrupt" && grants==2);
    std::filesystem::remove(path);std::filesystem::remove(other);std::filesystem::remove(folder);
}
