#include "serial_cli.h"

#include <Arduino.h>

#include <cstring>

#include <mcp2515.h>

#include "board.h"
#include "cli/theme.h"
#include "tasks.h"

namespace app
{

    namespace
    {

        namespace theme = cli::theme;

        // The model stores ForceMode values as raw bytes so the cli/ layer
        // needs no FreeRTOS includes; keep the two definitions in sync.
        static_assert(cli::kModeAuto == static_cast<uint8_t>(ForceMode::kAuto), "mode value mismatch");
        static_assert(cli::kModeOff == static_cast<uint8_t>(ForceMode::kOff), "mode value mismatch");
        static_assert(cli::kModeOn == static_cast<uint8_t>(ForceMode::kOn), "mode value mismatch");

        // Same reasoning for eTaskState (FreeRTOS task.h).
        static_assert(cli::kTaskStateRunning == static_cast<uint8_t>(eRunning), "task state mismatch");
        static_assert(cli::kTaskStateReady == static_cast<uint8_t>(eReady), "task state mismatch");
        static_assert(cli::kTaskStateBlocked == static_cast<uint8_t>(eBlocked), "task state mismatch");
        static_assert(cli::kTaskStateSuspended == static_cast<uint8_t>(eSuspended), "task state mismatch");
        static_assert(cli::kTaskStateDeleted == static_cast<uint8_t>(eDeleted), "task state mismatch");
        static_assert(cli::kTaskStateInvalid == static_cast<uint8_t>(eInvalid), "task state mismatch");

        // Same reasoning for MCP2515::EFLG (autowp-mcp2515's mcp2515.h).
        static_assert(cli::kCanEflgRx1Ovr == static_cast<uint8_t>(MCP2515::EFLG_RX1OVR), "EFLG bit mismatch");
        static_assert(cli::kCanEflgRx0Ovr == static_cast<uint8_t>(MCP2515::EFLG_RX0OVR), "EFLG bit mismatch");
        static_assert(cli::kCanEflgTxbo == static_cast<uint8_t>(MCP2515::EFLG_TXBO), "EFLG bit mismatch");
        static_assert(cli::kCanEflgTxep == static_cast<uint8_t>(MCP2515::EFLG_TXEP), "EFLG bit mismatch");
        static_assert(cli::kCanEflgRxep == static_cast<uint8_t>(MCP2515::EFLG_RXEP), "EFLG bit mismatch");
        static_assert(cli::kCanEflgTxwar == static_cast<uint8_t>(MCP2515::EFLG_TXWAR), "EFLG bit mismatch");
        static_assert(cli::kCanEflgRxwar == static_cast<uint8_t>(MCP2515::EFLG_RXWAR), "EFLG bit mismatch");
        static_assert(cli::kCanEflgEwarn == static_cast<uint8_t>(MCP2515::EFLG_EWARN), "EFLG bit mismatch");

        // How often the RTOS page's task table and heap stats are resampled
        // while it is on screen; polling more often than this just burns
        // cycles on numbers that will not visibly change.
        constexpr uint32_t kRtosStatsPeriodMs = 500;

        // 1 s, not the 500 ms used for RTOS stats: the RX/TX rate fields are
        // a frames-per-second figure, so the sampling window needs to be a
        // full second for the number on screen to mean what its label says.
        constexpr uint32_t kCanStatsPeriodMs = 1000;

        constexpr uint8_t kHeaderRow = 1;
        constexpr uint8_t kTopRuleRow = 2;
        constexpr uint8_t kTabsRow = 3;
        constexpr uint8_t kContentTop = 4;
        constexpr uint8_t kContentBottom = 21;
        constexpr uint8_t kBottomRuleRow = 22;
        constexpr uint8_t kFooterRow = 23;

        constexpr uint8_t kTabStep = 14;

        constexpr char kTitle[] = "SMART GARAGE - CMOS SETUP UTILITY";

        struct KeyHint
        {
            const char *key;
            const char *desc;
        };

        constexpr KeyHint kKeyHints[] = {
            {"Tab", "Next Page"},
            {"Arrows", "Select Item"},
            {"Enter", "Execute"},
            {"R", "Redraw"},
        };

        cli::Style fieldStyle(const cli::Field &field, const cli::Model &model)
        {
            return field.style != nullptr ? field.style(model) : theme::kValue;
        }

    } // namespace

    void SerialCli::begin()
    {
        connected_ = false;
        inputState_ = InputState::kIdle;
        updateConnectionState();
    }

    bool SerialCli::updateConnectionState()
    {
        bool nowConnected = Serial && Serial.dtr();
        if (nowConnected == connected_)
        {
            return nowConnected;
        }

        connected_ = nowConnected;
        if (connected_)
        {
            onConnected();
        }
        else
        {
            onDisconnected();
        }

        return connected_;
    }

