*This project has been created as part of the 42 curriculum by mtaranti, faru.*

# TAP42

## Description

TAP, or The Answer Protocol, is a MUD (multi-user dungeon): a multiplayer text-adventure game on a client-server architecture over TCP. Several players share one world — an inn on a dusty road whose basement leads down into rat-infested sewers — where they can explore rooms, chat, pick up and drop items, talk to NPCs, fight, complete quests and form groups, all in real time.

The project delivers three programs that talk the line-based protocol defined in the RFC 42TAP (`rfc42TAP.html`, working notes and our extensions in `PROTOCOL.md`):

- **`tap-server`** — the game server (C++20, standard library + POSIX sockets only).
- **`tap-client-cli`** — a terminal client built on ncurses.
- **`tap-client-gui`** — a graphical client built on Qt 6.

Static world data lives in `server/data/world.json` (map in `server/data/rat-cellar-map.html`).

## Instructions

### Requirements

- `make`, `cmake`, a C++20 compiler with `<format>` or the bundled fallback (g++ 10+ / clang tested)
- `libncurses-dev` (CLI client)
- `qt6-base-dev` (GUI client)
- `cppcheck` (optional, for `make lint`)

`make deps` checks what is missing.

### Building and Running

All commands are run from the repository root.

| Target | What it does |
|---|---|
| `make deps` | Shows dependency info and warns about missing tools |
| `make server` | Builds `server/tap-server` |
| `make cli` / `make gui` | Configures CMake (with the same `CXX` as the server, g++ by default) and builds the CLI / GUI client into `client/` |
| `make run-server` | Builds and runs the server on port `4242` (`./tap-server [port]` to choose another) |
| `make run-client` (`run-cli`) | Runs the CLI client against `127.0.0.1:4242` |
| `make run-client-gui` (`run-gui`) | Runs the GUI client against `127.0.0.1:4242` |
| `make run-cli-debug` / `run-gui-debug` | Same, with sanitizers enabled |
| `make lint` | Runs `cppcheck` on the server sources (skipped with a message if not installed) |
| `make clean` / `make fclean` | Removes build artifacts / also the client build directory |

Typical session, in three terminals:

```bash
make run-server
make run-client
make run-client-gui
```

Clients take `-i <host> -p <port>`; `-h` prints usage.

## Resources

