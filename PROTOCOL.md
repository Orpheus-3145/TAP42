# TAP — Protocol Draft v0.2

Riferimento condiviso tra server e client (CLI/GUI). Da tenere aggiornato man
mano che si chiudono i punti aperti in fondo al documento.

Stato implementazione lato server: tutti i comandi sono implementati e
verificati con test funzionali (item, combattimento, quest, gruppi). Il
world data è caricato davvero da `server/data/world.json` con validazione
referenziale completa (exits, item/npc piazzati, target delle quest — vedi
`server/src/world/loader.cpp`); manca ancora l'espansione del mondo alle
dimensioni minime richieste dal subject (8+ stanze, ecc.) — quello è lavoro
di game design (Fase 2), non di protocollo o di caricamento dati.

## 0. Persistenza

Nessuna persistenza: tutto lo stato del mondo vive in RAM per la durata del
processo server e viene perso al restart. Nessun salvataggio su file/DB
durante la partita.

## 1. Regole di framing

- TCP, UTF-8, **un messaggio per riga**, terminato da `\n`
- Ogni riga è un messaggio completo (nessun messaggio spezzato su più righe)
- I comandi sono `UPPERCASE`; gli argomenti liberi (nomi giocatore, testo
  chat) mantengono il case originale

## 2. Tre categorie di messaggio

| Direzione | Prefisso | Esempio |
|---|---|---|
| Client → Server | `<COMANDO> <args...>` | `MOVE north` |
| Server → Client (risposta sincrona) | `OK ...` / `ERR <code> <msg>` | `OK room=loc.bakery` |
| Server → Client (evento asincrono) | `EVT <categoria> <sottotipo> <args...>` | `EVT ROOM PRESENCE ENTER alice` |

Ogni comando riceve esattamente una risposta OK/ERR. Gli EVT arrivano in
qualsiasi momento; il client deve distinguerli dalla risposta pendente senza
bloccarsi.

## 3. Formato errori

```
ERR <CODE> <messaggio leggibile>
```

| CODE | Significato |
|---|---|
| `ERR_UNKNOWN_CMD` | comando non riconosciuto |
| `ERR_BAD_ARGS` | argomenti mancanti/malformati |
| `ERR_NOT_CONNECTED` | comando richiede CONNECT prima |
| `ERR_ALREADY_CONNECTED` | CONNECT già effettuato |
| `ERR_NAME_TAKEN` | nome già in uso da un altro giocatore connesso |
| `ERR_NO_EXIT` | direzione non valida da questa stanza |
| `ERR_ITEM_NOT_FOUND` | item non presente/riferito male |
| `ERR_NPC_NOT_FOUND` | NPC non presente nella stanza |
| `ERR_TARGET_NOT_FOUND` | target ATTACK/GROUP INVITE non valido o non connesso |
| `ERR_DEAD` | azione non permessa perché il player è a 0 HP |
| `ERR_QUEST_NOT_FOUND` | id quest non valido |
| `ERR_NOT_IN_GROUP` | CHAT GROUP inviato senza far parte di un gruppo |
| `ERR_INTERNAL` | errore server generico |

## 4. Comandi (Client → Server)

### CONNECT
```
C: CONNECT <name>
S: OK connected
S: ERR ERR_ALREADY_CONNECTED ...
S: ERR ERR_NAME_TAKEN ...
```
All'atto della CONNECT, tutte le quest del mondo vengono inizializzate a
`in_progress` per il player (vedi §7, nessun comando ACCEPT esplicito per le
quest — RFC non lo prevede).

### LOOK
```
C: LOOK
S: OK {"room": {"id":"...","name":"...","description":"...","exits":{"north":"loc.tavern",...}}, "players":["alice"], "items":["item.herbs"], "npcs":["guard"]}
```

### MOVE
```
C: MOVE <direction>
S: OK room=<new_room_id>
S: ERR ERR_NO_EXIT ...
```
Eventi broadcast (vecchia e nuova stanza):
```
EVT ROOM PRESENCE LEAVE <player>
EVT ROOM PRESENCE ENTER <player>
```

### CHAT
```
C: CHAT <GLOBAL|ROOM|GROUP> <text...>
S: OK
S: ERR ERR_NOT_IN_GROUP ...   # solo per scope GROUP
EVT <SCOPE> CHAT <player> <text...>
```

### TAKE / DROP
```
C: TAKE <item id o nome esatto>
S: OK taken=<item_id>
S: ERR ERR_ITEM_NOT_FOUND ...

C: DROP <item id o nome esatto>
S: OK dropped=<item_id>
```
Matching per id esatto o display name esatto (case-insensitive, es. `TAKE
Frothy Ale` o `TAKE item.ale`); multi-word supportato. Nessun match parziale
o fuzzy (`TAKE ale` da solo NON matcha "Frothy Ale" — coerente con l'esempio
RFC `TAKE Herbs`).

### INVENTORY
```
C: INVENTORY
S: OK ["item.herbs","item.bread"]
```

### TALK
```
C: TALK <npc id o nome esatto>
S: OK {"npc":"guard","dialogue":"Stay safe, traveler."}
S: ERR ERR_NPC_NOT_FOUND ...
```
Design choice: dialogo **ciclico** — ogni TALK mostra la riga successiva
dell'array `dialogue`, tornando alla prima dopo l'ultima. Deterministico e
facile da testare.

