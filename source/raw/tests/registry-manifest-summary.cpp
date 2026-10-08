#include <stdexcept>
#include <string>
#include "Runtime/RegistryManifestSummary.h"

int main()
{
    using namespace PS::RegistryManifestSummary;
    const std::string local=R"({"entries":[{"owner":"Bard","key":"Bard:note"},{"owner":"Flintlock","key":"Flintlock:shot"}]})";
    const std::string authority=R"({"entries":[{"owner":"Bard","key":"Bard:note2"},{"owner":"Summoning","key":"Summoning:cast"}]})";
    const auto difference=Difference(Owners(local),Owners(authority));
    if(difference.find("different=Bard")==std::string::npos
        || difference.find("client-only=Flintlock")==std::string::npos
        || difference.find("authority-only=Summoning")==std::string::npos)
        throw std::runtime_error("Registry manifest difference did not identify the affected mods");

    const auto session=Negotiate(Owners(local),Owners(authority));
    if(session.Exact() || session.Matched.size()!=0 || session.Different.size()!=1
        || session.ClientOnly.size()!=1 || session.AuthorityOnly.size()!=1
        || session.Summary()!="matched=0, changed=1, client-extras=1, unavailable-locally=1")
        throw std::runtime_error("Permissive session negotiation did not classify the manifests");

    const auto exact=Negotiate(Owners(local),Owners(local));
    if(!exact.Exact() || exact.Matched.size()!=2)
        throw std::runtime_error("Exact session negotiation was not accepted");
}
