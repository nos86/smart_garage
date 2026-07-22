# Smart Garage — Spec rete CANopen e Object Dictionary

Documento vivo. Chiude le voci P0 del backlog di [architecture-plan.md](architecture-plan.md): scelta stack, OD comune, addressing, mappa COB-ID, comportamento NMT/heartbeat/emergency, persistenza. È la specifica di riferimento per l'implementazione; le decisioni qui dentro sono vincolanti finché non vengono riviste esplicitamente.

Decisioni di impianto prese in fase di brainstorming (2026-07-22):

1. **Modello ibrido**: area 6000h in stile CiA 401 per gli I/O grezzi + area manufacturer 2000h per etichette di ruolo e tabella di regole di interazione.
2. **Segnali grezzi + etichette**: l'OD parla di relè/ingressi/distanza; il significato applicativo (cancello, luce, presenza) vive in oggetti-etichetta configurabili, letti dal gateway per auto-configurarsi.
3. **Regole autodescrittive**: una regola referenzia direttamente (node-ID sorgente, segnale, trigger); il firmware deriva da solo i COB-ID da ascoltare. I PDO restano il trasporto, invisibile al configuratore.
4. **Espressività minimale**: niente condizioni composte in v1; la logica combinatoria vive in Home Assistant (fuori dal percorso critico realtime).
5. **Addressing via LSS** con node-ID persistito; il DIP a 5 bit si libera dal ruolo di indirizzo.
6. **Persistenza su flash** necessaria: le regole devono sopravvivere al reboot, altrimenti il requisito "funziona senza master" è violato al primo power-cycle.

---

## 1. Scelta dello stack

**CANopenNode v4.1** (già vendored come submodule in `lib/CANopenNode/vendor`, scopato via `library.json`).

Criteri: C puro portabile (convive con Arduino/FreeRTOS via il port in `src/app/canopen/`), licenza Apache-2.0, attivamente mantenuto, copertura completa dei servizi necessari (NMT/HB, EMCY, SDO server, PDO, LSS slave **con fastscan**, CO_storage), footprint verificato compatibile con RP2040, già validato nel bring-up attuale (heartbeat + SDO funzionanti). Nessuna alternativa con pari copertura e licenza è emersa; la scelta si considera chiusa.

Moduli compilati: NMT/Heartbeat, HBconsumer, Emergency, SDOserver, TIME, SYNC, PDO, LEDs, LSSslave, storage. SDO client e LSS master restano esclusi dai nodi (il ruolo di LSS/config master è del gateway).

## 2. Identità del nodo (1000h, 1018h)

Prerequisito strutturale: **LSS fastscan identifica i nodi non configurati tramite la 1018h** — l'identity deve quindi essere univoca e popolata, non più a zero.

| Oggetto | Valore | Note |
|---|---|---|
| 1000h Device type | `0x00070191` | Profilo CiA 401 (0x0191) + additional info: digital input (bit 16), digital output (bit 17), analog input (bit 18) |
| 1018h.1 Vendor-ID | `0x00005347` | Non registrato CiA; valore convenzionale interno ("SG" ASCII). Unico requisito reale: coerenza su tutta la rete |
| 1018h.2 Product code | `0x00000001` | 1 = scheda "garage node" (questa). Valori futuri per hardware diversi |
| 1018h.3 Revision | `(major<<16)\|minor` | Revisione hardware+firmware compatibile; parte da `0x00010000` (1.0) |
| 1018h.4 Serial number | fold a 32 bit dello unique ID | RP2040: unique ID a 64 bit della flash QSPI, ridotto con XOR `(hi32 ^ lo32)`. Probabilità di collisione trascurabile su ~10 nodi |

Il serial è l'unico campo che distingue due schede identiche: senza di esso il fastscan non può separare due nodi vergini accesi insieme.

## 3. Addressing e LSS

### Decisione

Node-ID assegnato dal master via **LSS (CiA 305)** al commissioning e **persistito in flash** sul nodo. Il DIP switch non è più l'indirizzo.

### Comportamento

