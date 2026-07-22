# Smart Garage — Piano architetturale firmware

Documento vivo. Raccoglie le decisioni prese durante il brainstorming architetturale e la lista degli approfondimenti tecnici ancora da fare. Va aggiornato via via che si affrontano nuovi temi.

## Stato dei requisiti originali

| # | Requisito | Stato |
|---|-----------|-------|
| 1 | Comunicazione decentralizzata, tollerante a nodi/master down | Deciso a livello architetturale |
| 2 | Protocollo CANopen | **Deciso** — stack CANopenNode v4.1 e design OD/rete in [canopen-network-spec.md](canopen-network-spec.md) |
| 3 | Riprogrammabile via CAN | Parzialmente aperto (chi innesca l'OTA senza master) |
| 4 | Configurabile via CAN | **Deciso** — tabella regole + etichette in area 2000h, persistenza flash A/B; vedi [canopen-network-spec.md](canopen-network-spec.md) §5, §8 |
| 5 | Diagnostica via CAN + UART (testi/colori) | UART deciso; lato CAN da approfondire |
| 6 | Messaggistica realtime tra nodi | Deciso a livello architetturale |
| 7 | Consumi minimi in standby | Deciso a livello architetturale, da validare con numeri reali |
| ~~8~~ | ~~Supporto hardware depopolato, autorilevato al boot~~ | **Eliminato** — vedi nota sotto |

*Requisito 3 aggiornato*: OTA solo con master online (nessun percorso decentralizzato) — fattibilità flash/dual-bank verificata a livello preliminare, vedi sezione dedicata.

## Decisioni architetturali confermate

### Ruolo del master e decentralizzazione (Req. 1)

Il master non è un nodo NMT-master "classico" nel percorso critico. Il suo ruolo è: interfaccia verso Home Assistant, orchestrazione diagnostica e OTA. La logica di business (messaggistica realtime peer-to-peer, requisito 6) deve restare strutturalmente indipendente dalla sua presenza.

**Rischio critico identificato**: con il modello NMT standard, un nodo resta bloccato in Pre-Operational finché non riceve un comando NMT Start — se il master è down dopo un riavvio, i nodi restano congelati.

**Soluzione**: uso dell'oggetto CANopen 1F80h (NMT Startup), bit di "self-starting" — ogni nodo si autopromuove a NMT Operational a fine inizializzazione locale, senza attendere il master. Il master mantiene i comandi NMT come controllo supervisorio aggiuntivo quando è presente e raggiungibile (es. per fermare/riavviare nodi in modo coordinato durante un OTA).

*Da verificare*: bit esatto e comportamento sulla specifica ufficiale CiA 301/302 prima dell'implementazione — citato da conoscenza generale, non controllato sul testo ufficiale in questa sessione.

### Diagnostica UART (Req. 5, parte UART)

Uso esclusivamente come debug tecnico via terminale seriale (tipo PuTTY/minicom), con controllo pieno di testo e colore (ANSI escape codes). Nessun pannello o display esterno da pilotare, nessun protocollo a menu da progettare.

### Messaggistica realtime e budget di latenza (Req. 6)

Budget di latenza end-to-end concordato:

- **Cold-start** (nodo in sleep al momento dello stimolo): ≤ 500 ms
- **Hot-start** (nodo già sveglio): ≤ 100 ms

Scartato l'approccio "ritrasmissione/ack applicativo" per compensare il rischio di perdita del frame di risveglio su nodi dormienti — introduceva traffico ridondante e customizzazione del protocollo CANopen standard, non desiderata.

**Soluzione strutturale**: dato che tutti i nodi sono "prosumer" (sia sorgenti che target di eventi realtime — vedi sotto), il problema è risolto a monte tenendo il transceiver CAN sempre in ascolto, eliminando strutturalmente il rischio di perdere un frame in arrivo.

Per garantire il rispetto dei budget servirà comunque uno schema di priorità sui COB-ID, per evitare che traffico bulk (OTA, configurazione SDO) introduca jitter sugli eventi realtime — *vedi backlog*.

### Gestione energetica (Req. 7)

Tutti i nodi sono prosumer: nessuna distinzione tra nodi "emettitori" e "target". Di conseguenza:

- **MCP25625**: sempre attivo, mai in modalità standby, su tutti i nodi. Elimina il rischio di perdita del frame di wake senza bisogno di ritrasmissioni. Il MCU può comunque dormire (dormant) e risvegliarsi tramite l'interrupt dell'MCP (MCP_INT, GP6).
- **PIR**: sempre alimentato elettricamente (consumo fisso, non comprimibile). Il MCU si risveglia solo su fronte di salita del segnale (interrupt-driven, zero polling, zero costo tra un evento e l'altro).
- **Ultrasuoni** (solo su varianti hardware che lo montano): due velocità di scansione.
  - Baseline "a riposo": 1 misura/minuto in assenza di stimoli.
  - Fase "attiva": 1 misura/secondo, per una finestra scorrevole di 60 secondi dall'ultimo stimolo (pulsante, PIR, o evento dal master). Ogni nuovo stimolo rinnova la finestra.
- Nota terminologica corretta durante la discussione: il calcolo di distanza per tempo di volo appartiene esclusivamente al sensore a ultrasuoni; il PIR rileva solo presenza/movimento, nessuna misura di tempo/distanza.

**Target di consumo**: rete intera (~10 nodi previsti) sotto i 5 W misurati lato 220 V, con alimentazione condivisa (un unico alimentatore 220V→24V, distribuito ai nodi via bus a 4 pin CANH/CANL/24V/GND).

*Da validare*: questo target retroagisce sulla scelta "MCP sempre attivo" — con 10 nodi il budget è più permissivo rispetto all'ipotesi iniziale a 33 nodi (~500 mW/nodo lato AC anziché ~150 mW/nodo), ma va confermato con numeri reali di corrente, non solo stime — *vedi backlog*.

## Requisito 8 eliminato

Analisi svolta: solo l'MCP25625 (CAN, via SPI) è rilevabile in modo affidabile e deterministico, ma è comunque presumibilmente presente su ogni variante. L'ultrasuoni permette un test funzionale trigger→echo, ma probabilistico (falsi negativi possibili se nessun ostacolo è a portata). Relè, ingressi isolati (PC817C) e PIR sono GPIO puramente passivi senza percorso di feedback: "non popolato" e "popolato ma inattivo" producono la stessa lettura elettrica, indistinguibili senza una revisione hardware con punti di sense dedicati.

Decisione: requisito eliminato interamente, non solo la clausola di autorilevamento. Conseguenza pratica: la firmware diventa uniforme su tutte le varianti hardware — nessuna logica condizionale per variante, nessuna OD versionata per hardware, nessuna dichiarazione di variante via DIP. Un componente non popolato resta semplicemente inerte (nessun evento generato, nessun danno) senza bisogno di essere rilevato o dichiarato.

### Bootloader/OTA (Req. 3)

Decisione: l'aggiornamento firmware via CAN è possibile solo con il master online, che orchestra l'intero trasferimento. Nessun percorso di OTA decentralizzato/senza master. Coerente con la scelta già fatta di limitare il requisito 1 (decentralizzazione) alla sola logica di business — l'OTA è un servizio di supervisione del master, non una funzione realtime.

**Fattibilità flash/dual-bank verificata:**

- Il modulo Waveshare RP2040-Tiny monta 2 MB di flash esterna QSPI (confermato da più fonti, vedi sorgenti in fondo al documento).
- L'RP2040 non ha supporto OTA/dual-bank nativo nell'SDK, ma è uno schema consolidato e già implementato da progetti open source di riferimento: pico-flashloader (~2,9 KB, power-fail safe), pico_fota_bootloader, picowota. Non è quindi da progettare da zero.
- Come dimensione di riferimento del protocollo open source, ho compilato (nel sandbox, gcc -Os, target x86-64) un sottoinsieme realistico di CANopenNode — NMT/Heartbeat, HBconsumer, Emergency, SDOserver, TIME, SYNC, PDO, LEDs, LSSslave, storage, glue CANopen.c, driver ed esempio "blank" — esempio utile per stimare ingombro e costi, ma non ancora sufficiente per chiudere il design operativo.

## Checklist post-HAL

Questa checklist resta come backlog tecnico, ma viene affrontata solo dopo l'implementazione del HAL.

### P0 - Blocca l'implementazione core

Tutte le voci P0 sono **chiuse a livello di specifica** da [canopen-network-spec.md](canopen-network-spec.md) (2026-07-22); l'implementazione segue la spec (vedi §11 della spec per il riepilogo dei lavori).

- [x] Definire lo stack CANopen da usare e i criteri di scelta → CANopenNode v4.1 (spec §1).
- [x] Progettare l'Object Dictionary minimo comune a tutti i nodi → ibrido CiA 401 (6000h) + manufacturer (2000h): etichette di ruolo e tabella regole autodescrittive (spec §4–5).
- [x] Fissare node-ID, schema di indirizzamento e regole per il DIP a 5 bit → **LSS con node-ID persistito in flash** (sostituisce il provvisorio node-ID = DIP+1 di `task_canopen.cpp`); DIP=0 normale, DIP≠0 override da banco (spec §3).
- [x] Definire mappa COB-ID, priorità e traffico realtime vs bulk → pre-defined connection set, TPDO event-driven, budget verificato ≤71 ms worst-case (spec §6).
- [x] Specificare comportamento NMT, heartbeat, emergency e stato di startup → self-start confermato; gli errori di comunicazione non degradano lo stato NMT; codici EMCY manufacturer (spec §7).
- [x] Chiarire la policy definitiva del pin MCP_STBY e il comportamento CAN controller in standby → chiuso a livello hardware: RST/STBY non cablati come GPIO, MCP25625 sempre in Normal mode (vedi `include/hal/pins.h` e sezione "Gestione energetica" sopra).

### P1 - Sblocca configurazione e aggiornamenti

- [ ] Definire il flusso OTA end-to-end con master online.
- [ ] Stabilire modalità di ingresso in update, verifica integrità e rollback/fail-safe.
- [ ] Definire i parametri configurabili via CAN e la loro persistenza.
- [ ] Specificare il formato dei messaggi diagnostici CAN e il mapping con l'OD.
- [ ] Definire quale parte della diagnostica deve restare disponibile in fault mode.

### Criterio di lavoro

- Chiudere prima il HAL.
- Passare a questi punti solo dopo che il HAL è stabile e usabile.
- Non tenere aperto un backlog P2 separato: le validazioni di consumo, latenza e bus load si faranno solo se emergono come necessarie dal prototipo.
