#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace fusionps4::input {

// Pad state in the layout used by PS4's libScePad. Bytes exactly as the
// guest expects inside ScePadData.
struct PadData {
    std::uint32_t buttons        = 0;  // bit field (ScePadButtonDataOffset)
    std::uint8_t  leftStickX     = 128;
    std::uint8_t  leftStickY     = 128;
    std::uint8_t  rightStickX    = 128;
    std::uint8_t  rightStickY    = 128;
    std::uint8_t  l2             = 0;
    std::uint8_t  r2             = 0;
    std::uint8_t  padding1[2]    = {0, 0};
    std::uint8_t  connected      = 0;
    std::uint8_t  padding2[3]    = {0, 0, 0};
    std::uint64_t timestamp      = 0;  // microseconds
    std::uint8_t  padding3[0x30] = {0};
    std::uint8_t  touchData[0x10]= {0};
};

static_assert(sizeof(PadData) == 0x78, "PadData must match ScePadData layout");

// PS4 button bit positions.
enum PadButton : std::uint32_t {
    kPadL3       = 0x00000002,
    kPadR3       = 0x00000004,
    kPadOptions  = 0x00000008,
    kPadUp       = 0x00000010,
    kPadRight    = 0x00000020,
    kPadDown     = 0x00000040,
    kPadLeft     = 0x00000080,
    kPadL2       = 0x00000100,
    kPadR2       = 0x00000200,
    kPadL1       = 0x00000400,
    kPadR1       = 0x00000800,
    kPadTriangle = 0x00001000,
    kPadCircle   = 0x00002000,
    kPadCross    = 0x00004000,
    kPadSquare   = 0x00008000,
    kPadTouchPad = 0x00100000,
};

class InputManager {
public:
    static constexpr int kMaxPads = 4;

    InputManager() = default;
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Opens the SDL game controller subsystem and enumerates devices.
    bool init();

    // Closes all controllers.
    void shutdown();

    // Called once per host frame from Runtime::tick(). Updates PadData
    // snapshots from SDL state; delivers hot-plug events.
    void poll();

    // Snapshot of the current state for a pad index (0..3).
    PadData padState(int index) const;

    // True if a controller is physically attached at index.
    bool isConnected(int index) const;

    // Set vibration (rumble). percent 0..255 per channel.
    void setVibration(int index, int left, int right);

private:
    void updatePadFromController(int index);
    void updatePadFromKeyboard(int index);

    mutable std::mutex        m_mutex;
    std::array<PadData, 4>    m_pads{};
    std::array<void*, 4>      m_controllers{};   // SDL_GameController*
    bool                      m_sdlControllerReady = false;
};

} // namespace fusionps4::input
