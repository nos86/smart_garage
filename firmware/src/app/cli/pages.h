#pragma once

// Declarative page layouts. Each tab is a Page: static chrome (frames,
// labels, help text) drawn once on page entry, plus a table of dynamic
// Fields. A Field is declared exactly once — position, width, formatter,
// style — and the renderer uses the same declaration for both the full
// draw and the diff-based partial refresh.

#include <cstddef>
#include <cstdint>

#include "model.h"
#include "screen.h"
#include "theme.h"

namespace app
{
    namespace cli
    {

        // Longest text a field formatter may produce, including the NUL.
        constexpr size_t kFieldTextMax = 32;

        using FormatFn = void (*)(const Model &model, char *buf, size_t len);
        using StyleFn = Style (*)(const Model &model);
        using StaticFn = void (*)(Screen &screen);

        struct Field
        {
            uint8_t x;
            uint8_t y;
            uint8_t w;
            FormatFn format;
            StyleFn style; // nullptr -> theme::kValue
        };

        struct Page
        {
            const char *tabLabel;
            StaticFn drawStatic;
            const Field *fields;
            uint8_t fieldCount;
            uint8_t selectableCount;
        };

        extern const Page kPages[kTabCount];

    } // namespace cli
} // namespace app
