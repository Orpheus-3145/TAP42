# TAP — Protocol Draft v0.2

Shared reference between server and client (CLI/GUI). Keep it updated as
the open points at the bottom of the document get closed.

Server-side implementation status: all commands are implemented and
verified with functional tests (items, combat, quests, groups,
character creation/login). The world data is actually loaded from
`server/data/world.json` with full referential validation
(exits, placed items/npcs, quest targets — see
`server/src/world/loader.cpp`). The current world is an inn with a
basement that goes down into the sewers, 14 rooms in total with a closed
loop in the sewers and a dead-end branch toward the boss.

## 0. Persistence

The static world (rooms, items, npcs, quests — everything that comes from
`world.json`) stays in RAM only: no saving, it's lost and reloaded
identical on every restart.

Characters, on the other hand, are persisted to disk, one JSON file per
character under `server/data/characters/<name>.json` (see
`server/src/world/character_store.cpp`). Deliberate choice: one file per
character instead of a single database, so one player's save never takes
a lock that blocks another player's save — only two saves of the same
character (e.g. a quest completing because of another player's kill while
they're moving) serialize against each other. The file is rewritten after
every command that changes the character's state (MOVE, TAKE/DROP/USE,
ATTACK, TALK, quest completion) and again, as a safety net, on
disconnect. Every write goes through a temporary file first
(`<name>.json.tmp`) and is only renamed over the real file at the end: a
crash mid-write leaves the character with the previous save intact, never
with a truncated/corrupted JSON.

There are no credentials: a character's only identifier is its name,
which must be alphanumeric (plus `_` and `-`, max 32 characters) because
it ends up directly in the file path.

## 1. Framing rules

- TCP, UTF-8, **one message per line**, terminated by `\n`
- Every line is a complete message (no message split across multiple
  lines)
- Commands are `UPPERCASE`; free-form arguments (player names, chat text)
  keep their original case
- Right after accept, before the client sends anything at all, the
  server sends an unsolicited greeting line: `OK hello proto=1`. It is
  not the response to any command, it's just the "the server is alive and
  speaks this protocol version" signal — the client must wait for it
  before sending its first command, and must not reply to it.

## 2. Three message categories

| Direction | Prefix | Example |
|---|---|---|
| Client → Server | `<COMMAND> <args...>` | `MOVE north` |
| Server → Client (synchronous response) | `OK ...` / `ERR <code> <msg>` | `OK room=loc.bakery` |
| Server → Client (asynchronous event) | `EVT <category> <subtype> <args...>` | `EVT ROOM PRESENCE_ENTER alice` |

Every command gets exactly one OK/ERR response. EVTs can arrive at any
time; the client must be able to tell them apart from the pending
response without blocking.

## 3. Error format

```
ERR <3-digit numeric code> <SYMBOLIC_NAME> <human-readable detail>
```

The code is numeric as required by the RFC's ABNF grammar
(`error-code = 3DIGIT`). The symbolic name right after it is the
message's first token — a client can compare against that without having
to trust the text detail that follows, which is meant for human
logging/debugging. Where possible we reuse exactly the codes the RFC
assigns; for conditions the RFC doesn't cover (our CREATE_CHARACTER/
PROFILE, the persistent character system, etc.) we extend the scheme
staying in the same style (2xx identity, 3xx movement, 4xx resource not
found, same logic as the RFC itself reusing 404 for several conditions
with different text).

