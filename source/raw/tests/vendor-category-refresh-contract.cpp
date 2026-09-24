#include <fstream>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("vendor category contract source unavailable");
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("vendor loader and native refresh sources required");
    const auto loader=Read(argv[1]);
    const auto refresh=Read(argv[2]);
    const auto require=[](bool value){if(!value)throw std::runtime_error("vendor category floating regression");};
    require(loader.find("VendorCategoryGate::Filter")!=std::string::npos);
    require(loader.find("VendorOffers::Category(item)")!=std::string::npos);
    require(loader.find("rowItems.push_back({{\"Label\",category},{\"Collection\",nlohmann::json::array()}})")!=std::string::npos);
    require(loader.find("VendorCategoryText::WriteGroups")!=std::string::npos);
    require(loader.find("VendorCategoryText::VerifyGroups")!=std::string::npos);
    require(refresh.find("cachedArray.Empty()")!=std::string::npos);
    require(refresh.find("validArray.Empty()")!=std::string::npos);
    require(refresh.find("groups->Identical(source,labels)")!=std::string::npos);
}
