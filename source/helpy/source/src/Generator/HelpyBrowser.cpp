#include "Runtime/HelpyBrowser.h"
#include "Core/HelpyBlockerRelease.h"
#include "Core/HelpyZip.h"
#include "Core/CacheBundleIO.h"
#include "Core/ConfigFiles.h"
#include "Core/ProcessExitFuture.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Runtime/HostServices.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <functional>
#include <stdexcept>
#include <vector>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")

namespace PS::HelpyBrowser {
namespace {
using Json = nlohmann::json;
using Bytes = std::vector<char>;
View State;
PS::ProcessExitFuture<View> Worker;
std::atomic<bool> Cancelled = false;
bool Wanted = false, Checked = false;
std::chrono::steady_clock::time_point NextAttempt{};

std::filesystem::path Root() { return HostServices::StateDirectory() / "web-browser-extensions"; }

struct Handle { HINTERNET Value{}; ~Handle() { if (Value) WinHttpCloseHandle(Value); } operator HINTERNET() const { return Value; } };
// HTTPS GET with a size limit. Redirects are followed (release files redirect to GitHub's file
// host) but never from HTTPS to HTTP; what arrives is trusted only after its checksum matches.
#ifdef HELPY_BROWSER_TEST
std::function<Bytes(const std::string&)> DownloadForTest;
#endif
Bytes Get(const std::string& url, uint64_t limit, const wchar_t* accept) {
#ifdef HELPY_BROWSER_TEST
    if (DownloadForTest) return DownloadForTest(url);
#endif
    if (Cancelled) throw std::runtime_error("Cancelled.");
    const std::wstring wide(url.begin(), url.end());
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{}, path[2048]{};
    parts.lpszHostName = host; parts.dwHostNameLength = 256; parts.lpszUrlPath = path; parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) throw std::runtime_error("Not an HTTPS address.");
    Handle session{WinHttpOpen(L"RuneSchema-Helpy-Browser/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session || !WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000)) throw std::runtime_error("No network connection is available.");
    Handle connection{WinHttpConnect(session, host, parts.nPort, 0)};
    const wchar_t* types[]{accept, nullptr};
    Handle request{connection ? WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER, types, WINHTTP_FLAG_SECURE) : nullptr};
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))
        || !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr))
        throw std::runtime_error("GitHub could not be reached. Check your connection, then try again.");
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX) || status != 200)
        throw std::runtime_error(status == 403 || status == 429 ? "GitHub is limiting requests right now. Try again later." : "GitHub returned HTTP " + std::to_string(status) + ".");
    Bytes bytes;
    std::array<char, 32768> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    for (;;) {
        if (Cancelled || std::chrono::steady_clock::now() > deadline) throw std::runtime_error("The download was cancelled or timed out.");
        DWORD count = 0;
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &count)) throw std::runtime_error("The download was interrupted.");
        if (!count) break;
        if (bytes.size() + count > limit) throw std::runtime_error("The download is larger than expected.");
        bytes.insert(bytes.end(), buffer.data(), buffer.data() + count);
    }
    return bytes;
}
std::string Sha256(const Bytes& bytes) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
    struct Cleanup { BCRYPT_ALG_HANDLE& A; BCRYPT_HASH_HANDLE& H; ~Cleanup() { if (H) BCryptDestroyHash(H); if (A) BCryptCloseAlgorithmProvider(A, 0); } } cleanup{algorithm, hash};
    std::array<unsigned char, 32> digest{};
    if (bytes.size() > ULONG_MAX || BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0
        || BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0
        || BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())), static_cast<ULONG>(bytes.size()), 0) < 0
        || BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) throw std::runtime_error("The download could not be checked.");
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto c : digest) { result += digits[c >> 4]; result += digits[c & 15]; }
    return result;
}