- **Nodo non configurato** (nessun node-ID in flash): parte con node-ID `0xFF` → CANopenNode entra in stato *LSS waiting*: solo il server LSS è attivo, nessun HB/PDO/SDO. Il nodo è invisibile ma raggiungibile via fastscan.
- **Commissioning**: il gateway (unico LSS master della rete) esegue fastscan → trova il nodo per identity → `configure node-ID` → `store configuration`. Lo slave persiste l'ID tramite il callback `CO_LSSslave_initCfgStoreCall` (gancio implementativo previsto dallo stack) e riparte con reset communication → Operational.
- **A regime**: l'ID è in flash, la rete funziona senza master (requisito 1). Il master serve solo per commissioning e riassegnazioni.
- **Allocazione ID**: `1` riservato al gateway/master; i nodi ricevono ID da `2` in su. Il registro "ID ↔ posizione fisica ↔ serial" è tenuto lato master (fuori scope di questa spec).
- **Bitrate**: fisso a 250 kbps su tutta la rete; LSS *configure bit timing* non viene usato (supportato dallo stack, ma cambiarlo su una rete già installata è più rischio che beneficio).

### Nuovo ruolo del DIP a 5 bit

| Valore DIP | Comportamento |
|---|---|
| `0` (default di produzione) | Modalità normale: node-ID da flash (o LSS waiting se assente) |
| `1–31` | **Override manuale**: node-ID forzato = valore DIP, ignora la flash. Per banco/debug: accendere un nodo da solo sul tavolo senza bisogno di un LSS master |

L'override non persiste nulla e può collidere con ID assegnati via LSS: è dichiaratamente uno strumento da banco, non da impianto. In campo tutti i DIP restano a 0.

## 4. Area 6000h — I/O grezzi (stile CiA 401)

Firmware uniforme su tutte le varianti: ogni oggetto esiste sempre; un componente non montato produce valori inerti (ingressi fermi, uscite senza effetto, distanza invalida). Nessuna OD per variante.

### Canali

La numerazione dei canali è fissa ed è la stessa usata dalle etichette (§5.1) e dalle regole (§5.2):

| Canale | Segnale | Direzione | HAL |
|---|---|---|---|
| DI bit 0 | INPUT1 (optoisolato) | in | `board::input1` |
| DI bit 1 | INPUT2 (optoisolato) | in | `board::input2` |
| DI bit 2 | PIR | in | `board::pir` |
| DI bit 3–7 | riservati (leggono 0) | — | — |
| DO bit 0 | RELAY1 | out | `board::relay1` |
| DO bit 1 | RELAY2 | out | `board::relay2` |
| DO bit 2 | LED1 | out | `board::led1` |
| DO bit 3 | LED2 | out | `board::led2` |
| DO bit 4–7 | riservati (scritture ignorate) | — | — |
| AI 1 | Distanza ultrasuoni | in | `board::ultrasonic` |

### Oggetti

| Indice | Nome | Tipo | Accesso | Note |
|---|---|---|---|---|
| 6000h.1 | Read digital inputs | UNSIGNED8 | ro, TPDO | Bitfield DI. Già de-bounced e normalizzato dall'HAL (1 = attivo, indipendente dalla polarità elettrica) |
| 6200h.1 | Write digital outputs | UNSIGNED8 | rw, RPDO | Bitfield DO. Scrittura = stato desiderato; la lettura restituisce lo stato reale attuale (anche se modificato da una regola o da un impulso in corso) |
| 6401h.1 | Read analog input | INTEGER16 | ro, TPDO | Distanza in **mm**. `-1` = misura non valida / sensore assente / timeout echo |
| 6426h.1 | AI delta value | UNSIGNED16 | rw, persist | Variazione minima (mm) perché una nuova misura generi un evento TPDO2. Default 50 |

Le soglie *per gli eventi delle regole* stanno nella regola stessa (§5.2), non qui: 6426h governa solo la pubblicazione su bus della misura. Gli altri oggetti CiA 401 (polarità 6002h, filtri, limiti 6424h/6425h) non sono esposti in v1: la polarità è fissata dall'hardware e gestita dall'HAL.

## 5. Area 2000h — manufacturer

### 5.1 Identità applicativa ed etichette di ruolo

