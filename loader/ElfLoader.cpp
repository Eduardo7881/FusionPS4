#include "loader/ElfLoader.hpp"

#include "debug/Log.hpp"
#include "loader/elf/ElfReader.hpp"
#include "loader/elf/RelocationProcessor.hpp"
#include "loader/modules/ModuleRegistry.hpp"
#include "loader/symbols/SymbolResolver.hpp"
#include "runtime/memory/AddressSpace.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::memory::AddressSpace;
using fusionps4::runtime::memory::RegionProt;

namespace fusionps4::loader {

namespace {

constexpr std::uint64_t kPageSize = 0x1000;
constexpr std::uint64_t kMaxAlign = 1ull << 21;   // 2 MiB

std::uint64_t alignUp(std::uint64_t v, std::uint64_t a) {
    if (a == 0) a = 1;
    return (v + (a - 1)) & ~(a - 1);
}

RegionProt fromFlags(std::uint32_t pf) {
    RegionProt p = RegionProt::None;
    if (pf & elf::kPfR) p = p | RegionProt::Read;
    if (pf & elf::kPfW) p = p | RegionProt::Write;
    if (pf & elf::kPfX) p = p | RegionProt::Execute;
    return p;
}

} // namespace

struct ElfLoader::Impl {
    AddressSpace&            addressSpace;
    modules::ModuleRegistry& registry;
    symbols::SymbolResolver& resolver;

    // Keep every ELF file buffer alive for the process lifetime. Shared
    // pointers would be cleaner, but a single owner here is sufficient
    // because the loader outlives all modules.
    std::vector<std::unique_ptr<std::vector<std::uint8_t>>> buffers;
};

ElfLoader::ElfLoader(AddressSpace&            as,
                     modules::ModuleRegistry& registry,
                     symbols::SymbolResolver& resolver)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->addressSpace = as;
    m_impl->registry     = registry;
    m_impl->resolver     = resolver;
}

ElfLoader::~ElfLoader() = default;