    void SerialCli::onConnected()
    {
        model_.dipValue = board::dipSwitch.read();
        model_.ultrasonicMode = debugControls.ultrasonicMode.load();
        model_.ultrasonicPowered = board::ultrasonic.isPowered();
        model_.canOk = board::canOk();
        model_.canBitrateKbps = static_cast<uint32_t>(board::can.bitrateKbps());
        model_.canComputedBaudrateBps = board::can.computedBaudrateBps();
        requestFullRedraw();
        render();
    }

    void SerialCli::onDisconnected()
    {
        dirty_ = false;
        screen_.leave();
    }

    void SerialCli::markDirty()
    {
        dirty_ = true;
    }

    void SerialCli::requestFullRedraw()
    {
        fullRedrawPending_ = true;
        markDirty();
    }

    //////////////////////////////////////////////////////////////////////
    //  Rendering

    void SerialCli::drawChrome()
    {
        screen_.put(static_cast<uint8_t>((cli::kScreenWidth - strlen(kTitle)) / 2 + 1), kHeaderRow, kTitle,
                    theme::kHeader);
        screen_.fillRow(kTopRuleRow, '=', theme::kFrame);
        screen_.fillRow(kBottomRuleRow, '=', theme::kFrame);

        uint8_t x = 3;
        for (const KeyHint &hint : kKeyHints)
        {
            screen_.put(x, kFooterRow, hint.key, theme::kFooterKey);
            x += strlen(hint.key);
            screen_.put(x, kFooterRow, " : ", theme::kFooterText);
            x += 3;
            screen_.put(x, kFooterRow, hint.desc, theme::kFooterText);
            x += strlen(hint.desc) + 3;
        }
    }

    void SerialCli::drawTabs()
    {
        screen_.fillRow(kTabsRow, ' ', theme::kText);

        uint8_t x = 2;
        for (uint8_t i = 0; i < cli::kTabCount; ++i)
        {
            char label[16];
            snprintf(label, sizeof label, " %s ", cli::kPages[i].tabLabel);
            screen_.put(x, kTabsRow, label, i == model_.activeTab ? theme::kTabActive : theme::kTabIdle);
            x += kTabStep;
        }
    }

    void SerialCli::drawField(const cli::Field &field)
    {
        char text[cli::kFieldTextMax];
        field.format(model_, text, sizeof text);
        screen_.field(field.x, field.y, field.w, text, fieldStyle(field, model_));
    }

    void SerialCli::drawPage()
    {
        const cli::Page &page = cli::kPages[model_.activeTab];
        page.drawStatic(screen_);
        for (uint8_t i = 0; i < page.fieldCount; ++i)
        {
            drawField(page.fields[i]);
        }
    }

    void SerialCli::drawChangedFields()
    {
        const cli::Page &page = cli::kPages[model_.activeTab];
        char before[cli::kFieldTextMax];
        char after[cli::kFieldTextMax];

        for (uint8_t i = 0; i < page.fieldCount; ++i)
        {
            const cli::Field &field = page.fields[i];
            field.format(rendered_, before, sizeof before);
            field.format(model_, after, sizeof after);
            cli::Style style = fieldStyle(field, model_);
            if (strcmp(before, after) == 0 && style == fieldStyle(field, rendered_))
            {
                continue;
            }
            screen_.field(field.x, field.y, field.w, after, style);
        }
    }

    void SerialCli::render()
    {
        if (!connected_ || !dirty_)
        {
            return;
        }

        model_.ultrasonicMode = debugControls.ultrasonicMode.load();
        model_.ultrasonicPowered = board::ultrasonic.isPowered();
        dirty_ = false;

        if (fullRedrawPending_)
        {
            fullRedrawPending_ = false;
            screen_.enter();
            drawChrome();
            drawTabs();
            drawPage();
        }
        else if (model_.activeTab != rendered_.activeTab)
        {
            // Page switch: repaint tabs and content area only; header,
            // rules and footer stay untouched.
            drawTabs();
            screen_.fillRows(kContentTop, kContentBottom, theme::kText);
            drawPage();
        }
        else
        {
            drawChangedFields();
        }

        rendered_ = model_;
    }

    //////////////////////////////////////////////////////////////////////
    //  Navigation and actions

