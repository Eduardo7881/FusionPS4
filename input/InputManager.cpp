#include "input/InputManager.hpp"

#include "debug/Log.hpp"

#include <SDL.h>
#include <chrono>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::input {

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::init() {
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        FP4_WARN(LogCategory::Input)
            << "SDL_INIT_GAMECONTROLLER failed: " << SDL_GetError()
            << " — falling back to keyboard input";
        m_sdlControllerReady = false;
        return true;   // not fatal: keyboard still works
    }
    m_sdlControllerReady = true;

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        auto* gc = SDL_GameControllerOpen(i);
        if (!gc) {
            FP4_WARN(LogCategory::Input)
                << "SDL_GameControllerOpen(" << i << ") failed: "
                << SDL_GetError();
            continue;
        }
        for (int s = 0; s < kMaxPads; ++s) {
            if (!m_controllers[s]) {
                m_controllers[s] = gc;
                auto* joy = SDL_GameControllerGetJoystick(gc);
                const char* name = SDL_JoystickName(joy);
                FP4_INFO(LogCategory::Input)
                    << "pad " << s << " connected: "
                    << (name ? name : "<unknown>");
                break;
            }
        }
    }
    return true;
}

void InputManager::shutdown() {
    for (int i = 0; i < kMaxPads; ++i) {
        if (m_controllers[i]) {
            SDL_GameControllerClose(
                static_cast<SDL_GameController*>(m_controllers[i]));
            m_controllers[i] = nullptr;
        }
    }
    if (m_sdlControllerReady) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        m_sdlControllerReady = false;
    }
}

namespace {

// Helper: translate a normalized axis [-32768, 32767] to a PS4 byte [0, 255]
// where 128 is centered.
std::uint8_t axisToByte(Sint16 v) {
    const int centered = static_cast<int>(v) + 32768;   // [0, 65535]
    return static_cast<std::uint8_t>(centered >> 8);    // [0, 255]
}

std::uint8_t triggerToByte(Sint16 v) {
    const int c = static_cast<int>(v) + 32768;
    const int b = c >> 8;
    return static_cast<std::uint8_t>(b);
}

} // namespace

void InputManager::updatePadFromController(int index) {
    auto* gc = static_cast<SDL_GameController*>(m_controllers[index]);
    if (!gc) return;

    PadData d;
    d.connected = 1;

    auto btn = [&](SDL_GameControllerButton b) {
        return SDL_GameControllerGetButton(gc, b) != 0;
    };

    std::uint32_t buttons = 0;
    if (btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  buttons |= kPadL1;
    if (btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= kPadR1;
    if (btn(SDL_CONTROLLER_BUTTON_A))             buttons |= kPadCross;
    if (btn(SDL_CONTROLLER_BUTTON_B))             buttons |= kPadCircle;
    if (btn(SDL_CONTROLLER_BUTTON_X))             buttons |= kPadSquare;
    if (btn(SDL_CONTROLLER_BUTTON_Y))             buttons |= kPadTriangle;
    if (btn(SDL_CONTROLLER_BUTTON_DPAD_UP))       buttons |= kPadUp;
    if (btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN))     buttons |= kPadDown;
    if (btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT))     buttons |= kPadLeft;
    if (btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))    buttons |= kPadRight;
    if (btn(SDL_CONTROLLER_BUTTON_START))         buttons |= kPadOptions;
    if (btn(SDL_CONTROLLER_BUTTON_LEFTSTICK))     buttons |= kPadL3;
    if (btn(SDL_CONTROLLER_BUTTON_RIGHTSTICK))    buttons |= kPadR3;
    if (btn(SDL_CONTROLLER_BUTTON_BACK))          buttons |= kPadTouchPad;

    d.buttons    = buttons;
    d.leftStickX = axisToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
    d.leftStickY = axisToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
    d.rightStickX= axisToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX));
    d.rightStickY= axisToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY));

    const auto lt = triggerToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    const auto rt = triggerToByte(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    d.l2 = lt;
    d.r2 = rt;
    if (lt > 32) buttons |= kPadL2;
    if (rt > 32) buttons |= kPadR2;
    d.buttons = buttons;

    using namespace std::chrono;
    d.timestamp = static_cast<std::uint64_t>(
        duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()).count());

    m_pads[index] = d;
}