modules::ModulePtr ElfLoader::load(const std::string& path) {
    // 1) Read the file.
    auto buffer = std::make_unique<std::vector<std::uint8_t>>();
    if (!elf::ElfReader::readFile(path, *buffer)) return nullptr;

    // 2) Parse the ELF header and program headers.
    elf::ElfImage image;
    if (!elf::ElfReader::parse(*buffer, image)) return nullptr;

    // 3) Compute image layout: preferred base, total image size, alignment.
    std::uint64_t preferredBase = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t imageAlign    = kPageSize;
    for (const auto& s : image.phdrs) {
        if (s.type != elf::kPtLoad) continue;
        preferredBase = std::min(preferredBase, s.vaddr & ~(kPageSize - 1));
        imageAlign    = std::max(imageAlign, s.align ? s.align : kPageSize);
    }
    if (preferredBase == std::numeric_limits<std::uint64_t>::max()) {
        FP4_ERROR(LogCategory::Loader)
            << "\"" << path << "\" has no PT_LOAD segments";
        return nullptr;
    }
    imageAlign = std::min<std::uint64_t>(imageAlign, kMaxAlign);

    std::uint64_t imageSize = 0;
    for (const auto& s : image.phdrs) {
        if (s.type != elf::kPtLoad) continue;
        const std::uint64_t end =
            (s.vaddr - preferredBase) + alignUp(s.memsz, kPageSize);
        imageSize = std::max(imageSize, end);
    }
    imageSize = alignUp(imageSize, kPageSize);

    // 4) Find a free guest VA range large enough.
    const std::uint64_t actualBase =
        m_impl->addressSpace.findFreeRange(imageSize, imageAlign);
    if (!actualBase) {
        FP4_ERROR(LogCategory::Loader)
            << "no free guest range for image of size " << imageSize;
        return nullptr;
    }

    // 5) Construct Module and map segments.
    auto mod = std::make_shared<modules::Module>();
    mod->setup(path, image, preferredBase, actualBase);

    const auto delta = actualBase - preferredBase;

    for (const auto& s : image.phdrs) {
        if (s.type != elf::kPtLoad) continue;

        const std::uint64_t segActual = s.vaddr + delta;
        const std::uint64_t segMem    = alignUp(s.memsz, kPageSize);
        const auto prot = fromFlags(s.flags);

        // Reserve and map; the runtime AddressSpace checks that this exact
        // range is free.
        const auto mapped = m_impl->addressSpace.map(
            segActual, segMem, prot,
            std::string("seg:") + path);
        if (mapped != segActual) {
            FP4_ERROR(LogCategory::Loader)
                << "segment mapping mismatch: wanted "
                << reinterpret_cast<void*>(segActual)
                << " got " << reinterpret_cast<void*>(mapped)
                << " for \"" << path << "\"";
            return nullptr;
        }

        if (s.filesz) {
            const std::uint8_t* src = image.at(s.offset);
            if (!src || s.offset + s.filesz > image.size) {
                FP4_ERROR(LogCategory::Loader)
                    << "PT_LOAD file range out of file";
                return nullptr;
            }
            // Guest VA == host VA, so we copy straight into the mapped page.
            std::memcpy(reinterpret_cast<void*>(std::uintptr_t(segActual)),
                        src, static_cast<std::size_t>(s.filesz));

            // Zero the tail (bss) if memsz > filesz.
            if (s.memsz > s.filesz) {
                std::memset(
                    reinterpret_cast<void*>(std::uintptr_t(segActual + s.filesz)),
                    0, static_cast<std::size_t>(s.memsz - s.filesz));
            }
        }

        modules::LoadedSegment ls;
        ls.vaddr  = segActual;
        ls.filesz = s.filesz;
        ls.memsz  = s.memsz;
        ls.prot   = s.flags;
        mod->appendSegment(ls);

        FP4_DEBUG(LogCategory::Loader)
            << "  PT_LOAD " << reinterpret_cast<void*>(segActual)
            << " filesz=" << s.filesz << " memsz=" << s.memsz
            << " prot=" << s.flags;
    }

    // 6) Parse PT_DYNAMIC.
    if (const auto* dynSeg = image.findFirst(elf::kPtDynamic)) {
        if (dynSeg->filesz % sizeof(elf::Elf64_Dyn) != 0) {
            FP4_ERROR(LogCategory::Loader) << "PT_DYNAMIC size not multiple";
            return nullptr;
        }
        const auto count =
            static_cast<std::size_t>(dynSeg->filesz / sizeof(elf::Elf64_Dyn));
        const auto* src = image.as<elf::Elf64_Dyn>(dynSeg->offset);
        if (!src) {
            FP4_ERROR(LogCategory::Loader) << "PT_DYNAMIC out of file";
            return nullptr;
        }
        // Copy the entries into the module (preferred-address space).
        std::vector<elf::Elf64_Dyn> dyns(src, src + count);
        mod->dynamics() = dyns;  // friend-ish access via reference getter

        // PT_DYNAMIC's d_val entries reference preferred addresses. We
        // translate every loaded address on first use (in populate*).
    }

    // Because Module::dynamics() returns a const ref, we instead build the
    // vector via a small side-channel: the loader sets the dynamics before
    // we call populate. We achieve that by giving Module a mutable view.
    if (const auto* dynSeg = image.findFirst(elf::kPtDynamic)) {
        const auto count =
            static_cast<std::size_t>(dynSeg->filesz / sizeof(elf::Elf64_Dyn));
        const auto* src = image.as<elf::Elf64_Dyn>(dynSeg->offset);
        mod->setDynamics(src, count);
    }

    mod->populateSymbolTableFromDynamic();

    // 7) TLS.
    if (const auto* tls = image.findFirst(elf::kPtTls)) {
        const std::uint8_t* tmpl = image.at(tls->offset);
        mod->configureTls(tmpl, tls->filesz, tls->memsz, tls->align);
    } else {
        // No TLS; nothing to do.
    }

    // 8) Register the module BEFORE relocating, so GLOB_DAT can resolve
    //    against its own exports through the registry.
    m_impl->registry.add(path, mod);

    // 9) Apply relocations.
    elf::RelocationProcessor rp(*mod, m_impl->resolver);
    elf::RelocationStats stats;
    if (!rp.applyAll(stats)) {
        FP4_ERROR(LogCategory::Loader)
            << "relocation failed for \"" << path << "\"";
        return nullptr;
    }

    // 10) Entry point.
    const std::uint64_t entryPreferred = image.ehdr.e_entry;
    const std::uint64_t entryActual    = entryPreferred + delta;
    mod->setEntryPoint(entryActual);

    FP4_INFO(LogCategory::Loader)
        << "module \"" << path << "\" loaded at "
        << reinterpret_cast<void*>(std::uintptr_t(actualBase))
        << " entry=" << reinterpret_cast<void*>(std::uintptr_t(entryActual));

    // Keep the ELF buffer alive for the process lifetime.
    m_impl->buffers.emplace_back(std::move(buffer));

    return mod;
}

} // namespace fusionps4::loader