    void SerialCli::moveTab(int delta)
    {
        int count = cli::kTabCount;
        model_.activeTab = static_cast<uint8_t>((model_.activeTab + delta + count) % count);

        uint8_t selectable = cli::kPages[model_.activeTab].selectableCount;
        uint8_t &current = model_.selection[model_.activeTab];
        if (selectable == 0)
        {
            current = 0;
        }
        else if (current >= selectable)
        {
            current = selectable - 1;
        }
        markDirty();

        if (model_.activeTab == cli::kTabRtos)
        {
            // Sample immediately so the page does not show stale/zeroed
            // stats until the next periodic refresh in pollInput().
            rtosStatsLastMs_ = millis();
            refreshRtosStats();
        }
        else if (model_.activeTab == cli::kTabCan)
        {
            // Sample immediately, same reasoning as the RTOS branch above.
            // canStatsLastMs_ is whatever it was at the last sample (0 if
            // this is the first visit), so the rate this computes is the
            // average since then -- correct, just not "instantaneous" if the
            // page was not visited in a while.
            refreshCanStats();
        }
    }

    void SerialCli::moveSelection(int delta)
    {
        uint8_t selectable = cli::kPages[model_.activeTab].selectableCount;
        if (selectable == 0)
        {
            return;
        }

        uint8_t &current = model_.selection[model_.activeTab];
        int next = static_cast<int>(current) + delta;
        if (next < 0)
        {
            next = selectable - 1;
        }
        else if (next >= selectable)
        {
            next = 0;
        }

        current = static_cast<uint8_t>(next);
        markDirty();
    }

    void SerialCli::activateSelection()
    {
        switch (model_.activeTab)
        {
        case cli::kTabOutputs:
        {
            uint8_t selection = model_.selection[cli::kTabOutputs];
            if (selection < cli::kOutputCount)
            {
                toggleOutput(selection);
            }
            else
            {
                cycleUltraMode();
            }
            break;
        }
        default:
            break;
        }
    }

    void SerialCli::cycleUltraMode()
    {
        // Cycle in UI order: AUTO -> ON -> OFF -> AUTO.
        uint8_t next = cli::kModeAuto;
        switch (debugControls.ultrasonicMode.load())
        {
        case cli::kModeAuto:
            next = cli::kModeOn;
            break;
        case cli::kModeOn:
            next = cli::kModeOff;
            break;
        default:
            next = cli::kModeAuto;
            break;
        }

        debugControls.ultrasonicMode.store(next);
        markDirty();
    }

    void SerialCli::toggleOutput(uint8_t index)
    {
        switch (index)
        {
        case 0:
            board::relay1.set(!model_.relay1);
            model_.relay1 = board::relay1.isOn();
            break;
        case 1:
            board::relay2.set(!model_.relay2);
            model_.relay2 = board::relay2.isOn();
            break;
        case 2:
            model_.onboard = !model_.onboard;
            board::onboardLed.setColor(0, model_.onboard ? 32 : 0, 0);
            break;
        default:
            break;
        }

        markDirty();
    }

    //////////////////////////////////////////////////////////////////////
    //  RTOS task analysis

    void SerialCli::refreshRtosStats()
    {
        TaskStatus_t statuses[cli::kMaxRtosTasks];
        uint32_t totalRunTime = 0;
        UBaseType_t count = uxTaskGetSystemState(statuses, cli::kMaxRtosTasks, &totalRunTime);

        model_.rtosTaskCount = static_cast<uint8_t>(count);
        for (UBaseType_t i = 0; i < count; ++i)
        {
            cli::RtosTaskInfo &info = model_.rtosTasks[i];
            snprintf(info.name, sizeof info.name, "%s", statuses[i].pcTaskName);
            info.state = static_cast<uint8_t>(statuses[i].eCurrentState);
            info.priority = static_cast<uint8_t>(statuses[i].uxCurrentPriority);
            info.stackFreeWords = static_cast<uint32_t>(statuses[i].usStackHighWaterMark);
            // Share of total run time since boot, not a live/instantaneous
            // load -- configGENERATE_RUN_TIME_STATS only hands us cumulative
            // counters, no cheap way to get a windowed rate here.
            info.cpuPercent = totalRunTime > 0 ? static_cast<uint8_t>(
                                                      (static_cast<uint64_t>(statuses[i].ulRunTimeCounter) * 100) /
                                                      totalRunTime)
                                                : 0;
        }
        for (UBaseType_t i = count; i < cli::kMaxRtosTasks; ++i)
        {
            model_.rtosTasks[i] = cli::RtosTaskInfo{};
        }

        // This port's FreeRTOS heap wrapper (heap_3a.c) does not implement
        // xPortGetFreeHeapSize()/xPortGetMinimumEverFreeHeapSize() -- it
        // just forwards to newlib malloc/free. Use the arduino-pico core's
        // own heap accounting instead.
        model_.rtosFreeHeapBytes = static_cast<uint32_t>(rp2040.getFreeHeap());
        model_.rtosTotalHeapBytes = static_cast<uint32_t>(rp2040.getTotalHeap());
        model_.rtosUptimeMs = millis();

        markDirty();
    }

    //////////////////////////////////////////////////////////////////////
    //  CAN transceiver diagnostics

