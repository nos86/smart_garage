/*
 * Device and application specific definitions for CANopenNode, adapted from
 * CANopenNode/example/CO_driver_target.h for the smart_garage RP2040 node.
 *
 * This file is included from 301/CO_driver.h (in the vendored CANopenNode
 * submodule), which documents every macro/type below. It is our own,
 * project-owned file -- not part of the submodule -- so it is safe to edit.
 */

#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 (Cortex-M0+) is little endian; CANopen itself is little endian, so
 * no byte swapping is ever needed here. */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x

typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

/* Received CAN message object. Populated by canopenDispatchRxFrame() (see
 * CO_driver.c) from a hal::CanFrame drained off the MCP25625 by task_can.cpp
 * -- SPI cannot be touched from interrupt context, so dispatch always runs
 * at task priority, never from the GPIO ISR itself. */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
} CO_CANrxMsg_t;

#define CO_CANrxMsg_readIdent(msg) (((const CO_CANrxMsg_t *)(msg))->ident)
#define CO_CANrxMsg_readDLC(msg)   (((const CO_CANrxMsg_t *)(msg))->DLC)
#define CO_CANrxMsg_readData(msg)  (((const CO_CANrxMsg_t *)(msg))->data)

/* Received message object -- one per rxArray slot, matched by (ident, mask)
 * against every incoming frame in canopenDispatchRxFrame() (software
 * matching; the MCP25625's own hardware filters are left at their
 * accept-all reset state, see CO_driver.c). */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* Transmit message object. Unlike the upstream blank template, DLC is
 * stored directly (not bit-packed into ident) since this file is ours to
 * shape as needed -- CO_CANsend() in CO_driver.c reads it straight. */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object. CANptr is unused here -- the single hal::CanTransceiver
 * instance (app::board::can) is reached through the co_can_bridge.cpp
 * functions instead of being threaded through CANptr. */
typedef struct {
    void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
} CO_CANmodule_t;

/* Data storage object for one entry -- CO_storageBlank.c/.h (adapted from
 * the example, no-op) never actually persists anything, so addrNV is unused
 * for this bring-up. */
typedef struct {
    void *addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    void *addrNV;
} CO_storage_entry_t;

/* CO_CANsend() is called from both task_canopen.cpp's CO_process() loop and
 * (indirectly) task_can.cpp's RX dispatch (heartbeat/SDO/emergency replies
 * triggered by an incoming request) -- both must serialize against
 * app::canBusMutex, the same lock task_can.cpp uses to drain board::can, so
 * TX and RX never interleave mid-SPI-transaction. See co_can_bridge.cpp. */
void coCanLockSend(void);
void coCanUnlockSend(void);
#define CO_LOCK_CAN_SEND(CAN_MODULE)   coCanLockSend()
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) coCanUnlockSend()

/* CO_LOCK_EMCY/CO_LOCK_OD guard CANopenNode's shared Emergency/Object
 * Dictionary state, which is touched from two different FreeRTOS tasks in
 * this design: task_can.cpp's RX dispatch (canopenDispatchRxFrame, e.g. an
 * incoming SDO request) and task_canopen.cpp's CO_process() mainline. A
 * short FreeRTOS critical section is enough since both sides only hold it
 * for a few field reads/writes, never across a blocking call. */
#define CO_LOCK_EMCY(CAN_MODULE)   taskENTER_CRITICAL()
#define CO_UNLOCK_EMCY(CAN_MODULE) taskEXIT_CRITICAL()
#define CO_LOCK_OD(CAN_MODULE)     taskENTER_CRITICAL()
#define CO_UNLOCK_OD(CAN_MODULE)   taskEXIT_CRITICAL()

/* Single-threaded-enough for this bring-up: CO_CONFIG_FLAG_CALLBACK_PRE is
 * not enabled anywhere, so nothing relies on the CO_FLAG_* memory-barrier
 * dance across a real interrupt boundary. Kept as plain (non-atomic)
 * operations, matching the upstream blank template. */
#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                                                                                           \
    {                                                                                                                \
        CO_MemoryBarrier();                                                                                          \
        rxNew = (void *)1L;                                                                                          \
    }
#define CO_FLAG_CLEAR(rxNew)                                                                                         \
    {                                                                                                                \
        CO_MemoryBarrier();                                                                                          \
        rxNew = NULL;                                                                                                \
    }

#ifdef __cplusplus
}
#endif

#endif /* CO_DRIVER_TARGET_H */
