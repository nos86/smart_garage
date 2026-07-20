#include "pages.h"

#include <cstdio>

namespace app
{
    namespace cli
    {

        namespace
        {

            void copyText(char *buf, size_t len, const char *text)
            {
                snprintf(buf, len, "%s", text);
            }

            const char *modeLabel(uint8_t mode)
            {
                switch (mode)
                {
                case kModeOn:
                    return "ON";
                case kModeOff:
                    return "OFF";
                default:
                    return "AUTO";
                }
            }

            ////////////////////////////////////////////////////////////////
            //  Shared formatters and styles

            template <bool Model::*M>
            void fmtActiveIdle(const Model &m, char *buf, size_t len)
            {
                copyText(buf, len, m.*M ? "ACTIVE" : "IDLE");
            }

            template <bool Model::*M>
            Style styleActiveIdle(const Model &m)
            {
                return m.*M ? theme::kActive : theme::kValue;
            }

            void fmtDip(const Model &m, char *buf, size_t len)
            {
                char bits[6];
                for (uint8_t i = 0; i < 5; ++i)
                {
                    bits[4 - i] = ((m.dipValue >> i) & 0x01u) != 0 ? '1' : '0';
                }
                bits[5] = '\0';
                snprintf(buf, len, "%u (b%s)", static_cast<unsigned>(m.dipValue), bits);
            }

            void fmtDistance(const Model &m, char *buf, size_t len)
            {
                if (m.ultrasonicTimeout)
                {
                    copyText(buf, len, "timeout");
                }
                else if (m.distanceCm >= 0.0f)
                {
                    snprintf(buf, len, "%.1f cm", static_cast<double>(m.distanceCm));
                }
                else
                {
                    copyText(buf, len, "n/a");
                }
            }

            void fmtDistanceOrDisabled(const Model &m, char *buf, size_t len)
            {
                if (m.ultrasonicMode == kModeOff)
                {
                    copyText(buf, len, "disabled");
                    return;
                }
                fmtDistance(m, buf, len);
            }

            constexpr const char *kOutputLabels[kOutputCount] = {"Relay 1", "Relay 2", "LED 1", "LED 2", "RGB"};

            bool outputState(const Model &m, uint8_t index)
            {
                switch (index)
                {
                case 0:
                    return m.relay1;
                case 1:
                    return m.relay2;
                case 2:
                    return m.led1;
                case 3:
                    return m.led2;
                default:
                    return m.onboard;
                }
            }

            template <uint8_t I>
            void fmtOutputSummary(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "%-8s %s", kOutputLabels[I], outputState(m, I) ? "ON" : "OFF");
            }

            template <uint8_t I>
            Style styleOutputSummary(const Model &m)
            {
                return outputState(m, I) ? theme::kActive : theme::kValue;
            }

            template <uint8_t I>
            void fmtOutputRow(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, " %-10s [%-3s]", kOutputLabels[I], outputState(m, I) ? "ON" : "OFF");
            }

            template <uint8_t I>
            Style styleOutputRow(const Model &m)
            {
                if (activeSelection(m) == I)
                {
                    return theme::kSelected;
                }
                return outputState(m, I) ? theme::kActive : theme::kValue;
            }

            void fmtUltraMode(const Model &m, char *buf, size_t len)
            {
                copyText(buf, len, modeLabel(m.ultrasonicMode));
            }

            void fmtUltraPower(const Model &m, char *buf, size_t len)
            {
                copyText(buf, len, m.ultrasonicPowered ? "ON" : "OFF");
            }

            Style styleUltraPower(const Model &m)
            {
                return m.ultrasonicPowered ? theme::kActive : theme::kValue;
            }

