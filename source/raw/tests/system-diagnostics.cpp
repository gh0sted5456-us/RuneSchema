#include "Core/SystemDiagnostics.h"
#include <cassert>
int main(){const auto rows=PS::SystemDiagnostics::Startup(std::filesystem::current_path(),std::filesystem::temp_directory_path());bool memory=false,privacy=false,storage=false;for(const auto& row:rows){assert(!row.empty()&&row.size()<2048&&row.find('\n')==row.npos);memory|=row.starts_with("RAM:");storage|=row.find("storage:")!=row.npos;privacy|=row.starts_with("Privacy:");}assert(memory&&privacy&&storage);}