| Indice | Nome | Tipo | Accesso | Note |
|---|---|---|---|---|
| 2000h.1 | Node name | VISIBLE_STRING(16) | rw, persist | Etichetta umana ("cancello_nord"). Solo per diagnostica/gateway |
| 2010h.1–8 | I/O role | UNSIGNED16 ×8 | rw, persist | Ruolo di ciascun canale, nell'ordine: DI0, DI1, DI2 (PIR), DO0, DO1, DO2, DO3, AI1 |

Registro dei ruoli (estendibile; il gateway lo usa per creare le entità Home Assistant):

| Codice | Ruolo | Tipico su |
|---|---|---|
| 0x0000 | non usato / non montato | qualsiasi |
| 0x0001 | `gate_pulse` (impulso apri/chiudi) | DO |
| 0x0002 | `gate_open` / 0x0003 `gate_close` | DO |
| 0x0004 | `light` | DO |
| 0x0005 | `status_led` | DO |
| 0x0010 | `button` (pulsante generico) | DI |
| 0x0011 | `limit_open` / 0x0012 `limit_closed` (finecorsa) | DI |
| 0x0013 | `door_contact` | DI |
| 0x0020 | `presence` | DI (PIR) |
| 0x0030 | `distance_parking` | AI |
| 0x0031 | `distance_gate` | AI |

I codici sono dati, non logica: il firmware non li interpreta (coerente con "grezzo + etichette"); solo gateway e umani li leggono.

### 5.2 Tabella regole di interazione (2020h–202Fh)

**16 regole**, una per indice: regola *n* = `2020h + n`. Ogni regola è un RECORD:

| Sub | Campo | Tipo | Valori |
|---|---|---|---|
| 1 | Flags | UNSIGNED8 | bit 0 = enable; bit 1–7 riservati (scrivere 0) |
| 2 | Source node-ID | UNSIGNED8 | 1–127 = nodo remoto; **0 = questo nodo** (regola locale, es. pulsante → proprio relè, nessun giro sul bus) |
| 3 | Source signal | UNSIGNED8 | nibble alto = tipo (0 = DI, 1 = AI), nibble basso = indice canale. Es. `0x02` = DI bit 2 (PIR), `0x10` = AI 1 |
| 4 | Trigger | UNSIGNED8 | DI: 0 = fronte ↑, 1 = fronte ↓, 2 = entrambi. AI: 5 = supera soglia ↑, 6 = scende sotto soglia ↓ (isteresi fissa 5 % della soglia) |
| 5 | Threshold | INTEGER16 | Solo trigger AI: soglia in mm. Ignorato per DI |
| 6 | Action | UNSIGNED8 | 0 = off, 1 = on, 2 = toggle, 3 = pulse |
| 7 | Target output | UNSIGNED8 | Indice canale DO locale (0–3) |
| 8 | Param (ms) | UNSIGNED16 | Durata impulso per `pulse` (0 → default 500). Ignorato per le altre azioni |

Tutto il record è `rw, persist`.

**Semantica di esecuzione:**

- Le regole con source remoto si attivano sui dati ricevuti nei **TPDO del nodo sorgente** (COB-ID derivati automaticamente, §6): il firmware tiene l'ultimo valore noto per ogni sorgente referenziata e rileva i fronti confrontando frame consecutivi.
- Le regole locali (source = 0) si attivano direttamente sugli eventi HAL, senza passare dal bus.
- Le azioni operano sugli stessi canali DO di 6200h: una lettura di 6200h riflette sempre lo stato reale, chiunque l'abbia comandato.
- `pulse` è non-riavviabile in v1: un nuovo trigger durante l'impulso viene ignorato (comportamento sicuro per un motore di cancello: niente treni di impulsi da PIR che sfarfalla). Da rivedere se emergono casi d'uso contrari.
- **Primo frame da una sorgente** (dopo boot proprio o della sorgente): registra solo lo stato, non genera fronti. Evita azioni spurie al risveglio della rete.
- **Sorgente assente**: nessuna azione automatica sui target in v1 (un cancello non deve muoversi perché un sensore è morto). La rilevazione dell'assenza è demandata all'HB consumer (§7) che la segnala via EMCY; eventuali azioni di fail-safe configurabili sono materia di v2.

