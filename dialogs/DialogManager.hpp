#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace fusionps4::dialogs {

enum class DialogKind : std::uint32_t {
    Common,
    Message,
    Error,
    SaveData,
    Ime,
    Signin,
};

enum class DialogResult : std::int32_t {
    Ok       = 0,
    Cancel   = 1,
    Timeout  = 2,
    Rejected = 3,
};

struct DialogRequest {
    DialogKind                 kind = DialogKind::Message;
    std::string                title;
    std::string                message;
    std::vector<std::string>   buttons;
    std::uint32_t              defaultButton = 0;
    std::uint32_t              timeoutMs     = 0;   // 0 = no timeout
    // IME-specific.
    std::string                imeInitialText;
    std::uint32_t              imeMaxLength = 0;
    // Save data dialog specific.
    std::string                saveDirName;
    std::string                saveTitle;
    std::string                saveSubtitle;
    std::string                saveDetail;
    // Async completion, invoked by the dialog manager.
    std::function<void(DialogResult)> onComplete;
};

// Centralised dialog lifecycle. Guest code submits requests; the runtime
// pumps the manager each frame; the overlay renders the active dialog.
//
// The guest is never allowed to draw its own dialog: the request goes to
// this queue, and the runtime's overlay renders it on the HostWindow. This
// is the concrete realisation of the "runtime owns the window" invariant.
class DialogManager {
public:
    static DialogManager& instance();

    void init();
    void shutdown();

    // Submit a request. Returns immediately; completion fires from tick().
    // If the queue is full, returns false and the caller should retry.
    bool submit(DialogRequest req);

    // Called from Runtime::tick. Pumps the queue, processes the active
    // dialog against user input (currently: Enter = OK, Escape = Cancel),
    // and fires the completion when the dialog closes.
    void tick();

    // Information the overlay renderer needs.
    bool hasActiveDialog() const;
    struct ActiveDialogView {
        DialogKind              kind;
        std::string             title;
        std::string             message;
        std::vector<std::string> buttons;
        std::uint32_t           defaultButton;
        std::string             imeText;
    };
    bool current(ActiveDialogView& out) const;

    // Called by the input layer to feed dialog keystrokes. Return true if
    // the key was consumed by the active dialog (so the guest does not
    // also see it).
    bool handleKey(int scancode, bool down);
    bool handleText(const std::string& text);

    // Called when the guest requests a timeout-based cancellation.
    void cancelActive();

    std::size_t queueDepth() const;

private:
    DialogManager() = default;

    void presentNext();

    mutable std::mutex           m_mutex;
    std::queue<DialogRequest>    m_pending;
    std::unique_ptr<DialogRequest> m_active;
    std::uint64_t                m_activeStartMs = 0;
};

} // namespace fusionps4::dialogs