// The copy already unpacked, from the marker written after a verified install. No network.
View Installed() {
    View view;
    auto marker = Root() / "installed.json";
    std::error_code error;
    const bool pending=!std::filesystem::is_regular_file(marker,error);
    if (pending) marker=Root()/"downloaded.json";
    if (!std::filesystem::is_regular_file(marker, error) || std::filesystem::file_size(marker, error) > 4096) return view;
    try {
        RuneSchemaData::Safe(marker);
        CacheBundleIO::MetadataLock lock(Root()/"installed.json");
        const auto json = Json::parse(ConfigFiles::Read(marker,4096));
        const auto tag = json.at("Tag").get<std::string>();
        const auto folder = Root() / ("ubol-" + tag);
        if (!HelpyBlockerRelease::Tag(tag) || !std::filesystem::is_regular_file(folder / "manifest.json", error)) return view;
        view.State = Blocker::Ready; view.Version = tag; view.Folder = folder;view.PendingActivation=pending;
        view.Message = "uBlock Origin Lite " + tag + " is ready to load.";
    } catch (...) {}
    return view;
}

// Downloads the latest release when it differs from the installed one; keeps the old copy on any failure.
View Install(View current) {
    const auto latestJson = Get(std::string(HelpyBlockerRelease::LatestApi), 1024 * 1024, L"application/vnd.github+json");
    const auto release = HelpyBlockerRelease::Parse(Json::parse(latestJson.begin(), latestJson.end()));
    if (current.State == Blocker::Ready && current.Version == release.Tag) {
        current.Message = "uBlock Origin Lite " + release.Tag + " is ready and up to date.";
        return current;
    }
    const auto zip = Get(release.Url, release.Size, L"application/octet-stream");
    if (zip.size() != release.Size || Sha256(zip) != release.Sha256) throw std::runtime_error("The download did not match its published checksum, so Helpy did not use it.");
    const auto root = Root();
    RuneSchemaData::Safe(root);
    std::filesystem::create_directories(root);
    const auto stamp = std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    const auto file = root / ("download-" + stamp + ".zip"), staging = root / ("unpacking-" + stamp), folder = root / ("ubol-" + release.Tag);
    struct Tidy { std::filesystem::path A, B; ~Tidy() { std::error_code e; std::filesystem::remove(A, e); std::filesystem::remove_all(B, e); } } tidy{file, staging};
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(zip.data(), static_cast<std::streamsize>(zip.size()));out.close();
        if (!out) throw std::runtime_error("The download could not be saved.");
    }
    HelpyZip::Extract(file, staging);
    if (!std::filesystem::is_regular_file(staging / "manifest.json")) throw std::runtime_error("The download is not a browser extension.");
    const Json marker{{"Tag", release.Tag}, {"Asset", release.Asset}, {"Sha256", release.Sha256}, {"Source", release.Url}};
    ConfigFiles::Write(staging / "download.json", marker.dump(2));
    {
        CacheBundleIO::MetadataLock lock(root / "installed.json");
        RuneSchemaData::Safe(folder);
        if (std::filesystem::exists(folder)) {
            // Another Helpy may have published this version while we downloaded it.
            const auto metadata = std::filesystem::is_regular_file(folder / "download.json") ? folder / "download.json" : root / "installed.json";
            if (!std::filesystem::is_regular_file(folder / "manifest.json") || Json::parse(ConfigFiles::Read(metadata,4096)) != marker)
                throw std::runtime_error("The existing ad blocker does not match this download; it was left untouched.");
        } else std::filesystem::rename(staging, folder);
        ConfigFiles::Write(root/"downloaded.json",marker.dump(2));
    }
    // Installed versions can still be loaded by another Helpy process or needed for rollback.
    View view; view.State = Blocker::Ready; view.Version = release.Tag; view.Folder = folder; view.PendingActivation = true;
    view.Message = "uBlock Origin Lite " + release.Tag + " was downloaded and checked; open Browser to load it.";
    return view;
}

