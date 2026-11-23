#include "Core/HelpyBlockerRelease.h"
#include "Core/HelpyBrowserAddress.h"
#include "Core/HelpyZip.h"
#include "miniz.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
// Helpy's web browser: the address bar, the ad blocker's release choice, and safe ZIP extraction.
namespace A = PS::HelpyBrowserAddress; namespace R = PS::HelpyBlockerRelease; namespace Z = PS::HelpyZip; using Json = nlohmann::json;
static void Check(bool ok, const std::string& what) { if (!ok) throw std::runtime_error(what); }
template <class F> static bool Refused(F f) { try { f(); return false; } catch (const std::exception&) { return true; } }
// Writes a test ZIP. Stored entries keep their names exactly, so a path-traversal entry can be tested.
static void Zip(const std::filesystem::path& file, const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, file.string().c_str(), 0)) throw std::runtime_error("zip init");
    for (const auto& [name, body] : entries)
        if (!mz_zip_writer_add_mem_ex_v2(&zip, name.c_str(), body.data(), body.size(), nullptr, 0, MZ_NO_COMPRESSION, 0, 0, nullptr, nullptr, 0, nullptr, 0))
            throw std::runtime_error("zip add " + name);
    mz_zip_writer_finalize_archive(&zip); mz_zip_writer_end(&zip);
}
int main() {
    try {
        // Address bar.
        Check(A::Resolve("https://dragonwilds.runescape.wiki/w/Armour") == "https://dragonwilds.runescape.wiki/w/Armour", "web address kept");
        Check(A::Resolve("  youtube.com/watch?v=abc ") == "https://youtube.com/watch?v=abc", "bare host gets https");
        Check(A::Resolve("localhost:8080") == "https://localhost:8080", "host with port");
        Check(A::Resolve("dragonwilds armour sets") == "https://duckduckgo.com/?q=dragonwilds+armour+sets", "words become a search");
        Check(A::Resolve("javascript:alert(1)").starts_with(std::string(A::Search)), "javascript: is searched, not run");
        Check(A::Resolve("file:///C:/Windows").starts_with(std::string(A::Search)) && A::Resolve("data:text/html,x").starts_with(std::string(A::Search)), "other schemes are searched");
        Check(A::Resolve("").empty() && A::Resolve(std::string(A::MaxLength + 1, 'a')).empty(), "empty and overlong open nothing");
        Check(A::Web("HTTPS://Example.com") && A::Web("http://example.com/a?b") && !A::Web("https://") && !A::Web("https:///x") && !A::Web("https://a b")
            && !A::Web("ftp://x") && !A::Web("https://@other"), "only web addresses navigate");
        // Ad blocker release: the Edge build with a checksum, from the project's own downloads.
        const std::string hash(64, 'a'), tag = "2026.1026.1812", base = std::string(R::DownloadPrefix) + tag + "/";
        const auto asset = [&](const std::string& build, std::string digest = "", std::string url = "") {
            const auto name = "uBOLite_" + tag + build;
            return Json{{"name", name}, {"size", 4567890}, {"digest", digest.empty() ? "sha256:" + hash : digest}, {"browser_download_url", url.empty() ? base + name : url}};
        };
        Json release{{"tag_name", tag}, {"assets", Json::array({asset(".firefox.signed.xpi"), asset(".chromium.zip"), asset(".edge.zip")})}};
        const auto chosen = R::Parse(release);
        Check(chosen.Asset == "uBOLite_" + tag + ".edge.zip" && chosen.Sha256 == hash && chosen.Size == 4567890 && chosen.Url == base + chosen.Asset, "Edge build chosen");
        release["assets"] = Json::array({asset(".chromium.zip")});
        Check(R::Parse(release).Asset.ends_with(".chromium.zip"), "Chromium build as fallback");
        for (const auto& bad : std::vector<Json>{asset(".edge.zip", "md5:00"), asset(".edge.zip", "sha256:" + std::string(63, 'a')),
                 asset(".edge.zip", "", "https://downloads.example/uBOLite_" + tag + ".edge.zip"), asset(".edge.zip", "", base + "other.zip")}) {
            release["assets"] = Json::array({bad});
            Check(Refused([&] { R::Parse(release); }), "unverifiable or foreign download refused");
        }
        { auto big = asset(".edge.zip"); big["size"] = R::MaxBytes + 1; release["assets"] = Json::array({big}); Check(Refused([&] { R::Parse(release); }), "oversized refused"); }
        for (const auto* version : {"v1", "../1", "1.", "", ".1"}) { const Json r{{"tag_name", version}, {"assets", Json::array()}}; Check(Refused([&] { R::Parse(r); }), "odd version refused"); }
        Check(Refused([&] { R::Parse(Json{{"tag_name", tag}, {"assets", Json::array({asset(".firefox.signed.xpi")})}}); }), "no usable build refused");
        // ZIP extraction.
        for (const auto* good : {"manifest.json", "js/a.js", "rulesets/main/x.json", "dir/"}) Check(Z::SafeEntry(good), std::string("plain entry ") + good);
        for (const auto* bad : {"../outside.txt", "a/../../outside", "/abs.txt", "C:/x", "a\\b", "a//b", "./x", "", "..", "../", "a/../", ".. /outside.txt", "a./b", "a /b"}) Check(!Z::SafeEntry(bad), std::string("refused entry ") + bad);
        const auto root = std::filesystem::temp_directory_path() / "helpy-browser-test";
        std::filesystem::remove_all(root); std::filesystem::create_directories(root);
        Zip(root / "good.zip", {{"manifest.json", "{\"name\":\"uBlock Origin Lite\"}"}, {"js/app.js", "console.log(1)"}});
        Z::Extract(root / "good.zip", root / "out");
        Check(std::filesystem::exists(root / "out/manifest.json") && std::filesystem::exists(root / "out/js/app.js"), "zip extracted");
        Check(Refused([&] { Z::Extract(root / "good.zip", root / "out"); }), "never extracts over an existing folder");
        Zip(root / "traversal.zip", {{"manifest.json", "{}"}, {"../outside.txt", "x"}});
        Check(Refused([&] { Z::Extract(root / "traversal.zip", root / "traversal"); }), "path traversal refused");
        Check(!std::filesystem::exists(root / "traversal") && !std::filesystem::exists(root / "outside.txt"), "a refused zip leaves nothing behind");
        Check(Refused([&] { Z::Extract(root / "good.zip", root / "few", {1, 1024}); }), "file limit enforced");
        Check(Refused([&] { Z::Extract(root / "good.zip", root / "tiny", {4000, 10}); }), "size limit enforced");
        { std::ofstream junk(root / "junk.zip", std::ios::binary); junk << std::string(100, 'x'); }
        Check(Refused([&] { Z::Extract(root / "junk.zip", root / "junk"); }), "not a zip refused");
        std::filesystem::remove_all(root);
        std::cout << "Helpy browser passed: address bar, ad blocker release, safe ZIP extraction.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
