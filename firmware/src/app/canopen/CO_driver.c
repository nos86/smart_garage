/*
 * CAN module glue between CANopenNode and hal::CanTransceiver, adapted from
 * CANopenNode/example/CO_driver_blank.c for the smart_garage RP2040 node.
 *
 * Actual SPI access to the MCP25625 happens in co_can_bridge.cpp (C++, so it
 * can reach app::board::can and app::canBusMutex); this file only knows the
 * small extern "C" bridge surface declared below.
 *
 * Scope note: this bring-up does not implement a runtime NMT
 * reset-communication cycle (CO_CANsetConfigurationMode() /
 * CO_CANmodule_disable() are no-ops) -- CANopenNode is initialized once at
 * boot by task_canopen.cpp and stays up. A future OTA-driven reset flow
 * would need to give these real behavior.
 */

#include <string.h>

#include "301/CO_driver.h"

/* Implemented in co_can_bridge.cpp. */
extern bool coCanBridgeSend(uint32_t ident, uint8_t dlc, const uint8_t data[8]);

/* Single CAN module instance for this node -- CANopenNode only ever creates
 * one, so a file-local pointer (set by CO_CANmodule_init(), read by
 * canopenDispatchRxFrame()) is simpler than threading it through CANptr. */
static CO_CANmodule_t *activeCANmodule = NULL;

void CO_CANsetConfigurationMode(void *CANptr) {
    (void)CANptr;
    /* No-op: board::can is already programmed (bitrate, Normal mode) by
     * app::board::init()'s loopback self-test before CANopenNode ever
     * starts. See scope note above. */
}

void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule) { CANmodule->CANnormal = true; }

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, void *CANptr, CO_CANrx_t rxArray[], uint16_t rxSize,
                                    CO_CANtx_t txArray[], uint16_t txSize, uint16_t CANbitRate) {
    (void)CANbitRate; /* bitrate is fixed at board::init() time, see above */

    if (CANmodule == NULL || rxArray == NULL || txArray == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    /* Software ident/mask matching only (see canopenDispatchRxFrame below):
     * the MCP25625's hardware filter banks are left at their accept-all
     * reset state rather than programmed per rxArray entry. Programming
     * them is a bus-load optimization, not needed for this bring-up (see
     * docs/architecture-plan.md's deferred COB-ID/priority backlog). */
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for (uint16_t i = 0U; i < rxSize; i++) {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (uint16_t i = 0U; i < txSize; i++) {
        txArray[i].bufferFull = false;
    }

    activeCANmodule = CANmodule;
    return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t *CANmodule) {
    (void)CANmodule;
    /* No-op, see scope note above. */
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask,
                                     bool_t rtr, void *object, void (*CANrx_callback)(void *object, void *message)) {
    if (CANmodule == NULL || object == NULL || CANrx_callback == NULL || index >= CANmodule->rxSize) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_CANrx_t *buffer = &CANmodule->rxArray[index];
    buffer->object = object;
    buffer->CANrx_callback = CANrx_callback;
    buffer->ident = ident & 0x07FFU;
    if (rtr) {
        buffer->ident |= 0x0800U;
    }
    buffer->mask = (mask & 0x07FFU) | 0x0800U;

    return CO_ERROR_NO;
}

CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr,
                                uint8_t noOfBytes, bool_t syncFlag) {
    (void)rtr; /* none of NMT/HB/EM/SDO/LSS-slave/storage send RTR frames */

    if (CANmodule == NULL || index >= CANmodule->txSize) {
        return NULL;
    }

    CO_CANtx_t *buffer = &CANmodule->txArray[index];
    buffer->ident = ident & 0x07FFU;
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer) {
    CO_ReturnError_t err = CO_ERROR_NO;

    /* Sent synchronously and immediately -- no software TX queue. The
     * MCP25625 has 3 hardware TX buffers; at this bring-up's traffic level
     * (heartbeat + occasional NMT/SDO/emergency) that headroom is enough.
     * Queuing/priority under sustained bulk traffic is deferred (P0 backlog:
     * COB-ID priority scheme). */
    CO_LOCK_CAN_SEND(CANmodule);
    bool_t sent = coCanBridgeSend(buffer->ident, buffer->DLC, buffer->data);
    CO_UNLOCK_CAN_SEND(CANmodule);

    if (!sent) {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        err = CO_ERROR_TX_OVERFLOW;
    } else {
        CANmodule->firstCANtxMessage = false;
    }

    return err;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule) {
    (void)CANmodule;
    /* No software TX queue (see CO_CANsend) -- nothing pending to clear. */
}

void CO_CANmodule_process(CO_CANmodule_t *CANmodule) {
    (void)CANmodule;
    /* Bus-off/warning/passive error-counter mapping into CANerrorStatus is
     * deferred -- task_can.cpp already surfaces raw MCP25625 error flags on
     * the CLI's CAN tab (see docs/architecture-plan.md's deferred CAN
     * diagnostic format backlog) independently of this stack-level status
     * word, which nothing in this bring-up's NMT/HB/SDO path reads yet. */
}

/* Called from task_can.cpp's RX drain loop (task context, never from the
 * MCP_INT ISR itself -- SPI reads can't happen in interrupt context, which
 * is exactly why that ISR only gives a task notification). Mirrors the
 * software ident/mask matching loop from CANopenNode's own blank driver
 * template. */
void canopenDispatchRxFrame(uint32_t ident, uint8_t dlc, const uint8_t data[8]) {
    if (activeCANmodule == NULL) {
        return;
    }

    CO_CANrxMsg_t rcvMsg;
    rcvMsg.ident = ident & 0x07FFU;
    rcvMsg.DLC = dlc > 8U ? 8U : dlc;
    memcpy(rcvMsg.data, data, rcvMsg.DLC);

    CO_CANrx_t *buffer = activeCANmodule->rxArray;
    for (uint16_t index = activeCANmodule->rxSize; index > 0U; index--) {
        if (((rcvMsg.ident ^ buffer->ident) & buffer->mask) == 0U) {
            if (buffer->CANrx_callback != NULL) {
                buffer->CANrx_callback(buffer->object, &rcvMsg);
            }
            break;
        }
        buffer++;
    }
}
