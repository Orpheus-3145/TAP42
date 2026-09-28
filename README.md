# TAP42
TAP, or The Answer Protocol, is a MUD (multi-user dungeon), or a multiplayer text-adventure game, served on a client-server architecture based on TCP protocol, as specifically defined inside the RFC.42TAP document protocol

## Design choices: combat and quest systems

RFC42TAP §6.1 leaves the combat and quest systems intentionally underspecified and requires implementers to document and justify their own design choices here. This section covers what the server actually does and why, matching the RFC's own list of open points one by one.

### Combat system

- **Turn-based mechanics.** There is no turn state waiting on a second command. An `ATTACK` resolves the player's hit and, if the target survives, its counterattack in the same request/response — the player "acts", the server immediately computes both sides of the exchange, and the reply already carries the full outcome. This was chosen over a real turn queue because a shared-mutex, thread-per-connection server has no natural place to park "waiting for the other side's move" state without adding a second round of messages and a timeout for players who never send it; resolving everything inside one command keeps combat trivially thread-safe and easy to test.
- **Damage calculation.** Player damage is a uniform random roll (`PLAYER_DAMAGE_MIN`–`PLAYER_DAMAGE_MAX`, currently 10–15). An NPC that survives counters with its own uniform roll (`NPC_COUNTER_DAMAGE_MIN`–`NPC_COUNTER_DAMAGE_MAX`, currently 3–8). No defense stat, no critical hits: the RFC does not require them and a flat damage range is enough to make fights take more than one exchange without adding a stat system nothing else in the project needs yet.
- **Combat states.** There is no explicit "in combat" flag. Combat begins the moment `ATTACK` is sent and ends the moment the target (or the player) reaches 0 HP; `STATUS` derives an `in_combat` reading from whether the player's last attacked NPC is still alive, rather than tracking a separate state machine. A player who dies respawns immediately, in the same command, at a fixed safe room with a fixed HP value (`RESPAWN_HP`) — death is never a dead end that needs a separate command to recover from.
- **Additional commands.** `DEFEND` and `FLEE` are not implemented. The RFC's base command set doesn't require them, and adding either would need a real turn model (see above) to mean anything, which was a deliberate scope cut for this iteration.
- **Status effects.** None. Items and combat only ever change HP directly and immediately; there is no duration-tracked state (poison, buffs, debuffs) anywhere in `PlayerState` or `Npc`. Out of scope for the same reason as DEFEND/FLEE: no turn model to tick effects against.

### Quest system

- **Quest progression.** Two quest types: `fetch` (bring back a specific item) and `defeat` (kill a specific NPC). Every quest defined in the world data is seeded as `in_progress` for a player automatically on `CONNECT` — there's no accept step, since the RFC doesn't ask for one and one more round trip wouldn't change how the quest is tracked afterward.
- **Quest completion.** Fully automatic. `TAKE`-ing a fetch quest's target item, or defeating a defeat quest's target NPC, flips that quest to `completed` for the acting player the moment it happens, with no manual confirmation step.
- **Quest rewards.** A quest may optionally name a `reward_item_id`, added straight to the player's inventory the instant the quest completes. A quest with no reward simply leaves that field empty.
- **Quest dependencies.** Not implemented in this iteration — each quest here is independent, with no prerequisite chain.
- **Additional commands.** No `COMPLETE_QUEST` or `ABANDON_QUEST`: since completion is automatic and always forward-only, neither has anything to do yet.

### Planned extensions (in progress on other branches)

A few refinements to the above are built and tested but not yet merged into `main`:
- Hit/miss chance on both sides of an `ATTACK`, and a per-NPC override for counter-damage instead of one fixed range for every NPC (`game-design-implementation` branch).
- Defeat quests that name more than one target NPC (e.g. "clear every rat"), completing for every player who still has the quest in progress the moment the last one dies, plus NPC dialogue that unlocks new lines once specific quests are done (`game-design-implementation` branch).
- Persistent, named characters (`CREATE_CHARACTER`/`CONNECT` as separate creation and login steps, with character state saved to disk instead of reset on every reconnect) (`character-persistence` branch).
