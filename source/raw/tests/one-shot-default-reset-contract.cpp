#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("source unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Need(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    Need(argc == 4, "config, pruner, and UI sources required");
    const auto config = Read(argv[1]);
    const auto pruner = Read(argv[2]);
    const auto ui = Read(argv[3]);
    const auto read = config.find("const bool resetRequested =");
    const auto clear = config.find("restoration.resetOnce = false;", read);
    const auto persist = config.find("if (!Save())", clear);
    const auto arm = config.find("m_defaultResetOnceRequested = true;", persist);
    Need(read != config.npos && clear != config.npos && persist != config.npos
            && arm != config.npos && read < clear && clear < persist && persist < arm,
        "one-shot reset is armed before resetOnce=false is safely persisted");
    Need(config.find("reset was refused") != config.npos,
        "a failed self-reset does not refuse destructive restoration");
    Need(pruner.find("ConsumeDefaultResetOnce()") != pruner.npos
            && pruner.find("Mode::ReplaceSelected") != pruner.npos
            && pruner.find("Mode::MergeBaseline") != pruner.npos,
        "pruner does not distinguish safe merge from one-shot replacement");
    Need(ui.find("Safely merge selected Default.json baselines") != ui.npos
            && ui.find("Reset selected sections once on next launch") != ui.npos,
        "settings UI does not clearly separate safe and destructive modes");
}