bool Start(bool update) {
    if (Worker.valid() || std::chrono::steady_clock::now() < NextAttempt) return false;
    NextAttempt = std::chrono::steady_clock::now() + std::chrono::seconds(update ? 10 : 30);
    Cancelled = false;
    auto current = State;
    State.Message = update || current.State == Blocker::Ready ? "Checking for a newer uBlock Origin Lite..." : "Downloading uBlock Origin Lite from its GitHub release...";
    if (current.State != Blocker::Ready) State.State = Blocker::Working;
    try {
        Worker = std::async(std::launch::async, [current] {
            try { return Install(current); }
            catch (const std::exception& error) {
                auto view = Installed();
                if (view.State != Blocker::Ready) view.State = Blocker::Failed;
                view.Message = std::string(error.what()) + (view.State == Blocker::Ready ? " The previous copy is still available." : "");
                return view;
            }
        });
        return true;
    } catch (...) { State.State = current.State == Blocker::Ready ? Blocker::Ready : Blocker::Failed; State.Message = "Could not start the download."; return false; }
}
void Poll() {
    if (!Worker.valid() || Worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try { State = Worker.get(); } catch (...) { State.State = Blocker::Failed; State.Message = "The download failed."; }
}
}

void AdBlock(bool on) {
    Poll();
    if (!on) {
        if (Wanted) { Cancelled = true; Wanted = false; }
        if (!Worker.valid()) { const auto keep = State.Version; State = {}; State.Version = keep; }
        return;
    }
    if (!Wanted) { Wanted = true;State=Installed();Checked=State.State==Blocker::Ready; }
    if (Checked || Worker.valid()) return;
    if (State.State==Blocker::Ready) { Checked=true;return; }
    if (std::chrono::steady_clock::now() < NextAttempt) {
        if (State.State != Blocker::Failed) { State.State = Blocker::Failed; State.Message = "Waiting for the download cooldown; Helpy will retry when it ends."; }
        return;
    }
    State = Installed();
    Checked = State.State == Blocker::Ready || Start(false);
}
bool CheckForUpdate() { return Wanted && Start(true); }
View Current() { Poll(); return Wanted || Worker.valid() ? State : View{}; }
void Activated(const std::filesystem::path& folder) noexcept {
    if (!Wanted || State.State != Blocker::Ready || folder != State.Folder) return;
    if (!State.PendingActivation) return;
    try {
        RuneSchemaData::Safe(folder);
        if (folder.parent_path() != Root()) throw std::runtime_error("Unexpected ad blocker folder.");
        CacheBundleIO::MetadataLock lock(Root() / "installed.json");
        const auto metadata=std::filesystem::is_regular_file(folder/"download.json")?folder/"download.json":Root()/"installed.json";
        const auto marker=Json::parse(ConfigFiles::Read(metadata,4096));
        if (marker.at("Tag") != State.Version || folder.filename() != "ubol-"+State.Version) throw std::runtime_error("Ad blocker identity changed.");
        ConfigFiles::Write(Root()/"installed.json",marker.dump(2));
        State.PendingActivation=false;State.Message="uBlock Origin Lite "+State.Version+" is on.";
    } catch (const std::exception& error) { State.Message=std::string("The ad blocker loaded, but its installation could not be recorded: ")+error.what(); }
    catch (...) { State.Message="The ad blocker loaded, but its installation could not be recorded."; }
}
void Rejected(const std::filesystem::path& folder,const std::string& reason) noexcept {
    if (!Wanted || folder != State.Folder) return;
    try {
        auto previous=Installed();
        if (previous.State==Blocker::Ready && previous.Folder!=folder) {
            previous.Message=reason+" Restoring the previous ad blocker.";State=std::move(previous);
        } else { State={};State.State=Blocker::Failed;State.Message=reason; }
    } catch (...) { State={};State.State=Blocker::Failed; }
}
void Shutdown() noexcept {
    Cancelled = true;
    if (Worker.valid()) try { (void)Worker.get(); } catch (...) {}
    Wanted = Checked = false; State = {};
}
}
