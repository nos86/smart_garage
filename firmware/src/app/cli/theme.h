#pragma once

// Late-90s Award/AMI BIOS palette: blue canvas, yellow accents, red
// selection bar. Every UI element draws through a named Style so the look
// is defined in exactly one place.

#include <ansi.h>

#include <cstdint>

namespace app
{
    namespace cli
    {

        struct Style
        {
            uint8_t fg;
            uint8_t bg;
            bool bold;
        };

        inline bool operator==(const Style &a, const Style &b)
        {
            return a.fg == b.fg && a.bg == b.bg && a.bold == b.bold;
        }

        inline bool operator!=(const Style &a, const Style &b)
        {
            return !(a == b);
        }

        namespace theme
        {

            constexpr Style kText{ANSI::white, ANSI::blue, false};      // static labels
            constexpr Style kValue{ANSI::white, ANSI::blue, true};      // dynamic values
            constexpr Style kActive{ANSI::yellow, ANSI::blue, true};    // "ON"/active values
            constexpr Style kSelected{ANSI::yellow, ANSI::red, true};   // selection bar
            constexpr Style kFrame{ANSI::yellow, ANSI::blue, false};    // box borders
            constexpr Style kFrameTitle{ANSI::yellow, ANSI::blue, true};
            constexpr Style kHelp{ANSI::cyan + ANSI::bright, ANSI::blue, false};
            constexpr Style kHeader{ANSI::yellow, ANSI::blue, true};
            constexpr Style kTabIdle{ANSI::white, ANSI::blue, false};
            constexpr Style kTabActive{ANSI::yellow, ANSI::red, true};
            constexpr Style kFooterKey{ANSI::yellow, ANSI::blue, true};
            constexpr Style kFooterText{ANSI::white, ANSI::blue, false};

        } // namespace theme

    } // namespace cli
} // namespace app
