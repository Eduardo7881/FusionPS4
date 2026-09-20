// Confirms the virtual filesystem refuses to escape its root and rejects
// paths outside the mount table.

#include "filesystem/virtual/VirtualFileSystem.hpp"

#include <cstdio>
#include <vector>

using namespace fusionps4::filesystem;

int main() {
    virtual_fs::VirtualFileSystem vfs;
    vfs.configure("test_vfs_root", {
        {"/app",  "app",  /*ro*/ true,  false, false},
        {"/save", "save", /*ro*/ false, true,  true },
    });

    auto tryPath = [&](const char* p, policy::FsOp op, bool expectOk,
                       std::int64_t expectedErr) {
        std::int64_t err = 0;
        auto tr = vfs.resolve(p, op, err);
        if (tr.ok != expectOk) {
            std::fprintf(stderr, "FAIL: \"%s\" expected ok=%d got %d\n",
                         p, (int)expectOk, (int)tr.ok);
            return false;
        }
        if (!expectOk && expectedErr && err != expectedErr) {
            std::fprintf(stderr, "FAIL: \"%s\" expected err=%lld got %lld\n",
                         p, (long long)expectedErr, (long long)err);
            return false;
        }
        return true;
    };

    // Valid reads.
    if (!tryPath("/app/game.elf",  policy::FsOp::Read,   true,  0)) return 1;
    if (!tryPath("/save/slot1.bin",policy::FsOp::Write,  true,  0)) return 1;

    // Refused: no mount.
    if (!tryPath("/etc/passwd",   policy::FsOp::Read,   false, 0)) return 1;
    if (!tryPath("/dev/null",     policy::FsOp::Read,   false, 0)) return 1;

    // Refused: read-only mount.
    if (!tryPath("/app/game.elf", policy::FsOp::Write,  false, 0)) return 1;
    if (!tryPath("/app/game.elf", policy::FsOp::Delete, false, 0)) return 1;

    // ".." cannot escape the guest root.
    if (!tryPath("/app/../../../etc/passwd", policy::FsOp::Read, false, 0)) return 1;

    std::printf("vfs_sandbox_test: OK\n");
    return 0;
}