            void fmtUltraModeRow(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, " %-10s [%-4s]", "Ultrasonic", modeLabel(m.ultrasonicMode));
            }

            Style styleUltraModeRow(const Model &m)
            {
                if (activeSelection(m) == kOutputCount)
                {
                    return theme::kSelected;
                }
                return m.ultrasonicMode == kModeOff ? theme::kValue : theme::kActive;
            }

            ////////////////////////////////////////////////////////////////
            //  STATUS page

            void drawStatusStatic(Screen &screen)
            {
                screen.frame({1, 5, 25, 13}, "INPUTS");
                screen.frame({27, 5, 25, 13}, "OUTPUTS");
                screen.frame({53, 5, 27, 13}, "ULTRASONIC");

                screen.put(3, 8, "I1:", theme::kText);
                screen.put(3, 9, "I2:", theme::kText);
                screen.put(3, 10, "PIR:", theme::kText);
                screen.put(3, 11, "DIP:", theme::kText);

                screen.put(55, 8, "Mode:", theme::kText);
                screen.put(55, 9, "Power:", theme::kText);
                screen.put(55, 10, "Echo:", theme::kText);

                screen.put(55, 13, "Read-only overview.", theme::kHelp);
                screen.put(55, 14, "Tab: action pages.", theme::kHelp);
            }

            constexpr Field kStatusFields[] = {
                {8, 8, 7, fmtActiveIdle<&Model::input1>, styleActiveIdle<&Model::input1>},
                {8, 9, 7, fmtActiveIdle<&Model::input2>, styleActiveIdle<&Model::input2>},
                {8, 10, 7, fmtActiveIdle<&Model::pir>, styleActiveIdle<&Model::pir>},
                {8, 11, 12, fmtDip, nullptr},
                {29, 8, 13, fmtOutputSummary<0>, styleOutputSummary<0>},
                {29, 9, 13, fmtOutputSummary<1>, styleOutputSummary<1>},
                {29, 10, 13, fmtOutputSummary<2>, styleOutputSummary<2>},
                {29, 11, 13, fmtOutputSummary<3>, styleOutputSummary<3>},
                {29, 12, 13, fmtOutputSummary<4>, styleOutputSummary<4>},
                {62, 8, 5, fmtUltraMode, nullptr},
                {62, 9, 4, fmtUltraPower, styleUltraPower},
                {62, 10, 12, fmtDistance, nullptr},
            };

            ////////////////////////////////////////////////////////////////
            //  INPUTS page

            void drawInputsStatic(Screen &screen)
            {
                screen.frame({1, 5, 78, 15}, "STANDARD CMOS FEATURES");
                screen.frame({42, 8, 34, 8}, "NOTES");

                screen.put(4, 8, "Input 1 .......", theme::kText);
                screen.put(4, 9, "Input 2 .......", theme::kText);
                screen.put(4, 10, "PIR sensor ....", theme::kText);
                screen.put(4, 11, "DIP switch ....", theme::kText);
                screen.put(4, 12, "Distance ......", theme::kText);

                screen.put(44, 10, "Inputs are read-only.", theme::kHelp);
                screen.put(44, 11, "Use Left/Right for pages.", theme::kHelp);
                screen.put(44, 12, "PIR is feedback only.", theme::kHelp);
            }

            constexpr Field kInputsFields[] = {
                {20, 8, 7, fmtActiveIdle<&Model::input1>, styleActiveIdle<&Model::input1>},
                {20, 9, 7, fmtActiveIdle<&Model::input2>, styleActiveIdle<&Model::input2>},
                {20, 10, 7, fmtActiveIdle<&Model::pir>, styleActiveIdle<&Model::pir>},
                {20, 11, 12, fmtDip, nullptr},
                {20, 12, 12, fmtDistanceOrDisabled, nullptr},
            };

            ////////////////////////////////////////////////////////////////
            //  OUTPUTS page

            void drawOutputsStatic(Screen &screen)
            {
                screen.frame({1, 5, 42, 15}, "ADVANCED CMOS FEATURES");
                screen.frame({45, 5, 34, 15}, "HELP");

                screen.put(3, 13, "Sensor:", theme::kText);

                screen.put(47, 8, "Up/Down : select item", theme::kHelp);
                screen.put(47, 9, "Enter   : toggle / cycle mode", theme::kHelp);
                screen.put(47, 10, "Tab     : next page", theme::kHelp);
                screen.put(47, 12, "PIR cannot be forced here.", theme::kHelp);
            }

            constexpr Field kOutputsFields[] = {
                {3, 8, 38, fmtOutputRow<0>, styleOutputRow<0>},
                {3, 9, 38, fmtOutputRow<1>, styleOutputRow<1>},
                {3, 10, 38, fmtOutputRow<2>, styleOutputRow<2>},
                {3, 11, 38, fmtOutputRow<3>, styleOutputRow<3>},
                {3, 12, 38, fmtOutputRow<4>, styleOutputRow<4>},
                {3, 14, 38, fmtUltraModeRow, styleUltraModeRow},
            };

        } // namespace

        const Page kPages[kTabCount] = {
            {"STATUS", drawStatusStatic, kStatusFields, sizeof(kStatusFields) / sizeof(Field), 0},
            {"INPUTS", drawInputsStatic, kInputsFields, sizeof(kInputsFields) / sizeof(Field), 0},
            {"OUTPUTS", drawOutputsStatic, kOutputsFields, sizeof(kOutputsFields) / sizeof(Field), kOutputCount + 1},
        };

    } // namespace cli
} // namespace app
