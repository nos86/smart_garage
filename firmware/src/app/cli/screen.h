#pragma once

// Positioned drawing primitives over the ANSI library. Every call takes an
// explicit Style and leaves the terminal attributes reset, so drawing order
// never leaks colors between UI elements. field() writes fixed-width cells
// (value plus padding) instead of clear-to-end-of-line, which is what keeps
// partial refreshes free of flicker and border damage.

#include <ansi.h>

#include "theme.h"

namespace app
{
    namespace cli
    {

        struct Rect
        {
            uint8_t x;
            uint8_t y;
            uint8_t w;
            uint8_t h;
        };

        constexpr uint8_t kScreenWidth = 80;
        constexpr uint8_t kScreenHeight = 24;

        class Screen
        {
        public:
            explicit Screen(Stream *stream) : ansi_(stream) {}

            // Hide the cursor, clear and paint the full blue canvas.
            void enter();
            // Restore terminal defaults (colors, cursor).
            void leave();

            void put(uint8_t x, uint8_t y, const char *text, const Style &style);
            // Write text left-aligned in a fixed-width cell, space-padded.
            void field(uint8_t x, uint8_t y, uint8_t width, const char *text, const Style &style);
            void fillRow(uint8_t y, char c, const Style &style);
            void fillRows(uint8_t first, uint8_t last, const Style &style);
            void frame(const Rect &rect, const char *title);

        private:
            void applyStyle(const Style &style);

            ANSI ansi_;
        };

    } // namespace cli
} // namespace app
