#include "dialogs/DialogManager.hpp"

#include "debug/Log.hpp"

#include <chrono>

using fusionps4::debug::LogCategory;

namespace fusionps4::dialogs {

namespace {

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

DialogManager& DialogManager::instance() {
    static DialogManager m;
    return m;
}

void DialogManager::init() {
    std::lock_guard lock(m_mutex);
    while (!m_pending.empty()) m_pending.pop();
    m_active.reset();
    FP4_INFO(LogCategory::Host) << "DialogManager initialized";
}

void DialogManager::shutdown() {
    // Drain the queue, firing Cancel on every pending request so guests
    // do not hang waiting for a callback.
    std::queue<DialogRequest> pending;
    {
        std::lock_guard lock(m_mutex);
        std::swap(pending, m_pending);
        if (m_active) {
            if (m_active->onComplete) m_active->onComplete(DialogResult::Cancel);
            m_active.reset();
        }
    }
    while (!pending.empty()) {
        auto& r = pending.front();
        if (r.onComplete) r.onComplete(DialogResult::Cancel);
        pending.pop();
    }
}

bool DialogManager::submit(DialogRequest req) {
    std::lock_guard lock(m_mutex);
    if (m_pending.size() >= 32) return false;
    m_pending.push(std::move(req));
    return true;
}

void DialogManager::presentNext() {
    // Called with m_mutex held.
    if (m_active || m_pending.empty()) return;
    m_active = std::make_unique<DialogRequest>(std::move(m_pending.front()));
    m_pending.pop();
    m_activeStartMs = nowMs();
    FP4_DEBUG(LogCategory::Host)
        << "Dialog presented: kind=" << static_cast<int>(m_active->kind)
        << " title=\"" << m_active->title << "\"";
}

void DialogManager::tick() {
    std::unique_lock lock(m_mutex);
    presentNext();
    if (!m_active) return;

    // Timeout handling.
    if (m_active->timeoutMs > 0) {
        const auto elapsed = nowMs() - m_activeStartMs;
        if (elapsed >= m_active->timeoutMs) {
            auto done = std::move(m_active);
            m_active.reset();
            lock.unlock();
            if (done->onComplete) done->onComplete(DialogResult::Timeout);
            return;
        }
    }
}

bool DialogManager::hasActiveDialog() const {
    std::lock_guard lock(m_mutex);
    return m_active != nullptr;
}

bool DialogManager::current(ActiveDialogView& out) const {
    std::lock_guard lock(m_mutex);
    if (!m_active) return false;
    out.kind           = m_active->kind;
    out.title          = m_active->title;
    out.message        = m_active->message;
    out.buttons        = m_active->buttons;
    out.defaultButton  = m_active->defaultButton;
    out.imeText        = m_active->imeInitialText;
    return true;
}

bool DialogManager::handleKey(int scancode, bool down) {
    if (!down) return false;
    std::unique_ptr<DialogRequest> done;
    DialogResult result = DialogResult::Ok;
    {
        std::lock_guard lock(m_mutex);
        if (!m_active) return false;

        // SDL scancodes: 40 = Return, 41 = Escape.
        if (scancode == 40) {
            done = std::move(m_active);
            m_active.reset();
            result = DialogResult::Ok;
        } else if (scancode == 41) {
            done = std::move(m_active);
            m_active.reset();
            result = DialogResult::Cancel;
        } else {
            return true;   // swallow other keys while a dialog is up
        }
    }
    if (done && done->onComplete) done->onComplete(result);
    return true;
}

bool DialogManager::handleText(const std::string& text) {
    std::lock_guard lock(m_mutex);
    if (!m_active) return false;
    if (m_active->kind != DialogKind::Ime) return true;
    for (char c : text) {
        if (m_active->imeMaxLength > 0 &&
            m_active->imeInitialText.size() >= m_active->imeMaxLength) break;
        if (c == '\b') {
            if (!m_active->imeInitialText.empty())
                m_active->imeInitialText.pop_back();
        } else if (c >= 0x20) {
            m_active->imeInitialText.push_back(c);
        }
    }
    return true;
}

void DialogManager::cancelActive() {
    std::unique_ptr<DialogRequest> done;
    {
        std::lock_guard lock(m_mutex);
        if (!m_active) return;
        done = std::move(m_active);
        m_active.reset();
    }
    if (done->onComplete) done->onComplete(DialogResult::Cancel);
}

std::size_t DialogManager::queueDepth() const {
    std::lock_guard lock(m_mutex);
    return m_pending.size() + (m_active ? 1 : 0);
}

} // namespace fusionps4::dialogs
