// fs4-unimpl-report — reads a FUSIONPS4 log file (produced by a prior run
// with FUSIONPS4_LOGFILE set) and summarizes the UNIMPLEMENTED entries.
// The tool is intentionally simple: it greps the log for "UNIMPLEMENTED"
// lines and aggregates by prefix, so it works without any runtime linkage.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <fusionps4.log>\n", argv[0]);
        return 2;
    }
    std::ifstream f(argv[1]);
    if (!f) { std::perror("open"); return 1; }

    std::unordered_map<std::string, std::uint64_t> counts;
    std::string line;
    while (std::getline(f, line)) {
        const auto pos = line.find("UNIMPLEMENTED");
        if (pos == std::string::npos) continue;
        std::string tail = line.substr(pos);
        // Trim past the first argument-ish delimiter to normalize.
        const auto cut = tail.find_first_of(" (");
        if (cut != std::string::npos) tail = tail.substr(0, cut);
        counts[tail]++;
    }

    std::vector<std::pair<std::string, std::uint64_t>> sorted(
        counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::printf("%zu unique UNIMPLEMENTED entries\n", sorted.size());
    for (const auto& [k, v] : sorted) {
        std::printf("  %8llu  %s\n", (unsigned long long)v, k.c_str());
    }
    return 0;
}