- RFC 42TAP (`rfc42TAP.html`) and our protocol notes in `PROTOCOL.md`
- [cppreference](https://en.cppreference.com/) — C++20 library and language reference
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — sockets, `poll`
- [ncurses programming HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/)
- [Qt 6 documentation](https://doc.qt.io/qt-6/)
- [MUD (Wikipedia)](https://en.wikipedia.org/wiki/MUD) — background on the genre

**AI usage.** Claude (Anthropic) was used as an assistant for: making the project build on older toolchains (a `<format>` fallback, compiler warnings), adding the outgoing-response logging in the server, fixing the `make lint` target, running manual protocol tests against the server, and drafting this README. All AI-generated changes were reviewed and tested by the team, who remain responsible for them; the game design, server logic and both clients were written by the team.

## Architecture

**Server** (`server/src`)

- *Concurrency model:* one OS thread per connection. The accept loop (`network/server.cpp`) hands each client a thread that blocks on `recv()` and feeds complete lines (`network/line_reader.cpp`, 8192-byte cap) to the dispatcher. All shared game state (`World`: rooms, players, NPCs, items, quests, groups) sits behind **one mutex**; every command takes it, mutates state, and releases it. This is deliberately simple and obviously correct, at the cost of serialising commands — fine for a small shared world.
- *Dispatcher vs. inline handling:* a **dispatcher/router** (`commands/dispatcher.cpp`) parses the line, logs it, runs the flood check and routes by command name to handlers grouped by domain: `combat_commands`, `item_commands`, `npc_commands`, `quest_commands`, `group_commands`; movement, chat and presence live in the dispatcher.
- *Writes:* all outgoing lines go through `send_line()`, which takes a per-session `write_mutex` and checks `connected` under the same lock, so a send can never race with the socket being closed. It is also the single place where responses/errors/events are logged.
- *Disconnects:* player state is removed **before** the leave event is broadcast; broadcasts to sessions that vanished mid-send are skipped silently.
- *World:* loaded from JSON by a hand-written parser, then validated (every exit, placed item/NPC and quest target must reference something that exists); the server refuses to start on an invalid world.

**Clients** (`client/`) — a static library `TapShared` (TCP connection with `poll`, message parsing, argument parsing, logging, command suggestions) used by two front-ends, `cli/` (ncurses) and `gui/` (Qt Widgets). Both keep reading server events while waiting for user input.

## Protocol Implementation

We implement the full RFC 42TAP command set: `CONNECT`, `LOOK`, `MOVE`, `CHAT`, `TAKE`, `DROP`, `INVENTORY`, `TALK`, `ATTACK`, `STATUS`, `QUEST`, `QUESTS`, `WHO`, `GROUP`, `QUIT`, plus the `EVT` push events. Deviations and extensions (all documented in `PROTOCOL.md`):

- **`CREATE_CHARACTER <name> [race] [attrs…]`** (alias `CREATE PLAYER <name>`) — non-RFC. Characters are persistent and `CONNECT` is login for an existing one, so a creation step was needed. Success reply is the same `OK connected`.
- **`PROFILE`** and **`USE <item>`** — non-RFC (character data, healing/offensive items).
- **`GROUP CREATE [name]`** — the RFC lists it in the command table, we gave it a concrete behaviour.
- **Extra error codes** for conditions the RFC does not cover (`NAME_IN_USE`, `DEAD`, `CHARACTER_NOT_FOUND`, …) in the same numeric style; the symbolic name is always the first token after the code.
- **Persistence**, although not required, was added: one JSON file per character under `server/data/characters/`, written atomically (temp file + rename).
- `DEFEND` and `FLEE` are **not** implemented (see Combat System).

## Combat System

See *Design choices: combat and quest systems* below for the full description and justification.

## Quest System

See *Design choices: combat and quest systems* below.

## World Design

14 rooms, forming a closed loop in the sewers plus a dead-end branch to the boss:

- **Surface/inn:** Traveler's Road (spawn) → Inn Hall → Inn Corridor → Guest Rooms ×2, Bathroom; Inn Attic (up from the hall); Inn Basement (down).
- **Sewers:** Sewer Entrance → Sewer Passage → Flooded Chamber ↔ Stagnant Pool → Curving Passage (closing the loop), with a dead-end branch to the Rat Nest.
- **NPCs (10, three roles):** the *Bartender* (dialogue + quest hook, tough counter-attacker), eight *Sewer Rats* (enemies) and the *Rat King* (boss).
- **Items:** three Potions and one Grenade, placed in the world as unique instances (taking removes it from the room, dropping makes it available to others; items can be referenced by id or display name, multi-word names supported).
- **Quests:** *Clear the Rats* and *Defeat the Rat King* (both `defeat` quests).

## Server Logging

Every line on **stdout** is one JSON object: `{"ts":"<ISO-8601 UTC>","level":"INFO|WARN|ERROR","event":"<name>", …fields}`. Redirect it to a file to keep it (`./tap-server 4242 > server.log`) and filter with `grep`/`jq`.

| Level | Events |
|---|---|
| INFO | `server_started`, `world_loaded`, `client_connected` (with `ip`), `client_disconnected`, `player_connected`, `player_disconnected`, `character_created`, `command_received` (player + command), `response_sent` (player + `OK`), `event_sent` (player + event type), `player_moved`, `chat`, `item_taken`, `item_dropped`, `item_used`, `combat_attack`, `npc_defeated`, `player_respawned`, `quest_completed`, `quest_world_cleared`, `group_created`, `group_invite`, `group_join`, `group_leave` |
| WARN | `error_sent` (player, numeric `code`, symbolic `name`), `command_flood_detected`, `rapid_reconnect_detected`, `line_too_long`, `partial_message_stalled`, `accept_failed` |
| ERROR | `world_load_failed`, `world_parse_failed`, `world_validation_failed`, `character_save_failed`, `character_load_failed`, `bind_failed`, `listen_failed`, `socket_create_failed`, `platform_init_failed` |

**Abuse detection** (`logging/abuse_monitor.cpp`) is intentionally log-only: more than 10 commands/second on one session logs `command_flood_detected`; more than 5 connections in 10 s from one IP logs `rapid_reconnect_detected`. Nobody is blocked. Logging is a single `printf` per event and sending is logged after the write lock is released, so it does not slow down the game loop. To monitor a running server: `tail -f server.log | grep '"level":"WARN"'`.

## Group Contributions

| Member | Login | Responsibilities |
|---|---|---|
| Marco Tarantino | mtaranti | Server: network layer, command dispatcher and handlers (movement, items, NPCs, combat, quests, groups), world loading/validation, persistence, structured logging and abuse monitor, `PROTOCOL.md`, world design |
| Francesco | faru | Clients: shared client library, CLI client (ncurses), GUI client (Qt), command suggestions, client logging, CMake build |

## Testing

1. Build and start a server: `make run-server`.
2. **Multiplayer:** start two clients (`make run-client` in two terminals, or one CLI and one GUI), create two characters, then check that `WHO` lists both, `CHAT GLOBAL hi` appears on the other client, and `MOVE` produces `EVT ROOM PRESENCE_*` events on the other side. Quit one client and check the other sees the leave event and the player count drop.
3. **Without a client:** the protocol is plain text, so `nc 127.0.0.1 4242` works:
   ```
   CREATE_CHARACTER alice human
   LOOK
   MOVE east
   STATUS
   ```
4. **Items:** `TAKE`/`DROP` an item from two clients and check it disappears from / appears in the other client's `LOOK`.
5. **Combat:** go down to the sewers (`MOVE east` to the Inn Hall, `MOVE down` to the basement, then on to the Sewer Entrance), `ATTACK Sewer Rat` repeatedly, watch `STATUS` HP change; let HP reach 0 to see the respawn.
6. **Quests:** `QUESTS` before and after killing every rat — *Clear the Rats* flips to `completed` for everyone who had it in progress.
7. **Robustness:** send garbage (`nc` + `FOO`) and expect `ERR 200 BAD_MESSAGE`; paste more than 10 lines at once and check `command_flood_detected` in the server log.
8. `make lint` runs static analysis on the server.

## Known Limitations

- The GUI game window is a set of panels (player info, world events, user events, chat) driven by typed commands with autocompletion. It does **not** yet have action buttons, an inventory panel with TAKE/DROP buttons, separate Global/Room/Group chat views, or player counters.
- `DEFEND` and `FLEE` are not implemented.
- The world has three NPC roles (dialogue/quest, enemy, boss); the quest-giver is the bartender's dialogue rather than a dedicated NPC type.

## Client usage

#### Building
From the client/ folder:
	. `make cli` to build the Command Line interface
	. `make gui` to build the Graphic interface

#### Running
`tap-client -i <hostname> -p <port>`

`tap-client -h` for usage

#### Session
- The CLI version is supposed to be played with only with the keyboard, mouse won't be necessary.
- `shared/include/Config.hpp` has some basic customization of the UI, that the minimum size shuldn't be reduced to avoid bugs.
- Logs are stored inside `client/logs`.

##### Keys:
1. `tab` for switching focus (user events - world events - chat events) between tabs (text or buttons) of the window; a highlighted tab will blink repeatedly 
2. `up` and `down` arrows for checking the history of the previous input messages
3. `back-tab` to suggest a command (case-insesitive) from the given input
4. Other keyboard cursor manipulations (`del`, `backspace`, `home`, `end`, `left/right` arrow) work as in a normal terminal
5. Press `ENTER` to send an input or to activate a button

Note that the client will expected the TCP handshake from server before forwarding the username.

## Design choices: combat and quest systems

RFC42TAP §6.1 leaves the combat and quest systems intentionally underspecified and requires implementers to document and justify their own design choices here. This section covers what the server actually does and why, matching the RFC's own list of open points one by one.

### Combat system

- **Turn-based mechanics.** There is no turn state waiting on a second command. An `ATTACK` resolves the player's hit and, if the target survives, its counterattack in the same request/response — the player "acts", the server immediately computes both sides of the exchange, and the reply already carries the full outcome. This was chosen over a real turn queue because a shared-mutex, thread-per-connection server has no natural place to park "waiting for the other side's move" state without adding a second round of messages and a timeout for players who never send it; resolving everything inside one command keeps combat trivially thread-safe and easy to test.
- **Damage calculation.** Player damage is a uniform random roll (10–15). Both sides can also miss: the player hits 85% of the time, an NPC's counter lands 70% of the time, rolled independently — a miss on one side never cancels the other side's own roll. An NPC that survives counters with its own uniform roll, 3–8 by default, but that range is **per NPC**: the world data can override it per NPC, including setting min==max for a fixed hit (the bartender, for instance, always counters for exactly 100). No defense stat, no critical hits: the RFC does not require them and a flat damage range with a miss chance is enough to make fights take more than one exchange without adding a stat system nothing else in the project needs.
- **Combat states.** There is no explicit "in combat" flag. Combat begins the moment `ATTACK` is sent and ends the moment the target (or the player) reaches 0 HP; `STATUS` derives an `in_combat` reading from whether the player's last attacked NPC is still alive, rather than tracking a separate state machine. A player who dies respawns immediately, in the same command, at a fixed safe room with a fixed HP value — death is never a dead end that needs a separate command to recover from.
- **Additional commands.** `DEFEND` and `FLEE` are not implemented. The RFC's base command set doesn't require them, and adding either would need a real turn model (see above) to mean anything, which was a deliberate scope cut for this iteration. A non-RFC `USE` command was added instead, for item-based actions (healing potions, offensive items) that don't need a turn model to make sense.
- **Status effects.** None. Items and combat only ever change HP directly and immediately; there is no duration-tracked state (poison, buffs, debuffs) anywhere in `PlayerState` or `Npc`. Out of scope for the same reason as DEFEND/FLEE: no turn model to tick effects against.

### Quest system

- **Quest progression.** Two quest types: `fetch` (bring back a specific item) and `defeat`, which can name either one NPC or a whole list of them (e.g. "clear every rat in the sewers"). Every quest defined in the world data is seeded as `in_progress` for a player automatically on login — there's no accept step, since the RFC doesn't ask for one and one more round trip wouldn't change how the quest is tracked afterward.
- **Quest completion.** Fully automatic. `TAKE`-ing a fetch quest's target item flips it to `completed` immediately. A `defeat` quest completes the moment the *last* NPC on its list dies — and at that point it completes for **every player** who still has that quest in progress, not just whoever landed the final kill. Defeat is world state ("the sewers are clear"), not personal credit, so it wouldn't make sense to reward only the one player who happened to get the kill.
- **Quest rewards.** A quest may optionally name a `reward_item_id`, added straight to the player's inventory the instant the quest completes. Quests that can complete for several players at once deliberately have no reward item: handing the same item id to multiple inventories simultaneously would break instance uniqueness, so those are reward-free by design rather than by oversight.
- **Quest dependencies.** Not a quest-to-quest prerequisite chain, but a generic NPC dialogue unlock: an NPC can define bonus dialogue lines that only join its conversation cycle once a player has `completed` status on every quest in that NPC's unlock list. Reusable for any NPC/quest combination, not hardcoded to one storyline.
- **Additional commands.** No `COMPLETE_QUEST` or `ABANDON_QUEST`: since completion is automatic and always forward-only, neither has anything to do yet.

### Persistent characters

Players are no longer pure session state. `CREATE_CHARACTER` makes a named character once (with a couple of cosmetic, gameplay-inert fields — race, special attributes); `CONNECT` is login for a character that already exists, restoring room, HP, inventory, quest progress and dialogue progress exactly as they were. Full protocol details and the persistence design (one JSON file per character, atomic writes) are in `PROTOCOL.md` rather than duplicated here, since this section exists for the RFC's combat/quest justification requirement specifically.
