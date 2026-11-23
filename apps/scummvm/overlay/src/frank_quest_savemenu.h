/*
 * CMM2: frank-quest has no ScummVM GUI, so F5 in a SCUMM game opened nothing and no one could
 * find the way to save (Alt-1 to 9, and Ctrl-1 to 9 to restore: a user's report, 7 Nov 2026).
 * This is a menu of save slots drawn over the game, and a message box for what the player must see.
 */
#ifndef FRANK_QUEST_SAVEMENU_H
#define FRANK_QUEST_SAVEMENU_H

#include "gui/dialog.h"

namespace Scumm {
class ScummEngine;
}

/* a box of text over the game, one line for each \n (blank ones left out), gone after ms or at a key */
void frankQuestMessage(const char *text, uint32 ms);

/* F5's menu in a SCUMM game: run it with ScummEngine::runDialog, which pauses the game */
class FrankQuestSaveMenu : public GUI::Dialog {
public:
	explicit FrankQuestSaveMenu(Scumm::ScummEngine *vm) : GUI::Dialog(0, 0, 0, 0), _vm(vm) {}
	virtual int runModal();

private:
	Scumm::ScummEngine *_vm;
};

#endif
