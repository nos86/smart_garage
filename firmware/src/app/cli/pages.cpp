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

            ////////////////////////////////////////////////////////////////
            //  RTOS page

            const char *taskStateLabel(uint8_t state)
            {
                switch (state)
                {
                case kTaskStateRunning:
                    return "RUN";
                case kTaskStateReady:
                    return "READY";
                case kTaskStateBlocked:
                    return "BLOCK";
                case kTaskStateSuspended:
                    return "SUSP";
                case kTaskStateDeleted:
                    return "DEL";
                default:
                    return "INV";
                }
            }

            template <uint8_t I>
            void fmtRtosTaskRow(const Model &m, char *buf, size_t len)
            {
                if (I >= m.rtosTaskCount)
                {
                    copyText(buf, len, "");
                    return;
                }
                const RtosTaskInfo &task = m.rtosTasks[I];
                // Each value is self-labelled (Pri:/Stack:/CPU:) instead of
                // relying on a header row, so the full task name (up to
                // kTaskNameMax-1 chars -- FreeRTOS never gives us more) is
                // never squeezed out to make column room.
                snprintf(buf, len, "%-10s %-6s Pri:%-2u Stack:%-5lu CPU:%3u%%", task.name, taskStateLabel(task.state),
                         static_cast<unsigned>(task.priority), static_cast<unsigned long>(task.stackFreeWords),
                         static_cast<unsigned>(task.cpuPercent));
            }

            template <uint8_t I>
            Style styleRtosTaskRow(const Model &m)
            {
                if (I >= m.rtosTaskCount)
                {
                    return theme::kValue;
                }
                return m.rtosTasks[I].state == kTaskStateRunning ? theme::kActive : theme::kValue;
            }

            void fmtRtosTaskCount(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "Tasks : %u", static_cast<unsigned>(m.rtosTaskCount));
            }

            void fmtRtosHeapFree(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "Free  : %4lu kB", static_cast<unsigned long>(m.rtosFreeHeapBytes / 1024));
            }

            void fmtRtosHeapTotal(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "Total : %4lu kB", static_cast<unsigned long>(m.rtosTotalHeapBytes / 1024));
            }

            void fmtRtosUptime(const Model &m, char *buf, size_t len)
            {
                uint32_t totalSec = m.rtosUptimeMs / 1000;
                uint32_t hours = totalSec / 3600;
                uint32_t minutes = (totalSec % 3600) / 60;
                uint32_t seconds = totalSec % 60;
                snprintf(buf, len, "Up    : %lu:%02lu:%02lu", static_cast<unsigned long>(hours),
                         static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
            }

            // Two side-by-side frames, same pattern as the OUTPUTS page's
            // "ADVANCED CMOS FEATURES" + "HELP" boxes. Left one is a
            // single-column, "top"-style task list -- 50 of 78 cols (~64%),
            // wide enough that a full task name never gets clipped; right
            // one is the heap/uptime summary (~33%).
            constexpr uint8_t kRtosListX = 3;
            constexpr uint8_t kRtosListW = 50;
            constexpr uint8_t kRtosSidebarBorderX = kRtosListW + 3; // 2-col gap, as on OUTPUTS
            constexpr uint8_t kRtosSidebarX = kRtosSidebarBorderX + 2;

            void drawRtosStatic(Screen &screen)
            {
                screen.frame({1, 5, kRtosListW, 17}, "TASKS");
                screen.frame({kRtosSidebarBorderX, 5, static_cast<uint8_t>(78 - kRtosSidebarBorderX + 1), 17},
                              "SYSTEM");

                screen.put(kRtosSidebarX, 12, "Stack: free words", theme::kHelp);
                screen.put(kRtosSidebarX, 13, "CPU%: run-time", theme::kHelp);
                screen.put(kRtosSidebarX, 14, "share since boot", theme::kHelp);
            }

            constexpr Field kRtosFields[] = {
                {kRtosListX, 6, 46, fmtRtosTaskRow<0>, styleRtosTaskRow<0>},
                {kRtosListX, 7, 46, fmtRtosTaskRow<1>, styleRtosTaskRow<1>},
                {kRtosListX, 8, 46, fmtRtosTaskRow<2>, styleRtosTaskRow<2>},
                {kRtosListX, 9, 46, fmtRtosTaskRow<3>, styleRtosTaskRow<3>},
                {kRtosListX, 10, 46, fmtRtosTaskRow<4>, styleRtosTaskRow<4>},
                {kRtosListX, 11, 46, fmtRtosTaskRow<5>, styleRtosTaskRow<5>},
                {kRtosListX, 12, 46, fmtRtosTaskRow<6>, styleRtosTaskRow<6>},
                {kRtosListX, 13, 46, fmtRtosTaskRow<7>, styleRtosTaskRow<7>},
                {kRtosListX, 14, 46, fmtRtosTaskRow<8>, styleRtosTaskRow<8>},
                {kRtosListX, 15, 46, fmtRtosTaskRow<9>, styleRtosTaskRow<9>},
                {kRtosListX, 16, 46, fmtRtosTaskRow<10>, styleRtosTaskRow<10>},
                {kRtosListX, 17, 46, fmtRtosTaskRow<11>, styleRtosTaskRow<11>},
                {kRtosSidebarX, 7, 22, fmtRtosTaskCount, nullptr},
                {kRtosSidebarX, 8, 22, fmtRtosHeapFree, nullptr},
                {kRtosSidebarX, 9, 22, fmtRtosHeapTotal, nullptr},
                {kRtosSidebarX, 10, 22, fmtRtosUptime, nullptr},
            };

            static_assert(kMaxRtosTasks == 12, "kRtosFields task rows must match kMaxRtosTasks");

            ////////////////////////////////////////////////////////////////
            //  CAN page (transceiver/HAL level -- see CANOPEN for protocol
            //  state, added once that layer exists)

            void fmtCanLinkState(const Model &m, char *buf, size_t len)
            {
                copyText(buf, len, m.canOk ? "UP" : "DOWN");
            }

            Style styleCanLinkState(const Model &m)
            {
                return m.canOk ? theme::kActive : theme::kValue;
            }

            void fmtCanRxCount(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "%lu", static_cast<unsigned long>(m.canRxCount));
            }

            void fmtCanErrorFlags(const Model &m, char *buf, size_t len)
            {
                if (m.canErrorFlags == 0)
                {
                    copyText(buf, len, "none");
                    return;
                }
                snprintf(buf, len, "0x%02X", static_cast<unsigned>(m.canErrorFlags));
            }

            Style styleCanErrorFlags(const Model &m)
            {
                return m.canErrorFlags == 0 ? theme::kValue : theme::kActive;
            }

            void drawCanStatic(Screen &screen)
            {
                screen.frame({1, 5, 40, 13}, "MCP25625 TRANSCEIVER");

                screen.put(3, 8, "Link:", theme::kText);
                screen.put(3, 9, "RX frames:", theme::kText);
                screen.put(3, 10, "Errors:", theme::kText);

                screen.put(3, 15, "Read-only HAL diagnostics.", theme::kHelp);
                screen.put(3, 16, "Loopback self-test runs at boot.", theme::kHelp);
            }

            constexpr Field kCanFields[] = {
                {14, 8, 6, fmtCanLinkState, styleCanLinkState},
                {14, 9, 10, fmtCanRxCount, nullptr},
                {14, 10, 10, fmtCanErrorFlags, styleCanErrorFlags},
            };

            ////////////////////////////////////////////////////////////////
            //  CANOPEN page (protocol level -- see CAN for raw transceiver
            //  diagnostics)

            const char *nmtStateLabel(uint8_t state)
            {
                switch (state)
                {
                case kNmtOperational:
                    return "OPERATIONAL";
                case kNmtStopped:
                    return "STOPPED";
                case kNmtPreOperational:
                    return "PRE-OPERATIONAL";
                default:
                    return "INITIALIZING";
                }
            }

            void fmtNmtState(const Model &m, char *buf, size_t len)
            {
                copyText(buf, len, nmtStateLabel(m.nmtState));
            }

            Style styleNmtState(const Model &m)
            {
                return m.nmtState == kNmtOperational ? theme::kActive : theme::kValue;
            }

            void fmtCanopenNodeId(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "%u", static_cast<unsigned>(m.canopenNodeId));
            }

            void fmtHeartbeatCount(const Model &m, char *buf, size_t len)
            {
                snprintf(buf, len, "%lu", static_cast<unsigned long>(m.heartbeatCount));
            }

            void fmtLastHeartbeat(const Model &m, char *buf, size_t len)
            {
                if (m.heartbeatCount == 0)
                {
                    copyText(buf, len, "n/a");
                    return;
                }
                // Uptime timestamp of the last heartbeat, not a live
                // countdown -- this layer has no Arduino/millis() dependency
                // (see file header), same reasoning as fmtRtosUptime.
                snprintf(buf, len, "t+%lu ms", static_cast<unsigned long>(m.lastHeartbeatMs));
            }

            void drawCanopenStatic(Screen &screen)
            {
                screen.frame({1, 5, 40, 13}, "CANOPEN NODE");

                screen.put(3, 8, "NMT state:", theme::kText);
                screen.put(3, 9, "Node-ID:", theme::kText);
                screen.put(3, 10, "Heartbeats sent:", theme::kText);
                screen.put(3, 11, "Last heartbeat:", theme::kText);

                screen.put(3, 15, "Read-only protocol diagnostics.", theme::kHelp);
                screen.put(3, 16, "NMT self-starts -- no master needed.", theme::kHelp);
            }

            constexpr Field kCanopenFields[] = {
                {20, 8, 16, fmtNmtState, styleNmtState},
                {20, 9, 6, fmtCanopenNodeId, nullptr},
                {20, 10, 10, fmtHeartbeatCount, nullptr},
                {20, 11, 16, fmtLastHeartbeat, nullptr},
            };

        } // namespace

        const Page kPages[kTabCount] = {
            {"STATUS", drawStatusStatic, kStatusFields, sizeof(kStatusFields) / sizeof(Field), 0},
            {"INPUTS", drawInputsStatic, kInputsFields, sizeof(kInputsFields) / sizeof(Field), 0},
            {"OUTPUTS", drawOutputsStatic, kOutputsFields, sizeof(kOutputsFields) / sizeof(Field), kOutputCount + 1},
            {"RTOS", drawRtosStatic, kRtosFields, sizeof(kRtosFields) / sizeof(Field), 0},
            {"CAN", drawCanStatic, kCanFields, sizeof(kCanFields) / sizeof(Field), 0},
            {"CANOPEN", drawCanopenStatic, kCanopenFields, sizeof(kCanopenFields) / sizeof(Field), 0},
        };

    } // namespace cli
} // namespace app
