#include "CO_storageFlash.h"

#include <cstring>

#include "OD.h"
#include "../board.h"
#include "hal/flash_layout.h"
#include "hal/logic/persist_image.h"

namespace app::canopen::persist {

namespace {

constexpr size_t kLssNodeIdOffset = 0;
constexpr size_t kLssBitRateOffset = kLssNodeIdOffset + sizeof(uint8_t);
constexpr size_t kOdCommOffset = kLssBitRateOffset + sizeof(uint16_t);
constexpr size_t kPayloadLen = kOdCommOffset + sizeof(OD_PERSIST_COMM);

static_assert(sizeof(hal::logic::PersistHeader) + kPayloadLen <= hal::flash_layout::kSectorSize,
              "persisted snapshot (LSS state + OD_PERSIST_COMM) does not fit in one flash sector");

uint8_t g_writeSlot = 0;
uint32_t g_nextSequence = 1;
CO_EM_t* g_em = nullptr;

// EMCY manufacturer code 0xFF02 (docs/canopen-network-spec.md §7): "scrittura
// flash fallita durante store parametri". Only transmittable while the node
// has a configured node-ID (an LSS-unconfigured node has no EMCY COB-ID and
// CO_process() skips CO_EM_process() entirely); during first commissioning
// the failure is still visible as the LSS "store configuration" error reply.
void reportFlashWriteFailure() {
  if (g_em == nullptr) {
    return;
  }
  CO_errorReport(g_em, CO_EM_NON_VOLATILE_MEMORY, 0xFF02, 0);
}

void serializePayload(uint8_t* out) {
  out[kLssNodeIdOffset] = lssPendingNodeId;
  memcpy(out + kLssBitRateOffset, &lssPendingBitRate, sizeof(lssPendingBitRate));
  memcpy(out + kOdCommOffset, &OD_PERSIST_COMM, sizeof(OD_PERSIST_COMM));
}

void applyPayload(const uint8_t* in) {
  lssPendingNodeId = in[kLssNodeIdOffset];
  memcpy(&lssPendingBitRate, in + kLssBitRateOffset, sizeof(lssPendingBitRate));
  memcpy(&OD_PERSIST_COMM, in + kOdCommOffset, sizeof(OD_PERSIST_COMM));
}

ODR_t storeCallback(CO_storage_entry_t* entry, CO_CANmodule_t* CANmodule) {
  (void)entry;
  (void)CANmodule;
  return saveAll() ? ODR_OK : ODR_HW;
}

ODR_t restoreCallback(CO_storage_entry_t* entry, CO_CANmodule_t* CANmodule) {
  (void)entry;
  (void)CANmodule;
  return invalidateAll() ? ODR_OK : ODR_HW;
}

}  // namespace

uint8_t lssPendingNodeId = CO_LSS_NODE_ID_ASSIGNMENT;
uint16_t lssPendingBitRate = 250;

void bindEmergency(CO_EM_t* em) { g_em = em; }

void loadAtBoot() {
  hal::logic::PersistHeader header0{};
  hal::logic::PersistHeader header1{};
  static uint8_t payload0[kPayloadLen];
  static uint8_t payload1[kPayloadLen];

  board::persistFlash.read(0, 0, reinterpret_cast<uint8_t*>(&header0), sizeof(header0));
  board::persistFlash.read(0, sizeof(header0), payload0, sizeof(payload0));
  board::persistFlash.read(1, 0, reinterpret_cast<uint8_t*>(&header1), sizeof(header1));
  board::persistFlash.read(1, sizeof(header1), payload1, sizeof(payload1));

  bool valid0 = hal::logic::persist_header_valid(header0, payload0, sizeof(payload0));
  bool valid1 = hal::logic::persist_header_valid(header1, payload1, sizeof(payload1));

  hal::logic::PersistSlotDecision decision =
      hal::logic::persist_select_slots(valid0, header0.sequence, valid1, header1.sequence);
  g_writeSlot = decision.writeSlot;
  g_nextSequence = decision.nextSequence;

  if (decision.loadSlot < 0) {
    // First boot / both copies corrupt: OD_PERSIST_COMM keeps the compiled
    // defaults from OD.c, LSS starts unconfigured (spec §3: LSS waiting).
    lssPendingNodeId = CO_LSS_NODE_ID_ASSIGNMENT;
    lssPendingBitRate = 250;
    return;
  }

  applyPayload(decision.loadSlot == 0 ? payload0 : payload1);
}

bool saveAll() {
  // Clear any EMCY 0xFF02 latched by a previous failed attempt first: each
  // saveAll() call is a distinct, user-triggered event (SDO/LSS "store"), so
  // it should get its own fresh report if it fails again -- otherwise
  // CO_errorReport()'s change-only dedup (CO_Emergency.c) would silently
  // swallow every retry after the first, since the error bit stays latched
  // until something explicitly clears it. No-op if nothing was latched.
  if (g_em != nullptr) {
    CO_errorReset(g_em, CO_EM_NON_VOLATILE_MEMORY, 0);
  }

  uint8_t payload[kPayloadLen];
  serializePayload(payload);

  hal::logic::PersistHeader header = hal::logic::persist_build_header(kPayloadLen, g_nextSequence, payload);

  uint8_t frame[sizeof(header) + kPayloadLen];
  memcpy(frame, &header, sizeof(header));
  memcpy(frame + sizeof(header), payload, kPayloadLen);

  bool ok = board::persistFlash.writeSector(g_writeSlot, frame, sizeof(frame));
  if (ok) {
    // Next save toggles to the other slot -- spec §8: "si scrive sempre
    // sulla copia più vecchia".
    g_writeSlot = (g_writeSlot == 0) ? 1 : 0;
    g_nextSequence++;
  } else {
    reportFlashWriteFailure();
  }
  return ok;
}

bool invalidateAll() {
  uint8_t blankHeader[sizeof(hal::logic::PersistHeader)] = {0};  // magic=0 -> persist_header_valid() rejects it
  bool okA = board::persistFlash.writeSector(0, blankHeader, sizeof(blankHeader));
  bool okB = board::persistFlash.writeSector(1, blankHeader, sizeof(blankHeader));
  g_writeSlot = 0;
  g_nextSequence = 1;
  return okA && okB;
}

CO_ReturnError_t init(CO_storage_t* storage, CO_CANmodule_t* CANmodule, OD_entry_t* OD_1010_StoreParameters,
                      OD_entry_t* OD_1011_RestoreDefaultParameters, CO_storage_entry_t* entries, uint8_t entriesCount,
                      uint32_t* storageInitError) {
  if (storage == nullptr || entries == nullptr || entriesCount == 0 || storageInitError == nullptr) {
    return CO_ERROR_ILLEGAL_ARGUMENT;
  }

  CO_ReturnError_t ret = CO_storage_init(storage, CANmodule, OD_1010_StoreParameters, OD_1011_RestoreDefaultParameters,
                                          storeCallback, restoreCallback, entries, entriesCount);
  if (ret != CO_ERROR_NO) {
    return ret;
  }

  *storageInitError = 0;
  for (uint8_t i = 0; i < entriesCount; i++) {
    CO_storage_entry_t* entry = &entries[i];
    if (entry->addr == nullptr || entry->len == 0 || entry->subIndexOD < 2) {
      *storageInitError = i;
      return CO_ERROR_ILLEGAL_ARGUMENT;
    }
  }

  return ret;
}

bool_t lssStoreCallback(void* object, uint8_t id, uint16_t bitRate) {
  (void)object;
  lssPendingNodeId = id;
  lssPendingBitRate = bitRate;
  return saveAll() ? 1 : 0;
}

}  // namespace app::canopen::persist