### ATTACK
```
C: ATTACK <target>
S: OK {"target":"npc.guard","damage_dealt":12,"target_hp":8,"counter_damage":5,"player_hp":95,"respawned":false}
S: ERR ERR_TARGET_NOT_FOUND ...
S: ERR ERR_DEAD ...   # il player attaccante è già a 0 HP
```
Broadcast:
```
EVT ROOM COMBAT <attacker> <target> <damage> <target_hp_remaining>
EVT ROOM COMBAT_DEATH <npc_id>              # se l'NPC muore
EVT ROOM COMBAT_DEATH <player>              # se il player muore
EVT ROOM PRESENCE LEAVE <player>            # respawn: uscita dalla stanza vecchia
EVT ROOM PRESENCE ENTER <player>            # respawn: ingresso nella stanza sicura
```

**Design choice — combattimento**: risolto in modo atomico per ogni comando
ATTACK (un colpo del player, poi eventuale contrattacco NPC nella stessa
esecuzione), non come stato "a turni" che aspetta un secondo comando. Danno
player: 10-15 (uniforme). Contrattacco NPC: 3-8, solo se l'NPC sopravvive al
colpo. Un NPC sconfitto viene rimosso definitivamente dalla stanza (nessun
respawn NPC — solo i player respawnano, come richiesto dal subject). Un
player a 0 HP respawna a `loc.start` con 50 HP.
`DEFEND`/`FLEE` **non sono implementati**: restano un punto aperto (§6), non
fanno parte del set di comandi base dell'RFC.

### STATUS
```
C: STATUS
S: OK {"hp":95,"max_hp":100,"in_combat":true,"target":"npc.guard"}
```
`in_combat` è vero se l'ultimo NPC attaccato dal player è ancora vivo.

### QUEST / QUESTS
```
C: QUESTS
S: OK [{"id":"quest.fetch_ale","name":"Fetch the Ale","status":"in_progress"}]

C: QUEST <id>
S: OK {"id":"quest.fetch_ale","name":"...","description":"...","type":"fetch","status":"completed"}
S: ERR ERR_QUEST_NOT_FOUND ...
```
Evento broadcast al solo player interessato quando una quest si completa:
```
EVT QUEST COMPLETE <player> <quest_id>
```
**Design choice — progressione**: due tipi di quest, `fetch` (si completa al
`TAKE` dell'item target) e `defeat` (si completa quando il player uccide
l'NPC target via `ATTACK`). Nessun comando ACCEPT: ogni quest è
`in_progress` per tutti dal momento della CONNECT. Reward: se la quest ha un
`reward_item_id`, quell'item viene aggiunto all'inventario del player al
completamento — l'id di reward non è mai piazzato in nessuna stanza, così
resta un'istanza unica anche dopo l'assegnazione.

### WHO
```
C: WHO
S: OK {"room":["alice","bob"],"server":5}
```

### GROUP
```
C: GROUP INVITE <player>
S: OK invited=<player>
S: ERR ERR_TARGET_NOT_FOUND ...   # player non connesso

C: GROUP ACCEPT <inviter>
S: OK joined=<group_id>
S: ERR ERR_BAD_ARGS ...           # nessun invito pendente da quel giocatore

C: GROUP LEAVE
S: OK left
```
Eventi broadcast ai membri del gruppo (non a chi compie l'azione):
```
EVT GROUP INVITE <inviter>     # solo all'invitato
EVT GROUP JOIN <player>
EVT GROUP LEAVE <player>
```
**Design choice**: un solo invito pendente per invitato (un nuovo INVITE
sovrascrive il precedente). Il group id coincide con il nome di chi lo ha
fondato (chi manda il primo INVITE). Alla disconnessione un player viene
rimosso automaticamente dal proprio gruppo (con relativo `EVT GROUP LEAVE`).

### QUIT
```
C: QUIT
S: OK bye
(connessione chiusa dal server)
EVT ROOM PRESENCE LEAVE <player>
EVT GROUP LEAVE <player>   # se il player era in un gruppo
```

## 5. Convenzioni sui payload JSON

- Item/NPC/room hanno un **id stabile** (`item.herbs`, `loc.tavern`,
  `npc.guard`) usato internamente, più un `name` leggibile per l'utente
- I comandi utente possono riferirsi sia per id che per nome esatto
  (case-insensitive) — il server risolve
- Liste vuote sono `[]`, non l'assenza del campo

## 6. Punti aperti

1. DEFEND/FLEE: se e come aggiungerli (non richiesti dal set di comandi base)
2. Se un player può essere invitato in più gruppi contemporaneamente (oggi:
   no, un solo `player_group` per player)
3. Naming esatto dei campi JSON (snake_case scelto sopra — confermato?)

## 7. Riferimento rapido stato quest/combattimento

Vedi il codice sorgente per i dettagli esatti:
- `server/src/commands/combat_commands.cpp` — formule danno, respawn
- `server/src/commands/quest_commands.cpp` — trigger di completamento
- `server/src/world/loader.cpp` — quest e reward attualmente definite (dati
  placeholder, da espandere in Fase 2 di game design)
