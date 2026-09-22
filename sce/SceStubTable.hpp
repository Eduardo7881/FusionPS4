#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fusionps4::sce {

// Central registry of SCE functions implemented by the runtime. The guest's
// PLT/GOT entries are patched by the loader to point at these addresses.
//
// Every stub is a real C++ function with the System V AMD64 ABI, which is
// exactly the ABI PS4 userland uses. This is the key property that lets us
// run PS4 code natively without a binary translator.
class SceStubTable {
public:
    static SceStubTable& instance();

    // Register under the PS4 naming convention "<lib>::<fn>".
    void registerStub(const std::string& library,
                      const std::string& function,
                      void*              addr);

    // Returns nullptr if not registered; caller treats that as UNIMPLEMENTED.
    void* resolve(const std::string& library,
                  const std::string& function) const;

    // "<lib>::<fn>" form.
    void* resolveSymbol(const std::string& qualified) const;

    std::size_t size() const;

    // Snapshot for diagnostics.
    std::vector<std::pair<std::string, void*>> snapshot() const;
   
    std::uint32_t getOrAssignId(const std::string& qualified);
    void*         resolveById(std::uint32_t id)
private:
    SceStubTable() = default;

    static std::string key(const std::string& lib, const std::string& fn);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, void*> m_stubs;
    std::unordered_map<std::uint32_t, void*> m_byId;
    std::uint32_t m_nextId = 1;
};

} // namespace fusionps4::sce
