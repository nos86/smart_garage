#include "screen.h"

namespace app
{
    namespace cli
    {

        void Screen::applyStyle(const Style &style)
        {
            ansi_.normal();
            ansi_.color(style.fg, style.bg);
            if (style.bold)
            {
                ansi_.bold();
            }
        }

        void Screen::enter()
        {
            ansi_.cursorHide();
            ansi_.clearScreen();
            fillRows(1, kScreenHeight, theme::kText);
        }

        void Screen::leave()
        {
            ansi_.normal();
            ansi_.cursorShow();
        }

        void Screen::put(uint8_t x, uint8_t y, const char *text, const Style &style)
        {
            applyStyle(style);
            ansi_.gotoXY(x, y);
            ansi_.print(text);
            ansi_.normal();
        }

        void Screen::field(uint8_t x, uint8_t y, uint8_t width, const char *text, const Style &style)
        {
            applyStyle(style);
            ansi_.gotoXY(x, y);
            uint8_t written = 0;
            for (; text[written] != '\0' && written < width; ++written)
            {
                ansi_.print(text[written]);
            }
            if (written < width)
            {
                ansi_.repeat(' ', static_cast<uint8_t>(width - written));
            }
            ansi_.normal();
        }

        void Screen::fillRow(uint8_t y, char c, const Style &style)
        {
            applyStyle(style);
            ansi_.gotoXY(1, y);
            // Skip the bottom-right cell: writing it makes some terminals scroll.
            ansi_.repeat(c, y == kScreenHeight ? kScreenWidth - 1 : kScreenWidth);
            ansi_.normal();
        }

        void Screen::fillRows(uint8_t first, uint8_t last, const Style &style)
        {
            for (uint8_t row = first; row <= last; ++row)
            {
                fillRow(row, ' ', style);
            }
        }

        void Screen::frame(const Rect &rect, const char *title)
        {
            applyStyle(theme::kFrame);

            ansi_.gotoXY(rect.x, rect.y);
            ansi_.print('+');
            ansi_.repeat('-', static_cast<uint8_t>(rect.w - 2));
            ansi_.print('+');

            for (uint8_t row = rect.y + 1; row < rect.y + rect.h - 1; ++row)
            {
                ansi_.gotoXY(rect.x, row);
                ansi_.print('|');
                ansi_.gotoXY(static_cast<uint8_t>(rect.x + rect.w - 1), row);
                ansi_.print('|');
            }

            ansi_.gotoXY(rect.x, static_cast<uint8_t>(rect.y + rect.h - 1));
            ansi_.print('+');
            ansi_.repeat('-', static_cast<uint8_t>(rect.w - 2));
            ansi_.print('+');

            applyStyle(theme::kFrameTitle);
            ansi_.gotoXY(static_cast<uint8_t>(rect.x + 2), rect.y);
            ansi_.print(' ');
            ansi_.print(title);
            ansi_.print(' ');
            ansi_.normal();
        }

    } // namespace cli
} // namespace app