    void SerialCli::refreshCanStats()
    {
        uint32_t now = millis();
        uint32_t rx = board::can.rxFrameCount();
        uint32_t tx = board::can.txFrameCount();

        uint32_t dtMs = now - canStatsLastMs_;
        if (dtMs > 0)
        {
            model_.canRxRatePs = ((rx - canLastRxCount_) * 1000) / dtMs;
            model_.canTxRatePs = ((tx - canLastTxCount_) * 1000) / dtMs;
        }

        model_.canRxCount = rx;
        model_.canTxCount = tx;
        model_.canTxErrorCount = board::can.txErrorCount();
        model_.canRxErrorCount = board::can.rxErrorCount();

        canStatsLastMs_ = now;
        canLastRxCount_ = rx;
        canLastTxCount_ = tx;

        markDirty();
    }

    //////////////////////////////////////////////////////////////////////
    //  Input handling

    void SerialCli::handleInputByte(char c)
    {
        switch (inputState_)
        {
        case InputState::kIdle:
            if (c == 27)
            {
                inputState_ = InputState::kEsc;
            }
            else if (c == '\r' || c == '\n' || c == ' ')
            {
                activateSelection();
            }
            else if (c == '\t')
            {
                moveTab(1);
            }
            else if (c == 'r' || c == 'R' || c == 12) // 12 = Ctrl+L
            {
                requestFullRedraw();
            }
            break;

        case InputState::kEsc:
            inputState_ = (c == '[') ? InputState::kCsi : InputState::kIdle;
            break;

        case InputState::kCsi:
            inputState_ = InputState::kIdle;
            switch (c)
            {
            case 'A':
                moveSelection(-1);
                break;
            case 'B':
                moveSelection(1);
                break;
            case 'C':
                moveTab(1);
                break;
            case 'D':
                moveTab(-1);
                break;
            default:
                break;
            }
            break;
        }
    }

    void SerialCli::pollInput()
    {
        if (!updateConnectionState())
        {
            return;
        }

        while (Serial.available() > 0)
        {
            int value = Serial.read();
            if (value < 0)
            {
                break;
            }

            handleInputByte(static_cast<char>(value));
        }

        if (model_.activeTab == cli::kTabRtos)
        {
            uint32_t now = millis();
            if (now - rtosStatsLastMs_ >= kRtosStatsPeriodMs)
            {
                rtosStatsLastMs_ = now;
                refreshRtosStats();
            }
        }
        else if (model_.activeTab == cli::kTabCan)
        {
            uint32_t now = millis();
            if (now - canStatsLastMs_ >= kCanStatsPeriodMs)
            {
                refreshCanStats();
            }
        }

        render();
    }

    //////////////////////////////////////////////////////////////////////
    //  Events from the sensor tasks

    void SerialCli::handleEvent(const Event &ev)
    {
        switch (ev.type)
        {
        case EventType::kPirMotionStarted:
            model_.pir = true;
            board::relay1.on();
            board::onboardLed.setColor(0, 32, 0);
            model_.relay1 = board::relay1.isOn();
            markDirty();
            break;

        case EventType::kInputChanged:
            if (ev.index == 0)
            {
                model_.input1 = ev.state;
            }
            else
            {
                model_.input2 = ev.state;
            }
            markDirty();
            break;

        case EventType::kPirChanged:
            model_.pir = ev.state;
            markDirty();
            break;

        case EventType::kDipChanged:
            model_.dipValue = ev.index;
            markDirty();
            break;

        case EventType::kUltrasonicReading:
            model_.distanceCm = ev.distanceCm;
            model_.ultrasonicTimeout = false;
            markDirty();
            break;

        case EventType::kUltrasonicTimeout:
            model_.ultrasonicTimeout = true;
            markDirty();
            break;

        case EventType::kCanFrameReceived:
            // Counts and rates come from board::can.rxFrameCount() via the
            // periodic refreshCanStats() (see pollInput()/moveTab()), not
            // from this event -- it only needs to nudge a redraw.
            markDirty();
            break;

        case EventType::kCanError:
            model_.canErrorFlags = ev.index;
            markDirty();
            break;

        case EventType::kCanopenNodeId:
            model_.canopenNodeId = ev.index;
            markDirty();
            break;

        case EventType::kNmtStateChanged:
            model_.nmtState = ev.index;
            markDirty();
            break;

        case EventType::kHeartbeatSent:
            ++model_.heartbeatCount;
            model_.lastHeartbeatMs = millis();
            markDirty();
            break;

        case EventType::kLedStateChanged:
            if (ev.index == 0)
            {
                model_.led1Pattern = ev.value;
            }
            else
            {
                model_.led2Pattern = ev.value;
            }
            markDirty();
            break;
        }
    }

} // namespace app
