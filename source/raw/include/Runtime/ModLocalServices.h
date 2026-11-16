#pragma once
#include "RuneSchemaModApi.h"
#include <string>
namespace PS::ModLocalServices {
void Bind(const std::string& plugin,const std::string& owner);
const RuneSchemaModApiV1* Api(const char* plugin);
bool Drop(const std::string& plugin) noexcept;
void Start();
void Stop() noexcept;
}
