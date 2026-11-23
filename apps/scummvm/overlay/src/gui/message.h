/* Cabal - Legacy Game Implementations
 *
 * Minimal Message Dialog stub for embedded platforms
 *
 * CMM2: frank-quest's, which only printed, but for what a player must see: that a save or restore failed,
 * and the timed message, which is SCUMM's "saved" (frank_quest_savemenu.cpp). The other messages still only
 * print, as several engines give one at the start of a game that was never seen here.
 */

#ifndef GUI_MESSAGE_H
#define GUI_MESSAGE_H

#include "common/scummsys.h"
#include "common/str.h"
#include "gui/dialog.h"

void frankQuestMessage(const char *text, uint32 ms);

namespace GUI {

/**
 * Message dialog stub - just prints to console on embedded
 */
class MessageDialog : public Dialog {
public:
    MessageDialog(const Common::String &message, const char *defaultButton = "OK", const char *altButton = nullptr)
        : Dialog(0, 0, 0, 0), _message(message), _ms(message.hasPrefix("Failed to") ? 4000 : 0) {
        // On embedded, just print the message
        printf("MessageDialog: %s\n", message.c_str());
    }
    virtual ~MessageDialog() {}

    virtual int runModal() {
        if (_ms)
            frankQuestMessage(_message.c_str(), _ms);
        return Dialog::runModal();
    }

protected:
    Common::String _message;
    uint32 _ms;
};

/**
 * Timed message dialog stub - auto-dismisses after timeout on real ScummVM
 */
class TimedMessageDialog : public MessageDialog {
public:
    TimedMessageDialog(const Common::String &message, uint32 duration)
        : MessageDialog(message) {
        _ms = duration;
    }
    virtual ~TimedMessageDialog() {}
};

} // End of namespace GUI

#endif // GUI_MESSAGE_H
