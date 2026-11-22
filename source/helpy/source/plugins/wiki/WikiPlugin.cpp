// RuneSchema.Wiki, Helpy half: a "Wiki" tab that shows the RuneScape: Dragonwilds Wiki
// in a host-owned web page. "rs wiki <text>" in the game console reaches this tab
// through the plugin's in-game half (WikiGamePlugin.cpp) and an owned plugin event.
#include "Core/HelpyBranding.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "WikiLinks.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shellapi.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace {
using Json = nlohmann::json;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor), RUNESCHEMA_PLUGIN_API_VERSION, "RuneSchema.Wiki", "Dragonwilds Wiki", PS::HelpyBranding::Version};
constexpr const char* Route = "RuneSchema.Wiki.wiki";

std::mutex Gate;          // the events service runs on Helpy's bridge thread, Draw on its UI thread
std::string Navigate;     // next page to open in the tab
uint64_t NavigateId = 0;  // bumped for every new request
std::array<char, 256> Search{};

void Request(const std::string& query) {
    std::lock_guard lock(Gate);
    Navigate = PS::WikiLinks::Lookup(query);
    ++NavigateId;
}

int32_t RS_PLUGIN_CALL Events(void*, const char* input, char*, uint32_t, uint32_t* size) {
    if (size) *size = 0;
    if (!input || strnlen_s(input, 64 * 1024 + 1) > 64 * 1024) return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        const auto event = Json::parse(input);
        if (event.value("Event", std::string{}) != "Lookup") return RS_PLUGIN_OK; // EventsLost and others: nothing to do
        const auto& payload = event.at("Payload");
        Request(payload.value("Query", std::string{}));
        return RS_PLUGIN_OK;
    } catch (...) { return RS_PLUGIN_INVALID_ARGUMENT; }
}

int32_t RS_PLUGIN_CALL Draw(void*, const RuneSchemaNavUiV1* ui) {
    if (!ui || !ui->Text || !ui->Button) return RS_PLUGIN_INVALID_ARGUMENT;
    const bool web = ui->StructSize >= offsetof(RuneSchemaNavUiV1, WebPage) + sizeof(ui->WebPage) && ui->WebPage && ui->InputText;
    if (!web) {
        ui->Text(ui->Context, "This Helpy is too old to show web pages. Update RuneSchema, or open the wiki in your browser.");
        if (ui->Button(ui->Context, "Open the wiki in your browser")) ShellExecuteW(nullptr, L"open", L"https://dragonwilds.runescape.wiki/", nullptr, nullptr, SW_SHOWNORMAL);
        return RS_PLUGIN_OK;
    }
    const bool entered = ui->InputText(ui->Context, "Search the wiki", Search.data(), static_cast<uint32_t>(Search.size())) == 1;
    if (ui->Button(ui->Context, "Search") || entered) Request(Search.data());
    ui->Text(ui->Context, "Content from the RuneScape: Dragonwilds Wiki (CC BY-NC-SA 3.0). Tip: type rs wiki <name> in the game console.");
    std::string navigate; uint64_t id = 0;
    { std::lock_guard lock(Gate); navigate = Navigate; id = NavigateId; }
    const std::string origin(PS::WikiLinks::Origin), home(PS::WikiLinks::Home);
    const RuneSchemaWebPageV1 page{sizeof(RuneSchemaWebPageV1), origin.c_str(), home.c_str(), navigate.empty() ? nullptr : navigate.c_str(), id};
    ui->WebPage(ui->Context, &page);
    return RS_PLUGIN_OK;
}
}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept { return &Descriptor; }
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host, void** instance) noexcept {
    if (!host || !instance || host->StructSize < offsetof(RuneSchemaHostApi, RegisterNavTab) + sizeof(host->RegisterNavTab) || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION
        || !host->RegisterService || !host->RegisterCapability || !host->CallService || !host->RegisterNavTab) return RS_PLUGIN_INCOMPATIBLE_API;
    *instance = nullptr;
    try {
        char identity[256]{}; uint32_t size = 0;
        if (host->CallService(Descriptor.Id, "helpy.host", "{}", identity, sizeof(identity), &size) != RS_PLUGIN_OK || Json::parse(identity).value("Process", std::string{}) != "Helpy") return RS_PLUGIN_INCOMPATIBLE_API;
        if (host->RegisterCapability(Descriptor.Id, "helpy.nav-tab") != RS_PLUGIN_OK) return RS_PLUGIN_DUPLICATE;
        if (host->RegisterService(Descriptor.Id, "RuneSchema.Wiki.events", Events, nullptr) != RS_PLUGIN_OK) return RS_PLUGIN_DUPLICATE;
        static const RuneSchemaNavTabV1 tab{sizeof(RuneSchemaNavTabV1), Route, "Wiki", "W", Draw, nullptr,
            "Read the Dragonwilds wiki without leaving the game.", "wiki.png", 0};
        if (host->RegisterNavTab(Descriptor.Id, &tab) != RS_PLUGIN_OK) return RS_PLUGIN_FAILED;
        *instance = const_cast<RuneSchemaPluginDescriptor*>(&Descriptor);
        return RS_PLUGIN_OK;
    } catch (...) { return RS_PLUGIN_FAILED; }
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept {}