**Validazione alla scrittura** (extension callback OD): node-ID fuori range, canale inesistente, tipo/trigger incoerenti (es. threshold su DI), target fuori range → SDO abort `0x06090030` (value range exceeded). Una regola con flags.enable = 0 può contenere valori parziali; la validazione completa avviene quando si abilita.

### 5.3 Diagnostica minima (2030h)

| Sub | Campo | Tipo | Note |
|---|---|---|---|
| 1 | Rules fired count | UNSIGNED32 ro | Totale azioni eseguite dal boot |
| 2 | Last rule fired | UNSIGNED8 ro | Indice (0–15) dell'ultima regola scattata, 0xFF = nessuna |
| 3 | Source frames matched | UNSIGNED32 ro | Frame ricevuti riconosciuti come sorgente di almeno una regola |

Volatile, azzerata al boot. Il formato diagnostico CAN esteso resta voce P1.

## 6. PDO e mappa COB-ID

### TPDO (produzione — uguale su tutti i nodi)

| PDO | COB-ID | Mapping | Trasmissione |
|---|---|---|---|
| TPDO1 | `0x180 + ID` | byte 0 = 6000h.1 (DI), byte 1 = 6200h.1 (DO) | Tipo 255 (event-driven) su variazione di uno dei due byte; **inhibit 50 ms**; event timer 5 s (refresh periodico di stato per gateway/regole) |
| TPDO2 | `0x280 + ID` | byte 0–1 = 6401h.1 (distanza mm) | Tipo 255 su variazione ≥ 6426h; inhibit 500 ms; event timer 60 s |
| TPDO3–4 | disabilitati (bit 31 del COB-ID) | — | riserva |

Il DO byte nel TPDO1 serve a due cose: il gateway vede lo stato reale delle uscite senza polling SDO, e una regola può (in futuro) usare come sorgente anche un'uscita remota.

### RPDO

RPDO1–4 **disabilitati in v1**. L'interazione tra nodi non passa dagli oggetti RPDO: il dispatcher software (`canopenDispatchRxFrame`) confronta i COB-ID in ingresso con l'insieme `{0x180+S, 0x280+S | S = source node-ID referenziato da almeno una regola abilitata}`, ricalcolato a ogni modifica della tabella regole. Gli RPDO standard restano di riserva per usi CANopen ortodossi futuri (es. comando diretto di 6200h da un tool).

Chi vuole comandare un'uscita da fuori (gateway) usa **SDO su 6200h.1** in v1: per comandi umani via Home Assistant la latenza SDO è irrilevante.

### Mappa COB-ID e priorità

L'arbitraggio CAN dà priorità al COB-ID più basso; la mappa segue il pre-defined connection set, che già ordina bene il traffico:

| COB-ID | Funzione | Classe |
|---|---|---|
| `0x000` | NMT | supervisione (solo master, raro) |
| `0x080 + ID` | EMCY | realtime, massima priorità applicativa |
| `0x180 + ID` | TPDO1 (eventi digitali) | **realtime** — è il percorso del budget ≤ 100 ms |
| `0x280 + ID` | TPDO2 (distanza) | realtime a bassa frequenza |
| `0x580/0x600 + ID` | SDO | bulk (config, diagnostica, futuro OTA) |
| `0x700 + ID` | Heartbeat | supervisione |
| `0x7E5/0x7E4` | LSS | solo commissioning |

SYNC (`0x080`) e TIME (`0x100`) **non usati** in v1: tutto è event-driven; il modulo SYNC resta compilato ma inattivo.

### Budget di bus e latenza (250 kbps, ~10 nodi)

Un frame CAN standard a 8 byte ≈ 130 bit ≈ 0,52 ms a 250 kbps.

