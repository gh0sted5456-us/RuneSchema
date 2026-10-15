#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing discovery contract source");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==3,"Expected recipe loader and native discovery adapter");
    const auto recipe=Read(argv[1]),native=Read(argv[2]);
    Check(recipe.contains("ApplyDiscovery();")&&recipe.contains("NativeRecipeDiscovery::Apply(requests,")
        &&recipe.contains("propertyName == \"Discovery\""),"Recipe discovery was not integrated as loader metadata");
    Check(recipe.contains("Discovery changes require a full restart")&&recipe.contains("RegistryEnrollment::Reject(\"recipe Discovery\""),"Late/incomplete discovery changes must fail safely");
    Check(native.contains("ProgressionBundleRow")&&native.contains("UnlockMatchQuery")&&native.contains("ReflectedContract::Fits"),"Discovery must use verified native progression layouts");
    Check(native.contains("RetainOtherRecipes(before,replaced)")&&native.contains("row->CopyScriptStruct(change.After->GetData(),source)"),"An override must preserve unrelated outputs and native row fields");
    Check(native.contains("while(committed)")&&native.contains("Identical(row,actual,change.After->GetData())"),"Native transaction needs verified readback and rollback");
    Check(native.contains("std::map<std::string,const Request*>")&&native.contains("Conflicting recipe Discovery policies"),"Duplicate/conflicting target recipes must not choose an arbitrary policy");
    Check(!native.contains("RecipesUnlocked\"")&&!native.contains("NewRecipes\"")&&!native.contains("RegisterEngineTick")&&!native.contains("std::thread"),"Discovery must not force-write learned state or add a polling rule engine");
}