| Code | Symbolic name | When |
|---|---|---|
| 102 | `NOT_CONNECTED` | command requires CONNECT/CREATE_CHARACTER first |
| 200 | `BAD_MESSAGE` | unknown command or missing/malformed arguments |
| 201 | `NAME_IN_USE` | name already online, or already exists on disk (CREATE_CHARACTER) |
| 301 | `NO_EXIT` | invalid direction from this room |
| 401 | `NOT_IN_GROUP` | CHAT GROUP sent without being in a group |
| 402 | `ALREADY_IN_GROUP` | GROUP CREATE sent while already in a group |
| 201 | `NAME_IN_USE` | chosen group name (GROUP CREATE \<name\>) already used by another group |
| 404 | `ITEM_NOT_FOUND` | item not present in the room (TAKE) |
| 404 | `ITEM_NOT_IN_INVENTORY` | item not in inventory (DROP, USE) |
| 404 | `NPC_NOT_FOUND` | NPC not present in the room (TALK, ATTACK, USE) |
| 404 | `PLAYER_NOT_FOUND` | player not connected (GROUP INVITE) |
| 404 | `CHARACTER_NOT_FOUND` | CONNECT with a name that never did CREATE_CHARACTER |
| 404 | `QUEST_NOT_FOUND` | invalid quest id |
| 409 | `DEAD` | action (ATTACK or USE) not allowed because the player is at 0 HP |
| 500 | `INTERNAL` | generic server error |

Same 404 code for several conditions, different text — exactly like the
RFC itself does with `ITEM_NOT_FOUND`/`ITEM_NOT_IN_INVENTORY`/`NPC_NOT_FOUND`.

## 4. Commands (Client → Server)

### CREATE_CHARACTER — non-RFC extension
```
C: CREATE_CHARACTER <name> <race> [special_attributes...]
S: OK {"name":"...","race":"...","special_attributes":"...","room":"loc.start","hp":100,"max_hp":100,"inventory":[]}
S: ERR 200 BAD_MESSAGE ...
S: ERR 201 NAME_IN_USE ...
S: ERR 201 NAME_IN_USE ...   # session already connected to another character
```
Creates a new character and connects the current session to it, in one
step. `name` and `race` are a single token each; `special_attributes` is
free-form, can contain spaces, takes the rest of the line. Only `name`
is required: `race` and `special_attributes` can both be missing, they
simply stay empty strings until the client collects them.

`race` and `special_attributes` are fixed fields (the slot always exists)
but free-valued: the player writes whatever they want, the server saves
them and returns them as-is, without them ever affecting a stat or a dice
roll — they're only there for the client to display. All characters start
identical: same HP (100/100), same spawn point (`loc.start`), regardless
of what was written in these fields. The same spawn point is also used
for respawn after death (§ ATTACK).

**Compatibility alias**: `CREATE PLAYER <name>` (two tokens) is accepted
as an exact synonym of `CREATE_CHARACTER <name>` — it's the string the
CLI client sends today. Same handler, same response; documented here only
because it isn't the command's canonical name.

`NAME_IN_USE` triggers both if the name is already online and if a file
for that character already exists on disk from a previous session — a
name, once created, is reserved forever.

### CONNECT
```
C: CONNECT <name>
S: OK connected
S: ERR 201 NAME_IN_USE ...
S: ERR 404 CHARACTER_NOT_FOUND ...
```
CONNECT is the login for a character that already exists: the server
checks that the name isn't already online, loads the saved state from
`data/characters/<name>.json` (room, HP, inventory, quest progress,
dialogue progress — everything as it was at the last disconnect) and puts
it back into play exactly from there. A name that was never created with
CREATE_CHARACTER gives `404 CHARACTER_NOT_FOUND`, it doesn't create
anything on the client's behalf.

The success response is literally `OK connected`, with no payload:
RFC42TAP §5.1.1 specifies it exactly that way, so there's no room here to
slip the character's profile in. See PROFILE right below for that — the
client calls it after a successful CONNECT (or at any other time).

### PROFILE — non-RFC extension
```
C: PROFILE
S: OK {"name":"...","race":"...","special_attributes":"...","room":"...","hp":...,"max_hp":...,"inventory":[...]}
S: ERR 102 NOT_CONNECTED ...
```
Returns the exact same JSON block that CREATE_CHARACTER gives at creation
time: name, cosmetic fields, current room, HP and inventory. It isn't
tied to login time, it can be called at any point in the session, same
treatment as STATUS/INVENTORY.

Quests still get passed through the `in_progress` initialization on every
CONNECT (see §7, no explicit ACCEPT command for quests — the RFC doesn't
call for one): this only covers the case where `world.json` gained a new
quest since the character was last saved, so it doesn't stay stuck
forever on `not_started`. Quests already present in the save are left
untouched.

