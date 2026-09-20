// A minimal test that exercises the ELF reader against a tiny synthetic
// image. Does not require a real PS4 binary; validates the parser's
// accept/reject behaviour.

#include "loader/elf/ElfReader.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace fusionps4::loader::elf;

static std::vector<std::uint8_t> makeMinimalElf() {
    std::vector<std::uint8_t> buf(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr), 0);
    Elf64_Ehdr eh{};
    eh.e_ident[0] = 0x7F; eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';  eh.e_ident[3] = 'F';
    eh.e_ident[4] = kElfClass64;
    eh.e_ident[5] = kElfDataLsb;
    eh.e_ident[6] = kElfVersionCur;
    eh.e_ident[7] = kElfOsAbiFreeBsd;
    eh.e_type      = kEtExec;
    eh.e_machine   = kEmX86_64;
    eh.e_version   = 1;
    eh.e_entry     = 0x401000;
    eh.e_phoff     = sizeof(Elf64_Ehdr);
    eh.e_phentsize = sizeof(Elf64_Phdr);
    eh.e_phnum     = 1;
    std::memcpy(buf.data(), &eh, sizeof(eh));

    Elf64_Phdr ph{};
    ph.p_type   = kPtLoad;
    ph.p_flags  = kPfR | kPfX;
    ph.p_offset = 0;
    ph.p_vaddr  = 0x400000;
    ph.p_filesz = 0x1000;
    ph.p_memsz  = 0x1000;
    ph.p_align  = 0x1000;
    std::memcpy(buf.data() + sizeof(Elf64_Ehdr), &ph, sizeof(ph));
    return buf;
}

int main() {
    auto bytes = makeMinimalElf();
    ElfImage img;
    if (!ElfReader::parse(bytes, img)) {
        std::fprintf(stderr, "FAIL: parse returned false for valid image\n");
        return 1;
    }
    if (img.phdrs.size() != 1) {
        std::fprintf(stderr, "FAIL: expected 1 phdr, got %zu\n", img.phdrs.size());
        return 1;
    }
    if (img.phdrs[0].type != kPtLoad) {
        std::fprintf(stderr, "FAIL: expected PT_LOAD\n");
        return 1;
    }
    if (img.ehdr.e_entry != 0x401000) {
        std::fprintf(stderr, "FAIL: entry mismatch\n");
        return 1;
    }

    // Corrupt the magic and confirm the parser rejects it.
    bytes[0] = 0;
    ElfImage bad;
    if (ElfReader::parse(bytes, bad)) {
        std::fprintf(stderr, "FAIL: parse accepted corrupt magic\n");
        return 1;
    }

    std::printf("elf_loader_test: OK\n");
    return 0;
}