void InputManager::updatePadFromKeyboard(int index) {
    // Fallback: keyboard only drives pad 0. Uses SDL_GetKeyboardState, which
    // reflects events already pumped by HostWindow::processEvents().
    if (index != 0) return;
    if (m_controllers[0]) return;    // a real controller is present

    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (!ks) return;

    PadData d;
    d.connected = 1;

    std::uint32_t buttons = 0;
    if (ks[SDL_SCANCODE_UP])     buttons |= kPadUp;
    if (ks[SDL_SCANCODE_DOWN])   buttons |= kPadDown;
    if (ks[SDL_SCANCODE_LEFT])   buttons |= kPadLeft;
    if (ks[SDL_SCANCODE_RIGHT])  buttons |= kPadRight;
    if (ks[SDL_SCANCODE_RETURN]) buttons |= kPadCross;
    if (ks[SDL_SCANCODE_ESCAPE]) buttons |= kPadCircle;
    if (ks[SDL_SCANCODE_X])      buttons |= kPadSquare;
    if (ks[SDL_SCANCODE_Z])      buttons |= kPadTriangle;
    if (ks[SDL_SCANCODE_Q])      buttons |= kPadL1;
    if (ks[SDL_SCANCODE_E])      buttons |= kPadR1;
    if (ks[SDL_SCANCODE_W])      buttons |= kPadL2;
    if (ks[SDL_SCANCODE_R])      buttons |= kPadR2;
    if (ks[SDL_SCANCODE_TAB])    buttons |= kPadOptions;
    if (ks[SDL_SCANCODE_SPACE])  buttons |= kPadTouchPad;

    d.buttons = buttons;

    // WASD for left stick.
    std::uint8_t lx = 128, ly = 128;
    if (ks[SDL_SCANCODE_A]) lx = 0;
    if (ks[SDL_SCANCODE_D]) lx = 255;
    if (ks[SDL_SCANCODE_W]) ly = 0;
    if (ks[SDL_SCANCODE_S]) ly = 255;
    d.leftStickX = lx;
    d.leftStickY = ly;

    using namespace std::chrono;
    d.timestamp = static_cast<std::uint64_t>(
        duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()).count());

    m_pads[index] = d;
}

void InputManager::poll() {
    std::lock_guard lock(m_mutex);

    // Handle SDL hot-plug events already pumped by HostWindow.
    SDL_Event ev;
    while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT,
                          SDL_CONTROLLERDEVICEADDED,
                          SDL_CONTROLLERDEVICEREMOVED) > 0) {
        if (ev.type == SDL_CONTROLLERDEVICEADDED) {
            const int devIdx = ev.cdevice.which;
            if (SDL_IsGameController(devIdx)) {
                auto* gc = SDL_GameControllerOpen(devIdx);
                for (int s = 0; s < kMaxPads && gc; ++s) {
                    if (!m_controllers[s]) {
                        m_controllers[s] = gc;
                        FP4_INFO(LogCategory::Input)
                            << "pad " << s << " hot-plugged";
                        break;
                    }
                }
            }
        } else {
            // Removal: SDL gives us the joystick instance id, not the index.
            for (int s = 0; s < kMaxPads; ++s) {
                auto* gc = static_cast<SDL_GameController*>(m_controllers[s]);
                if (!gc) continue;
                auto* joy = SDL_GameControllerGetJoystick(gc);
                if (SDL_JoystickInstanceID(joy) == ev.cdevice.which) {
                    SDL_GameControllerClose(gc);
                    m_controllers[s] = nullptr;
                    m_pads[s] = PadData{};
                    FP4_INFO(LogCategory::Input)
                        << "pad " << s << " unplugged";
                    break;
                }
            }
        }
    }

    for (int i = 0; i < kMaxPads; ++i) {
        if (m_controllers[i]) updatePadFromController(i);
        else                  updatePadFromKeyboard(i);
    }
}

PadData InputManager::padState(int index) const {
    if (index < 0 || index >= kMaxPads) return PadData{};
    std::lock_guard lock(m_mutex);
    return m_pads[index];
}

bool InputManager::isConnected(int index) const {
    if (index < 0 || index >= kMaxPads) return false;
    std::lock_guard lock(m_mutex);
    return m_pads[index].connected != 0;
}

void InputManager::setVibration(int index, int left, int right) {
    if (index < 0 || index >= kMaxPads) return;
    std::lock_guard lock(m_mutex);
    auto* gc = static_cast<SDL_GameController*>(m_controllers[index]);
    if (!gc) return;
    const Uint16 lo = static_cast<Uint16>((left  & 0xFF) * 257);
    const Uint16 hi = static_cast<Uint16>((right & 0xFF) * 257);
    SDL_GameControllerRumble(gc, lo, hi, 250);
}

} // namespace fusionps4::input
