#pragma once

#include <Arduino.h>

#include "cli/model.h"
#include "cli/pages.h"
#include "cli/screen.h"
#include "events.h"

namespace app
{

    // Serial "BIOS setup" style debug console, layered for maintainability:
    //   cli/model.h  - plain-data snapshot of what is on screen
    //   cli/theme.h  - the 90s BIOS palette
    //   cli/screen.h - positioned drawing primitives over the ANSI library
    //   cli/pages.h  - declarative page layouts (statics + dynamic fields)
    // This class owns input handling, hardware actions and the diff-based
    // renderer: after the initial full redraw (on every terminal connect),
    // only fields whose text or style changed get rewritten, so the terminal
    // does not flicker.
    class SerialCli
    {
    public:
        void begin();
        void handleEvent(const Event &ev);
        void pollInput();

    private:
        enum class InputState : uint8_t
        {
            kIdle = 0,
            kEsc = 1,
            kCsi = 2,
        };

        cli::Screen screen_{&Serial};
        cli::Model model_{};
        cli::Model rendered_{};
        bool connected_ = false;
        bool dirty_ = false;
        bool fullRedrawPending_ = true;
        InputState inputState_ = InputState::kIdle;

        bool updateConnectionState();
        void onConnected();
        void onDisconnected();
        void markDirty();
        void requestFullRedraw();

        void render();
        void drawChrome();
        void drawTabs();
        void drawPage();
        void drawField(const cli::Field &field);
        void drawChangedFields();

        void moveTab(int delta);
        void moveSelection(int delta);
        void activateSelection();
        void toggleOutput(uint8_t index);
        void cycleUltraMode();
        void handleInputByte(char c);
    };

} // namespace app
