#include "debug/TraceConfig.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace fusionps4::debug {

namespace {

std::string trim(std::string s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c); };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), isSpace));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), isSpace).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, delim)) out.push_back(tok);
    return out;
}

} // namespace

std::vector<std::string> TraceConfig::categoryNames() {
    std::vector<std::string> out;
    const auto count = static_cast<std::size_t>(LogCategory::Count);
    for (std::size_t i = 0; i < count; ++i) {
        out.emplace_back(lower(logCategoryName(static_cast<LogCategory>(i))));
    }
    return out;
}

bool TraceConfig::categoryFromName(const std::string& name, LogCategory& out) {
    const auto names = categoryNames();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            out = static_cast<LogCategory>(i);
            return true;
        }
    }
    return false;
}

bool TraceConfig::apply(const std::string& specRaw) {
    const std::string spec = trim(lower(specRaw));
    if (spec.empty()) return true;

    // Base state: parse whether we start with "all disabled" (whitelist) or
    // "all enabled" (blacklist). A plain token list without leading '-' is
    // treated as a whitelist; any leading '-' anywhere means blacklist mode
    // with everything else enabled.
    std::vector<std::string> tokens = split(spec, ',');
    bool blacklistMode = false;
    for (const auto& t : tokens) {
        if (!t.empty() && t[0] == '-') { blacklistMode = true; break; }
    }

    auto setAll = [&](bool enabled) {
        const auto count = static_cast<std::size_t>(LogCategory::Count);
        for (std::size_t i = 0; i < count; ++i) {
            Log::setCategoryEnabled(static_cast<LogCategory>(i), enabled);
        }
    };

    if (spec == "all") {
        setAll(true);
        return true;
    }
    if (spec == "none") {
        setAll(false);
        return true;
    }

    setAll(blacklistMode);

    bool ok = true;
    for (auto t : tokens) {
        t = trim(t);
        if (t.empty()) continue;
        bool enable = true;
        if (t[0] == '-') { enable = false; t = trim(t.substr(1)); }
        if (t == "all") { setAll(enable); continue; }
        if (t == "none") { setAll(!enable); continue; }

        LogCategory c;
        if (!categoryFromName(t, c)) {
            Log::write(LogLevel::Warn, LogCategory::Error,
                       "FUSIONPS4_TRACE: unknown category \"" + t + "\"");
            ok = false;
            continue;
        }
        Log::setCategoryEnabled(c, enable);
    }
    return ok;
}

void TraceConfig::applyFromEnvironment() {
    const char* env = std::getenv("FUSIONPS4_TRACE");
    if (!env || !*env) return;
    apply(env);
}

} // namespace fusionps4::debug
