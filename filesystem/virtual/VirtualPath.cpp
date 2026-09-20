#include "filesystem/virtual/VirtualPath.hpp"

#include <sstream>
#include <vector>

namespace fusionps4::filesystem::virtual_fs {

namespace {

bool isPrintablePathChar(char c) {
    const auto u = static_cast<unsigned char>(c);
    if (u == 0) return false;
    // Allow tabs/spaces? Reject: PS4 paths are strictly ASCII printable.
    return u >= 0x20 && u < 0x7F;
}

} // namespace

bool VirtualPath::normalize(const std::string& input, std::string& out) {
    out.clear();
    if (input.empty()) return false;

    // Embedded NUL is a hard reject.
    if (input.find('\0') != std::string::npos) return false;

    std::vector<std::string> stack;
    std::size_t i = 0;
    const bool absolute = input[0] == '/';
    if (absolute) ++i;

    std::string cur;
    for (; i <= input.size(); ++i) {
        const char c = (i < input.size()) ? input[i] : '/';
        if (c == '/') {
            if (cur.empty()) {
                // Collapse runs of '/'.
                continue;
            }
            if (cur == ".") {
                // Drop.
            } else if (cur == "..") {
                if (!stack.empty()) stack.pop_back();
                // ".." at root is a no-op; never escapes.
            } else {
                for (char ch : cur) if (!isPrintablePathChar(ch)) return false;
                stack.push_back(cur);
            }
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }

    std::ostringstream oss;
    oss << '/';
    for (std::size_t k = 0; k < stack.size(); ++k) {
        if (k) oss << '/';
        oss << stack[k];
    }
    out = oss.str();
    return true;
}

bool VirtualPath::translate(const std::string& guestPath,
                            const std::string& vfsRoot,
                            const std::string& hostSubdir,
                            std::string&       outHostPath) {
    outHostPath.clear();

    std::string canonical;
    if (!normalize(guestPath, canonical)) return false;

    // The mount point's prefix should already have been stripped by the
    // caller (VirtualFileSystem does that when matching). We only receive
    // the trailing part here — but to be robust, accept either.
    std::string suffix = canonical;
    if (suffix == "/") suffix.clear();

    outHostPath  = vfsRoot;
    if (!outHostPath.empty() && outHostPath.back() != '/') outHostPath += '/';
    outHostPath += hostSubdir;
    if (!suffix.empty()) {
        if (outHostPath.back() != '/') outHostPath += '/';
        // Skip leading '/' of suffix.
        std::size_t start = (suffix[0] == '/') ? 1 : 0;
        outHostPath += suffix.substr(start);
    }
    return true;
}

} // namespace fusionps4::filesystem::virtual_fs
