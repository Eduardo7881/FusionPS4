#pragma once

#include "loader/elf/ElfTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::loader::elf {

struct SegmentInfo {
    std::uint32_t type    = 0;
    std::uint32_t flags   = 0;
    std::uint64_t offset  = 0;
    std::uint64_t vaddr   = 0;
    std::uint64_t filesz  = 0;
    std::uint64_t memsz   = 0;
    std::uint64_t align   = 0;
};

// A non-owning view over the raw ELF file bytes plus the parsed headers.
// The buffer must outlive the image and any module built on top of it.
struct ElfImage {
    const std::uint8_t*          data  = nullptr;
    std::size_t                  size  = 0;
    Elf64_Ehdr                   ehdr{};
    std::vector<SegmentInfo>     phdrs;

    const SegmentInfo* findFirst(std::uint32_t type) const {
        for (const auto& s : phdrs) if (s.type == type) return &s;
        return nullptr;
    }

    const std::uint8_t* at(std::uint64_t fileOffset) const {
        if (fileOffset >= size) return nullptr;
        return data + fileOffset;
    }

    template <typename T>
    const T* as(std::uint64_t fileOffset) const {
        if (fileOffset + sizeof(T) > size) return nullptr;
        return reinterpret_cast<const T*>(data + fileOffset);
    }
};

class ElfReader {
public:
    // Reads the entire file into `out`. Returns false and logs on failure.
    static bool readFile(const std::string& path,
                         std::vector<std::uint8_t>& out);

    // Validates and parses header + program headers. `data` must be the
    // buffer whose begin pointer is stored in `out.data`. Returns false
    // (with logging) on any structural problem.
    static bool parse(const std::vector<std::uint8_t>& data, ElfImage& out);

    static const char* typeName(std::uint16_t e_type);
    static const char* machineName(std::uint16_t e_machine);
    static const char* phdrTypeName(std::uint32_t p_type);
};

} // namespace fusionps4::loader::elf
