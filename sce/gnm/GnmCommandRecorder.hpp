#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

namespace fusionps4::sce::gnm {

// Thread-safe FIFO that the guest thread (via SCE stubs) pushes into and
// the runtime thread (via Runtime::tick) drains. Commands are POD; there
// is no ordering dependency beyond the queue itself.
class GnmCommandRecorder {
public:
    GnmCommandRecorder() = default;

    void record(const graphics::RecordedDraw& cmd);

    // Atomically extract and clear the pending commands.
    std::vector<graphics::RecordedDraw> drain();

    std::size_t pending() const;

    // Signal used by sceVideoOutSubmitFlip: it indicates the guest has
    // finished recording for the frame. The runtime observes this and
    // performs the present. It is reset by the runtime after each present.
    void requestFrame();
    bool takeFrameRequest();

private:
    mutable std::mutex                          m_mutex;
    std::vector<graphics::RecordedDraw>         m_commands;
    bool                                        m_frameRequested = false;
};

} // namespace fusionps4::sce::gnm
