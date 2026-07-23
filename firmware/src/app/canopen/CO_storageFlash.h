#pragma once

// CANopen data storage backend on top of hal::FlashStorage (board::persistFlash)
// -- replaces CO_storageBlank.c/.h. See docs/canopen-network-spec.md §8.
//
// Design: a single combined snapshot -- LSS node-ID/bit-rate plus every
// registered CO_storage entry (currently just OD_PERSIST_COMM; OD_PERSIST_APP
// joins once the custom OD is regenerated, spec §11 point 1) -- written as
// one atomic A/B flash image on every save, regardless of which OD/LSS path
// triggered it (OD 1010h write, or LSS "store configuration"). This matches
// spec §8's "due copie complete" model more directly than per-entry storage
// (CO_storageEeprom's model), and keeps a power-fail from ever leaving a
// half-updated configuration.

#include "CANopen.h"
#include "storage/CO_storage.h"

namespace app::canopen::persist {

// Static-duration storage for the LSS "pending" node-ID/bit-rate pair.
// CO_LSSinit() keeps a raw pointer to whatever is passed to it for the
// entire CO_t lifetime (see CO_LSSslave_t::pendingNodeID/pendingBitRate in
// the vendored 305/CO_LSSslave.c) -- these must not be stack locals of
// initCanopen(), so this module owns them.
extern uint8_t lssPendingNodeId;
extern uint16_t lssPendingBitRate;

// Lets saveAll() report a failed flash write via EMCY manufacturer code
// 0xFF02 (docs/canopen-network-spec.md §7: "scrittura flash fallita durante
// store parametri"). Safe to call with a null `em` (skips reporting). Note:
// only effective once the node has a configured node-ID -- an
// LSS-unconfigured node has no EMCY COB-ID and CO_process() skips
// CO_EM_process(); first-commissioning failures surface via the LSS "store
// configuration" error reply instead.
void bindEmergency(CO_EM_t* em);

// Must run once at boot, before board::dipSwitch is consulted for the
// manual override and before CO_LSSinit()/CO_CANopenInit(): loads the
// freshest valid A/B image, if any, into lssPendingNodeId/lssPendingBitRate
// and over OD_PERSIST_COMM's compiled-in defaults. Leaves everything at its
// compiled/default state if no valid image is found (first boot, or both
// copies corrupt -- spec §8: "CRC invalido su entrambe = primo avvio").
void loadAtBoot();

// Writes a full snapshot (lssPendingNodeId/lssPendingBitRate + every
// registered CO_storage entry) to the older of the two A/B slots. Called
// both from the CO_storage store() callback (OD 1010h write) and from the
// LSS "store configuration" callback -- both converge on the same
// complete-copy write.
bool saveAll();

// Invalidates both A/B slots so the next boot behaves like a blank device
// (spec §8's "restore defaults" path -- OD 1011h write).
bool invalidateAll();

// Registers `entries` with CANopenNode's generic CO_storage module (see
// storage/CO_storage.h), wiring OD 1010h/1011h to saveAll()/invalidateAll().
// Mirrors CO_storageBlank_init()'s signature so it's a drop-in replacement
// at the task_canopen.cpp call site. `storage` and `entries` must be
// static/global (CO_storage_init() stores pointers into them for the OD
// extension's lifetime).
CO_ReturnError_t init(CO_storage_t* storage, CO_CANmodule_t* CANmodule, OD_entry_t* OD_1010_StoreParameters,
                      OD_entry_t* OD_1011_RestoreDefaultParameters, CO_storage_entry_t* entries, uint8_t entriesCount,
                      uint32_t* storageInitError);

// Matches CO_LSSslave_initCfgStoreCall()'s callback signature
// (bool_t (*)(void* object, uint8_t id, uint16_t bitRate)): invoked when the
// LSS master sends "store configuration". Updates
// lssPendingNodeId/lssPendingBitRate (redundant with the write LSS already
// did through the pointers passed to CO_LSSinit, kept for clarity) and calls
// saveAll(). Returns true (ack to master) on success.
bool_t lssStoreCallback(void* object, uint8_t id, uint16_t bitRate);

}  // namespace app::canopen::persist