### LOOK
```
C: LOOK
S: OK {"room": {"id":"...","name":"...","description":"...","exits":{"north":"loc.tavern",...}}, "players":["alice"], "items":["item.herbs"], "npcs":["guard"]}
```

### MOVE
```
C: MOVE <direction>
S: OK room=<new_room_id>
S: ERR 301 NO_EXIT ...
```
Broadcast events (old and new room):
```
EVT ROOM PRESENCE_LEAVE <player>
EVT ROOM PRESENCE_ENTER <player>
```

### CHAT
```
C: CHAT <GLOBAL|ROOM|GROUP> <text...>
S: OK
S: ERR 401 NOT_IN_GROUP ...   # only for GROUP scope
EVT <SCOPE> CHAT {"player":"...","text":"..."}
```

### TAKE / DROP
```
C: TAKE <item id or exact name>
S: OK {"taken":"<item_id>"}
S: ERR 404 ITEM_NOT_FOUND ...

C: DROP <item id or exact name>
S: OK {"dropped":"<item_id>"}
```
Matches by exact id or exact display name (case-insensitive, e.g. `TAKE
Frothy Ale` or `TAKE item.ale`); multi-word supported. No partial or
fuzzy matching (`TAKE ale` alone does NOT match "Frothy Ale" — consistent
with the RFC's own `TAKE Herbs` example).

### INVENTORY
```
C: INVENTORY
S: OK ["item.herbs","item.bread"]
```

### TALK
```
C: TALK <npc id or exact name>
S: OK {"npc":"guard","dialogue":"Stay safe, traveler."}
S: ERR 404 NPC_NOT_FOUND ...
```
Design choice: **cyclic** dialogue — every TALK shows the next line in
the `dialogue` array, wrapping back to the first after the last one.
Deterministic and easy to test. Progress is **per player**, not shared:
two players talking to the same NPC each hear their own sequence from the
start, one doesn't "consume" lines for the other.

An NPC can also have `bonus_dialogue`: lines appended to the end of the
cycle only once the player has `completed` status on **all** the quests
listed in the NPC's `unlock_quest_ids`. Until they're all completed, the
cycle only uses `dialogue`; once unlocked, the cycle grows to include the
bonus lines too. Generic mechanism, not specific to any particular NPC.

### ATTACK
```
C: ATTACK <target>
S: OK {"target":"npc.guard","hit":true,"damage_dealt":12,"target_hp":8,"countered":true,"counter_damage":5,"player_hp":95,"respawned":false}
S: ERR 404 NPC_NOT_FOUND ...
S: ERR 409 DEAD ...   # the attacking player is already at 0 HP
```
Broadcast:
```
EVT ROOM COMBAT {"player":"...","npc":"...","damage":"...","health":"..."}        # only if the target survives
EVT ROOM COMBAT_DEATH_NPC {"player":"...","npc":"..."}                            # if the NPC dies (never together with COMBAT)
EVT ROOM COMBAT_DEATH_PLAYER {"player":"...","npc":"..."}                         # if the player dies
EVT ROOM PRESENCE_LEAVE <player>            # respawn: leaving the old room
EVT ROOM PRESENCE_ENTER <player>            # respawn: entering the safe room
```
`COMBAT_DEATH_NPC`/`COMBAT_DEATH_PLAYER` are two distinct events (not one
with an ambiguous id) so the client immediately knows how to interpret
the id that follows, without having to cross-reference other state to
figure out whether it was an NPC or a player that died. When the hit
kills the target, only the death event arrives (never also `COMBAT`):
whoever receives it already knows the last exchange was the fatal one.

**Design choice — combat**: resolved atomically for each ATTACK command
(the player's hit, then any NPC counterattack, in the same execution),
not as a "turn-based" state waiting on a second command. There is no
autonomous NPC action at all: if the player never sends ATTACK, the NPC
stays completely inert, even if it remains in its room forever. The
counterattack always reacts only to whoever just landed a hit, there's no
concept of aggro or a fixed target: if two players attack the same NPC,
each one risks its counterattack on their own hit.

Both the player's hit and the NPC's counterattack have a chance to miss:
player 85%, NPC 70%. The two rolls are independent — if the player
misses, the NPC still tries to counterattack, it isn't "spared" by the
player's failure. `hit` and `countered` in the response explicitly say
what happened, instead of having to infer it from zero damage.

Player damage: 10-15 (uniform). NPC counterattack: 3-8 by default, but
it's **per NPC** — an NPC can define a different range in the world data
(min==max for fixed damage, e.g. the bartender always deals exactly 100).
A defeated NPC is permanently removed from the room (no NPC respawn —
only players respawn, as required by the assignment). A player at 0 HP
respawns at `loc.start` with 50 HP.
`DEFEND`/`FLEE` are **not implemented**: they remain an open point (§6),
they aren't part of the RFC's base command set.

### USE — non-RFC extension
```
C: USE <item>
S: OK {"used":"item.potion_1","effect":{"healed":20,"hp":100}}                          # healing item
S: ERR 404 ITEM_NOT_IN_INVENTORY ...
S: ERR 409 DEAD ...

C: USE <item> <target>
S: OK {"used":"item.grenade_1","effect":{"target":"npc.rat_king","damage_dealt":20,"target_hp":0}}   # offensive item
S: ERR 200 BAD_MESSAGE ...            # offensive item used without a target
S: ERR 404 NPC_NOT_FOUND ...          # target not in the room
```
Broadcast:
```
EVT ROOM ITEM_USE {"player":"...","item":"..."}                                   # healing item, others aren't told how much it healed
EVT ROOM COMBAT {"player":"...","npc":"...","damage":"...","health":"..."}        # offensive item, same event as ATTACK — only if the target survives
EVT ROOM COMBAT_DEATH_NPC {"player":"...","npc":"..."}                            # if the target dies (never together with COMBAT)
```
Not part of the RFC's base command set, so we document it here as our own
extension. Every usable item has an `effect` (`heal` or `damage`) and a
fixed `amount` defined in the world data — no random variation on the
item's effect, only on ATTACK's hit. The item is always consumed on use,
whether the effect succeeds or there's no valid target doesn't change
that: if the target isn't there, the item isn't consumed (the error
arrives before removal from the inventory). An offensive item doesn't
roll to hit like ATTACK: if the target is in the room, the damage always
lands.

### STATUS
```
C: STATUS
S: OK {"hp":95,"max_hp":100,"in_combat":true,"target":"npc.guard"}
```
`in_combat` is true if the last NPC the player attacked is still alive.

### QUEST / QUESTS
```
C: QUESTS
S: OK [{"id":"quest.fetch_ale","name":"Fetch the Ale","status":"in_progress"}]

C: QUEST <id>
S: OK {"id":"quest.fetch_ale","name":"...","description":"...","type":"fetch","status":"completed"}
S: ERR 404 QUEST_NOT_FOUND ...
```
Broadcast event to only the player involved when a quest completes:
```
EVT ROOM QUEST_COMPLETE <player> <quest_id>
```
`ROOM` category for consistency with the `EVT <category> <single-word
subtype>` scheme used everywhere else, even though the send is direct to
the player involved and not an actual room broadcast.
**Design choice — progression**: two quest types, `fetch` (completes on
`TAKE`-ing the target item, a single `target_id`) and `defeat` (completes
when **all** the NPCs in `target_ids` are dead — a list, not a single id,
so "kill the boss" and "kill every rat" use the same mechanism, only the
list's length changes). No ACCEPT command: every quest is `in_progress`
for everyone from the moment of CONNECT (or already `completed` if
another player had already finished a `defeat` quest before this one
connected).

**Design choice — world-level completion of defeat quests**: a `defeat`
quest is not personal credit for whoever landed the final blow — it's the
state of the world that counts. When the last NPC on the list dies,
**every** player who still has that quest `in_progress` sees it complete
at the same instant, not just whoever killed that last NPC. If two
players split the targets (one kills half, the other the rest), the quest
still completes for both. As a consequence a `defeat` quest **never has a
`reward_item_id`** in the current world data: handing the same item to
multiple players at the same instant would violate instance uniqueness.
`fetch` quests, on the other hand, remain individual credit (whoever
picks up the item).

### WHO
```
C: WHO
S: OK {"room":["alice","bob"],"server":5}
```

### GROUP
```
C: GROUP CREATE [name]
S: OK group=<group_id>
S: ERR 402 ALREADY_IN_GROUP ...  # you're already in a group
S: ERR 201 NAME_IN_USE ...       # the chosen name is already used by another group

C: GROUP INVITE <player>
S: OK {"invited":"<player>"}
S: ERR 404 PLAYER_NOT_FOUND ...   # player not connected

C: GROUP ACCEPT <inviter>
S: OK {"joined":"<group_id>"}
S: ERR 200 BAD_MESSAGE ...        # no pending invite from that player
```
**Compatibility alias**: `GROUP JOIN <inviter>` is accepted as an exact
synonym of `GROUP ACCEPT <inviter>` — it's the string the CLI client
sends today instead of ACCEPT.
```

C: GROUP LEAVE
S: OK {"left":"<group_id>"}
```
Events broadcast to the group's members (not to whoever performed the
action):
```
EVT GROUP INVITE {"player":"...","group":"..."}     # only to the invitee
EVT GROUP JOIN {"player":"...","group":"..."}
EVT GROUP LEAVE {"player":"...","group":"..."}
```
**Design choice**: one pending invite per invitee (a new INVITE overwrites
the previous one). The group id is the name chosen in `GROUP CREATE
<name>`, if given; otherwise (or if the group is implicitly created by
the first INVITE) it matches the name of whoever founded it — same
behavior as before, just extended. A group name, like a character name,
is unique only while the group exists: once it's disbanded (last member
leaves) it becomes available again. On disconnect a player is
automatically removed from their group (with the corresponding
`EVT GROUP LEAVE`). `GROUP CREATE` was added because the RFC lists it
explicitly as its own command; before, only the implicit creation on the
first INVITE existed, which remains unchanged for compatibility — they're
two ways to reach the same state, neither replaces the other.

### QUIT
```
C: QUIT
S: OK bye
(connection closed by the server)
EVT ROOM PRESENCE_LEAVE <player>
EVT GROUP LEAVE {"player":"...","group":"..."}   # if the player was in a group
```

### STATS — broadcast event
```
EVT STATS {"count":<n>}
```
Required by RFC42TAP §6.2.4 (`EVT STATS players=<n>`, here as a JSON
payload for consistency with the rest of the protocol). It isn't the
response to a command: the server sends it to **every** connected session
whenever the number of online players changes — a successful CONNECT or
CREATE_CHARACTER, or a disconnect. The count is that of registered
sessions (`SessionRegistry`), i.e. characters actually online, not plain
TCP connections waiting to log in.

## 5. JSON payload conventions

- Items/NPCs/rooms have a **stable id** (`item.herbs`, `loc.tavern`,
  `npc.guard`) used internally, plus a human-readable `name` for the user
- User commands can refer to either the id or the exact name
  (case-insensitive) — the server resolves it
- Empty lists are `[]`, not the field's absence

## 6. Open points

1. DEFEND/FLEE: whether and how to add them (not required by the base
   command set)
2. Whether a player can be invited into several groups at the same time
   (currently: no, one `player_group` per player)
3. Exact naming of the JSON fields (snake_case chosen above — confirmed?)

## 7. Quick reference: quest/combat status

See the source code for exact details:
- `server/src/commands/combat_commands.cpp` — damage formulas, hit
  chances, respawn
- `server/src/commands/item_commands.cpp` — TAKE/DROP/INVENTORY and USE
- `server/src/commands/quest_commands.cpp` — completion triggers
- `server/src/world/loader.cpp` — items, npcs, quests currently defined
  in the world data
- `server/src/world/character_store.cpp` — character creation/login/save
  to disk