- **Fondo**: 10 HB/s + refresh TPDO1 (10 × 0,2/s) + TPDO2 lenti ≈ 15 frame/s ≈ **0,8 % di carico**.
- **Burst peggiore**: tutti e 10 i nodi emettono TPDO1+TPDO2 nello stesso istante = 20 frame ≈ 10,4 ms di occupazione bus.
- **Latenza hot-start** (evento → azione remota): acquisizione HAL (≤ 5 ms, task sensori) + inhibit TPDO1 (≤ 50 ms nel caso peggiore di evento ravvicinato al precedente) + trasmissione e arbitraggio (≤ 11 ms in burst massimo) + dispatch + azione (≤ 5 ms) ≈ **≤ 71 ms worst-case** → dentro il budget di 100 ms con margine. Il caso tipico (nessun evento nei 50 ms precedenti) è ≈ 10–15 ms.
- Il traffico bulk SDO (config/OTA) non può invertire le priorità: perde ogni arbitraggio contro 0x180/0x280 per costruzione.

## 7. NMT, heartbeat, emergency

- **Startup**: self-start a Operational (decisione già presa, oggetto 1F80h "self-starting"; nell'implementazione è il flag `CO_NMT_STARTUP_TO_OPERATIONAL` già attivo in `task_canopen.cpp`). 1F80h non è esposto nell'OD in v1: il comportamento è fisso, non configurabile. Il master conserva i comandi NMT come supervisione (es. fermare i nodi durante un OTA). *Resta aperta* la verifica del bit esatto sul testo CiA 302 se mai si esporrà l'oggetto.
- **HB producer (1017h)**: 1000 ms (valore attuale confermato). Con 10 nodi il costo è trascurabile e la diagnosi di nodo morto arriva in ~2–3 s.
- **HB consumer (1016h, 8 slot)**: configurato dal master al commissioning, coerente con le sorgenti delle regole del nodo (timeout consigliato 3000 ms = 3 heartbeat persi). In v1 la coerenza regole↔1016h è responsabilità del configuratore; l'auto-derivazione dalle regole è candidata v2.
- **Reazione agli errori — modifica rispetto al bring-up attuale**: un timeout HB di un peer o un errore di comunicazione **non deve degradare lo stato NMT del nodo** (un nodo che retrocede a Pre-Operational spegne i propri TPDO e propaga il guasto ai vicini — l'opposto del requisito 1). Dal `kNmtControl` attuale va quindi rimosso il comportamento "error register → cambio stato" per i bit di comunicazione (`CO_ERR_REG_COMMUNICATION`); il nodo resta Operational, segnala via EMCY e recupera da solo. Il bus-off resta gestito dal recovery del driver.
- **EMCY**: codici standard CiA (0x8130 heartbeat consumer, 0x8140 recovered from bus-off, ecc.) più area manufacturer `0xFF00–0xFFFF`:
  - `0xFF00` = regola invalida rilevata a runtime (es. dopo restore da flash corrotta → regola disabilitata)
  - `0xFF01` = timeout persistente sensore ultrasuoni (montato ma muto)
  - `0xFF02` = scrittura flash fallita durante store parametri

## 8. Persistenza

### Cosa si salva

| Gruppo storage | Contenuto | Trigger di salvataggio |
|---|---|---|
| `PERSIST_COMM` | Parametri di comunicazione (1016h, 1017h, COB-ID/timing PDO, 6426h) | 1010h sub 2 |
| `PERSIST_APP` (nuovo) | 2000h node name, 2010h ruoli, 2020h–202Fh regole | 1010h sub 3 |
| LSS | node-ID assegnato (+ bitrate pending) | automatico su LSS *store configuration* (callback `CO_LSSslave_initCfgStoreCall`) |

1010h sub 1 = salva tutto; 1011h speculare per il restore ai default. Firme standard CiA 301: scrivere `0x65766173` ("save") / `0x64616F6C` ("load"). **Nessun autosave**: una scrittura SDO non persistita si perde al reboot — comportamento CANopen standard, il configuratore deve chiudere con 1010h.

### Dove e come

- Flash QSPI 2 MB del modulo: riservati gli **ultimi 8 KB** (2 settori da 4 KB, `0x1FE000` e `0x1FF000`).
- Schema **A/B**: due copie complete, ognuna con header (magic, versione layout, contatore di sequenza, CRC32). Si scrive sempre sulla copia più vecchia; al boot si carica la più recente con CRC valido. Un power-fail durante la scrittura lascia intatta l'altra copia (nessuno stato "brick").
- CRC invalido su entrambe = primo avvio: default di fabbrica (nessun node-ID → LSS waiting, regole vuote).
- La versione di layout nell'header gestisce le migrazioni future dell'OD; versione sconosciuta = trattata come vuota.
- Il blocco è **fuori dalle aree immagine** del futuro OTA dual-bank (bootloader + 2 slot): l'aggiornamento firmware non tocca mai la configurazione. Vincolo da rispettare quando si dimensioneranno gli slot (P1).
- Implementazione: backend flash per `CO_storage` (sostituisce `CO_storageBlank`), scritture in sezione critica compatibile con XIP (`flash_safe_execute` dell'SDK, con l'altro core in pausa).

## 9. Workflow di configurazione end-to-end

Scenario: *"il PIR del nodo 2 accende per impulso di 500 ms il relè 1 del nodo 5"*.

**Commissioning (una tantum, master presente):**

1. Nodo nuovo, DIP = 0, flash vergine → LSS waiting.
2. Gateway: fastscan → trova identity `5347:0001:xxxx:serial` → configure node-ID = 5 → store configuration → il nodo persiste e riparte Operational, heartbeat visibile.

**Configurazione della regola (SDO verso il nodo 5):**

| Passo | Scrittura | Valore |
|---|---|---|
| 1 | `2020h.2` (rule 0, source node) | 2 |
| 2 | `2020h.3` (source signal) | `0x02` (DI bit 2 = PIR) |
| 3 | `2020h.4` (trigger) | 0 (fronte di salita) |
| 4 | `2020h.6` (action) | 3 (pulse) |
| 5 | `2020h.7` (target) | 0 (RELAY1) |
| 6 | `2020h.8` (param) | 500 |
| 7 | `2020h.1` (flags) | 1 (enable → qui scatta la validazione completa) |
| 8 | `2010h.4` (ruolo DO0) | `0x0001` (gate_pulse) — per il gateway |
| 9 | `1010h.3` | `0x65766173` ("save") |

Il nodo 5 ricalcola l'insieme dei COB-ID sorvegliati (ora include `0x180+2`) e da quel momento reagisce al PIR del nodo 2 anche a master spento. Facoltativo: il master allinea `1016h` del nodo 5 per monitorare l'heartbeat del nodo 2.

**Verifica:** muovere la mano davanti al PIR del nodo 2 → osservare TPDO1 di 2 sul bus, click del relè su 5, `2030h.1` incrementato, tab CANOPEN della CLI.

## 10. Fuori scope (v2 / backlog)

- Condizioni composte nelle regole (AND di un secondo segnale, fasce orarie) — delegate a Home Assistant finché non emerge un caso d'uso che deve funzionare senza master.
- Azioni di fail-safe su perdita della sorgente (oggi: solo segnalazione EMCY).
- Auto-derivazione di 1016h dalla tabella regole.
- Impulso riavviabile / retriggerable.
- SYNC/TIME, uso degli RPDO standard, comando remoto via PDO (oggi via SDO).
- OTA via CAN (P1) e formato diagnostica CAN estesa (P1).
- Verifica sul testo ufficiale CiA 302 del bit self-starting di 1F80h (rilevante solo se l'oggetto verrà esposto).

## 11. Impatti sull'implementazione attuale (riepilogo per il futuro lavoro)

- Rigenerare `OD.c/OD.h` (CANopenEditor) con: identity §2, area 6000h §4, area 2000h §5, mapping/timing TPDO §6, gruppo storage `PERSIST_APP` — e stavolta **versionare il file di progetto `.xpd`** accanto ai generati.
- `task_canopen.cpp`: rimuovere `nodeIdFromDip()` a favore di flash + override DIP (§3); correggere `kNmtControl` (§7); registrare il callback LSS store.
- Nuovo modulo storage flash A/B (§8) al posto di `CO_storageBlank`.
- Nuovo modulo "rule engine" collegato al dispatcher RX e all'event queue esistente.
- Collegare gli oggetti OD all'HAL via extension callback (6000h/6200h/6401h ↔ `board::*`).
