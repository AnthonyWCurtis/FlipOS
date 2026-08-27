# Phase 6 Global GameSwitcher / Session Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a durable, system-agnostic multi-session GameSwitcher that records only successful GameSwitcher handoffs, resumes through existing Session Adapter/recovery policy, retains valid sessions until explicit close, recovers interrupted sessions, and never depends on Game Boy-specific persistence behavior.

**Architecture:** Add a focused Session Manager beneath the frontend and above existing session/process metadata. Use one atomic metadata file per retained session under the existing SD-card sessions root so one damaged entry cannot destroy unrelated sessions. Keep Save Manager, State Manager and Phase 5C Recovery policy authoritative for game data and resume safety. Keep the existing current-process session record as crash/interruption evidence, but stop using Recent/current-session data as the GameSwitcher source of truth. Preserve exit code 20 during migration and centralize its translation to semantic session actions. Crucially, do not record every exit-20 result: the existing frontend lid check must first distinguish a real GameSwitcher yield from lid-suspend auto-resume.

**Tech Stack:** C11 host tests, Python architecture tests, AArch64 static production build, existing FlipOS Lifecycle Host, Session Adapter, Session process metadata, Save Manager, State Manager, Recovery policy, DRM/Input frontend, FAT32/vfat SD storage.

**Spec:** `docs/superpowers/specs/2026-08-27-phase6-gameswitcher-session-manager-design.md`

## Global Constraints

- SD-card-only development; no internal NAND/MTD writes.
- `main` remains the hardware-tested Phase 5C baseline until Phase 6 passes its full hardware gate.
- GameSwitcher/Session Manager must contain no Gambatte, GB save-memory, GB state-path or Pokémon-specific behavior.
- Library, Recent and retained GameSwitcher Sessions remain distinct data models.
- Valid retained sessions have no automatic count/age eviction; they remain until explicit close.
- Closing/pruning a session removes only Session Manager metadata. Never delete `.sav`, `.state`, `.prev`, `.recovery`, or `.restore-pending` data.
- Phase 5C resume blocking remains authoritative. A physical state file is not sufficient to claim Resume availability.
- Save Manager remains authoritative for cartridge progress; State Manager remains authoritative for serialized emulator state.
- A retained session may be created/updated only after a successful controlled GameSwitcher persistence handoff.
- Preserve START+SELECT exit semantics and lid-close/deep-resume semantics.
- Preserve exit code `20` compatibility until the semantic boundary is proven; do not invent a new hardware protocol in this phase.
- Exit `20` currently represents both saved lid-suspend yield and saved GameSwitcher yield. The frontend's proven hall/lid classification remains required to distinguish them after process exit.
- A lid-close yield must never create a retained GameSwitcher session; after lid-open it must still auto-resume directly without showing Game Options.
- Use staged file write, `fflush()`, `fsync()`, close and rename for retained-session metadata. FAT32/vfat does not provide journaled durability.
- Use dynamic in-memory lists; do not introduce an arbitrary `MAX_SESSIONS` product limit.
- Preserve the six existing untracked diagnostic files throughout development.

---

## Phase 6A — Persistent Session Registry

### Task 1: Add the generic retained-session storage model

**Files:**
- Create: `include/flip_session_manager.h`
- Create: `src/runtime/flip_session_manager.c`
- Create: `tests/test_flip_session_manager.c`
- Modify: `Makefile`

**Storage decision:** one file per retained session:

```text
FLIP_SESSION_MANAGER_ROOT/<system>/<rom-hash>.session
```

Default `FLIP_SESSION_MANAGER_ROOT` is `/mnt/SDCARD/flipos-dev/sessions`, sharing the Phase 5C root but using a distinct `.session` suffix. Unit tests override it with `/tmp/flipos-session-manager-test`.

**File format v1:**

```text
version=1
system=<system-id>
rom=<exact-rom-path>
status=<suspended|interrupted>
last_active=<epoch>
```

**Initial public interface:**

```c
typedef enum {
    FLIP_RETAINED_SESSION_SUSPENDED = 0,
    FLIP_RETAINED_SESSION_INTERRUPTED = 1
} flip_retained_session_status_t;

typedef struct {
    char system[32];
    char rom[512];
    flip_retained_session_status_t status;
    long long last_active;
    int resume_available;
} flip_retained_session_t;

typedef struct {
    flip_retained_session_t *items;
    size_t count;
} flip_retained_session_list_t;

int flip_session_manager_record(
    const char *system_id,
    const char *rom_path,
    flip_retained_session_status_t status,
    long long last_active);

int flip_session_manager_load(
    flip_retained_session_list_t *list);

void flip_session_manager_list_free(
    flip_retained_session_list_t *list);

int flip_session_manager_remove(
    const char *system_id,
    const char *rom_path);
```

- [ ] Add RED path/identity test. The exact Pokémon Yellow path may be used as a generic fixture and must hash to the already-frozen `792f9628ba43b7d9` identity.
- [ ] Add RED empty-root test: load succeeds with `count == 0`.
- [ ] Add RED record/read test for one suspended session and exact metadata round-trip.
- [ ] Add RED update test: recording the same system+ROM replaces metadata atomically rather than creating a duplicate.
- [ ] Add RED multiple-session test with more than ten entries to prove no Recent-style fixed product limit was copied into Session Manager.
- [ ] Add RED ordering test requiring descending `last_active` order.
- [ ] Add RED remove test requiring exactly the requested `.session` metadata file to disappear.
- [ ] Add RED malformed-file isolation test: one damaged `.session` file must not prevent unrelated valid files from loading.
- [ ] Add RED forced staged-write/rename failure test proving an already-committed session remains readable.
- [ ] Add `test-session-manager` Makefile target using `-DFLIP_SESSION_MANAGER_ROOT='"/tmp/flipos-session-manager-test"'`.
- [ ] Run `make test-session-manager`; expected RED before production implementation.
- [ ] Implement the minimal generic storage layer, dynamic list allocation/free, parser, stable exact-ROM-path FNV-1a64 file identity, atomic writes and most-recent sort.
- [ ] Do not include `flip_save.h`, `flip_state.h`, Gambatte/libretro headers, or any save/state filename suffix in this module.
- [ ] Run `make test-session-manager`; expected GREEN.
- [ ] Commit `Add persistent GameSwitcher session registry`.

### Task 2: Add validation, pruning and adapter-aware resume availability

**Files:**
- Modify: `include/flip_session_manager.h`
- Modify: `src/runtime/flip_session_manager.c`
- Modify: `tests/test_flip_session_manager.c`
- Create: `tests/test_session_manager_architecture.py`
- Modify: `Makefile`

**Interfaces:**

```c
int flip_session_manager_prune(void);
int flip_session_manager_refresh(
    flip_retained_session_list_t *list);
```

`refresh` loads structurally valid retained sessions, resolves each system through `flip_system_find()`, validates ROM readability/support, and derives `resume_available` through `flip_system_resume_available()`.

- [ ] Add RED test for unsupported system metadata -> pruned.
- [ ] Add RED test for missing/unreadable ROM -> pruned.
- [ ] Add RED test for ROM path unsupported by registered system -> pruned.
- [ ] Add RED duplicate/collision-defense test: canonical same system+ROM appears once.
- [ ] Add RED test that missing resume availability does **not** prune an otherwise valid retained session; it remains listed with `resume_available == 0`.
- [ ] Add RED architecture test requiring resume availability to flow through `flip_system_resume_available()` rather than direct state-path inspection.
- [ ] Add architecture assertions forbidding `Gambatte`, `flip_gb`, `.sav`, `.state`, `.restore-pending`, `FLIP_SAVE_ROOT`, and `FLIP_STATE_ROOT` from Session Manager implementation.
- [ ] Run `make test-session-manager test-architecture`; expected RED.
- [ ] Implement validation/pruning using the generic system registry and adapter-facing resume API only.
- [ ] Treat directory/read errors conservatively: report error rather than bulk-deleting valid unknown entries.
- [ ] Run `make test-session-manager test-architecture`; expected GREEN.
- [ ] Commit `Validate retained GameSwitcher sessions`.

---

## Phase 6B — Lifecycle / Handoff Integration

### Task 3: Centralize exit-code compatibility into a generic semantic boundary

**Files:**
- Modify: `include/flip_session.h`
- Modify: `include/flip_session_adapter.h`
- Modify: `src/runtime/flip_session.c`
- Modify: `src/runtime/flip_session_adapter.c`
- Modify: `tests/test_flip_session_adapter_save_safety.c`
- Modify: `tests/test_session_adapter_architecture.py`
- Modify/Create: `tests/test_session_handoff_architecture.py`
- Modify: `Makefile`

**Protocol rule:** preserve the numeric compatibility value but name it generically in one place:

```c
#define FLIP_SESSION_EXIT_SAVED_HANDOFF 20

flip_session_action_t flip_session_action_from_exit_code(int exit_code);
```

The generic process/session layer should translate `20` into `FLIP_SESSION_ACTION_SUSPEND`. System adapters stop carrying duplicate `map_exit_code` implementations when all FlipOS-hosted cores share the same lifecycle process protocol.

- [ ] Add RED unit/architecture coverage for `0 -> EXIT`, `20 -> SUSPEND`, abnormal/signaled paths unchanged.
- [ ] Add RED assertion that literal `20` no longer appears in `src/runtime/flip_session_adapter.c` or frontend code.
- [ ] Add RED assertion that Game Boy/GBC adapters no longer need per-core exit-code mapping.
- [ ] Run focused adapter/architecture tests; expected RED.
- [ ] Implement the single generic compatibility mapping in session process policy while preserving observable action behavior.
- [ ] Keep `FLIP_LIFECYCLE_EXIT_SWITCHER` compatible or alias it to the shared protocol constant without changing emulator exit behavior.
- [ ] Run focused tests plus `make test-lifecycle`; expected GREEN.
- [ ] Commit `Centralize session handoff exit protocol`.

### Task 4: Record only true GameSwitcher handoffs, never lid suspend

**Files:**
- Modify: `src/frontend/flip_frontend.c`
- Modify: `include/flip_session_manager.h`
- Modify: `src/runtime/flip_session_manager.c`
- Create/Modify: `tests/test_gameswitcher_handoff_architecture.py`
- Modify: `Makefile`

**Critical existing behavior:** after a saved handoff, `launch_game()` checks the hall sensor. Lid closed -> wait for lid open -> one-shot `automatic_resume` relaunch. Lid open -> this was a GameSwitcher request. Keep that classification.

**Interface helper:**

```c
int flip_session_manager_record_gameswitcher(
    const flip_system_t *system,
    const char *rom_path);
```

This stamps `status=suspended`, current epoch and calls the generic record operation.

- [ ] Add RED architecture fixture proving the Session Manager record call occurs only on the lid-open/GameSwitcher branch after a successful `FLIP_SESSION_ACTION_SUSPEND` result.
- [ ] Add RED assertion that the lid-closed -> `wait_for_lid_open()` -> `automatic_resume = 1` path contains no retained-session record call.
- [ ] Add RED assertion that normal exit (`0`/EXIT) creates no retained session.
- [ ] Add RED assertion that failed persistence never reaches a saved handoff and therefore cannot create a retained session.
- [ ] Run the new architecture test; expected RED.
- [ ] Implement the narrow post-classification record call without moving persistence into frontend/Session Manager.
- [ ] On registry-write failure, log `FLIP_SESSION_MANAGER: RECORD_FAILED` and return safely to frontend; do not alter already-committed `.sav`/`.state`.
- [ ] Preserve existing `FRONTEND_STANDBY auto resume` and `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME` paths unchanged.
- [ ] Run `make test-session-manager test-lifecycle test-architecture frontend`; expected GREEN.
- [ ] Commit `Record successful GameSwitcher handoffs`.

---

## Phase 6C — Generic GameSwitcher UI / Switching

### Task 5: Add a focused GameSwitcher UI/model separate from Recent

**Files:**
- Create: `include/flip_gameswitcher_ui.h`
- Create: `src/frontend/flip_gameswitcher_ui.c`
- Create: `tests/test_gameswitcher_ui_model.c` or equivalent pure-model test
- Create/Modify: `tests/test_gameswitcher_architecture.py`
- Modify: `src/frontend/flip_frontend.c`
- Modify: `Makefile`

**Public UI result:**

```c
typedef enum {
    FLIP_GAMESWITCHER_BACK = 0,
    FLIP_GAMESWITCHER_NEW_GAME,
    FLIP_GAMESWITCHER_RESUME,
    FLIP_GAMESWITCHER_CLOSE,
    FLIP_GAMESWITCHER_ERROR
} flip_gameswitcher_action_t;

typedef struct {
    flip_gameswitcher_action_t action;
    char system[32];
    char rom[512];
} flip_gameswitcher_result_t;

int flip_gameswitcher_run(
    flip_drm_t *drm,
    flip_gameswitcher_result_t *result);
```

The UI queries `flip_session_manager_refresh()` and never opens Session Manager files itself.

- [ ] Add RED model/UI tests for empty list -> `+ NEW GAME` remains available.
- [ ] Add RED tests for multiple retained sessions shown in most-recent order.
- [ ] Add RED architecture assertions that `flip_frontend.c` no longer uses `recent_roms`/Recent history as the GameSwitcher data source.
- [ ] Add RED architecture assertion that `flip_gameswitcher_ui.c` contains no registry path, FNV hash, `.sav`, `.state`, or recovery marker strings.
- [ ] Add RED behavior/model test: resumable session offers `RESUME` and `CLOSE SESSION`.
- [ ] Add RED behavior/model test: non-resumable session does not falsely offer direct Resume; Close and New Game remain available.
- [ ] Add RED test that `NEW GAME` returns to the normal library flow rather than mutating session registry.
- [ ] Run focused UI/architecture tests; expected RED.
- [ ] Implement compact 640x480 GameSwitcher UI in the new focused file using existing DRM/draw/input patterns.
- [ ] Replace the old singleton `suspended_rom_path` GameSwitcher presentation path with Session Manager-backed UI while retaining Recent screen/history as a separate feature.
- [ ] Run focused tests and `make frontend test-architecture`; expected GREEN.
- [ ] Commit `Add persistent multi-session GameSwitcher UI`.

### Task 6: Resume selected sessions through generic system launch paths

**Files:**
- Modify: `src/frontend/flip_frontend.c`
- Modify: `src/frontend/flip_gameswitcher_ui.c`
- Modify: `include/flip_session_manager.h`
- Modify: `src/runtime/flip_session_manager.c`
- Modify/Create: `tests/test_gameswitcher_architecture.py`

**Interface:**

```c
int flip_session_manager_touch(
    const char *system_id,
    const char *rom_path);
```

- [ ] Add RED architecture test that GameSwitcher resume resolves system via `flip_system_find()` and launches through the same normal frontend/session path used by library games.
- [ ] Add RED assertion that GameSwitcher does not construct emulator argv or call `flip-gb` directly.
- [ ] Add RED assertion that `resume_available == 0` blocks the Resume action before launch.
- [ ] Add RED test that selecting Resume updates `last_active` only when the selection is accepted, preserving most-recent ordering.
- [ ] Run focused tests; expected RED.
- [ ] Implement resume delegation by constructing/resolving a normal `rom_entry_t` (or a small shared equivalent) from system registry metadata and calling the established launch path.
- [ ] Do not add a GameSwitcher-specific restore implementation; Save/State/Recovery behavior must remain the normal launcher behavior.
- [ ] Run focused tests + `make verify`; expected GREEN.
- [ ] Commit `Resume retained sessions through generic launcher`.

---

## Phase 6D — Explicit Close / Remove

### Task 7: Make Close Session metadata-only and explicit

**Files:**
- Modify: `src/frontend/flip_gameswitcher_ui.c`
- Modify: `src/runtime/flip_session_manager.c`
- Modify: `tests/test_flip_session_manager.c`
- Modify: `tests/test_gameswitcher_architecture.py`

- [ ] Add RED confirmation-flow test/architecture check requiring explicit player confirmation before Close Session mutation.
- [ ] Add RED Session Manager test that close/remove unlinks only the target `.session` metadata file.
- [ ] Add RED architecture checks forbidding Save Manager, State Manager or Recovery deletion APIs from the close path.
- [ ] Add RED test that another retained session remains intact after closing one.
- [ ] Add RED test that re-recording the same game later creates a fresh retained session normally.
- [ ] Run focused tests; expected RED.
- [ ] Implement close confirmation and refresh list after successful metadata removal.
- [ ] Keep old `.sav`, `.state`, `.prev`, `.recovery` and restore marker data untouched by construction.
- [ ] Run focused tests + `make verify`; expected GREEN.
- [ ] Commit `Add metadata-only GameSwitcher close action`.

---

## Phase 6E — Restart / Interrupted Recovery

### Task 8: Reconcile stale current-process metadata into retained sessions

**Files:**
- Modify: `include/flip_session_manager.h`
- Modify: `src/runtime/flip_session_manager.c`
- Modify: `src/runtime/flip_session.c` only if a small current-session read/recovery API correction is needed
- Modify: `src/frontend/flip_frontend.c`
- Create/Modify: `tests/test_session_manager_recovery.c`
- Modify/Create: `tests/test_gameswitcher_architecture.py`
- Modify: `Makefile`

**Interface:**

```c
int flip_session_manager_recover_interrupted(void);
```

Algorithm:
1. call existing `flip_session_recover_current()`;
2. read `flip_session_read_current()`;
3. only reconcile metadata that is structurally valid and now `state=interrupted`;
4. validate system/ROM generically;
5. record/update retained status `interrupted`;
6. let `flip_system_resume_available()` decide whether UI can Resume it.

- [ ] Add RED test for stale running current-session -> interrupted classification -> retained interrupted entry.
- [ ] Add RED test for malformed current-session -> no fabricated retained entry.
- [ ] Add RED test for unsupported/missing ROM -> no retained entry.
- [ ] Add RED test proving already-committed save/state files are not modified by recovery.
- [ ] Add RED test for interrupted entry with resume unavailable: retained metadata remains visible but direct Resume remains false.
- [ ] Add RED idempotence test: repeated startup recovery does not duplicate a retained session.
- [ ] Run focused tests; expected RED.
- [ ] Implement recovery/reconciliation and invoke it during frontend startup before GameSwitcher list display.
- [ ] Keep the single current-process record as diagnostic/crash evidence; do not make it the retained-session source of truth during normal operation.
- [ ] Run focused tests + `make verify`; expected GREEN.
- [ ] Commit `Recover interrupted GameSwitcher sessions`.

### Task 9: Finish separation of Recent and GameSwitcher session data

**Files:**
- Modify: `src/frontend/flip_frontend.c`
- Modify: `include/flip_session.h` comments/API names only where needed
- Modify: `src/runtime/flip_session.c` only for narrow naming/comment cleanup
- Modify/Create: `tests/test_gameswitcher_architecture.py`

- [ ] Add RED architecture assertions that GameSwitcher does not parse `/state/recent/history` and does not depend on `flip_session_prune_recent()` or `flip_session_remove_recent()`.
- [ ] Preserve the Recent screen as recently-played history; do not remove it merely because GameSwitcher no longer uses it.
- [ ] Remove/rename misleading comments that describe Recent as GameSwitcher session storage.
- [ ] Run architecture tests; expected RED before cleanup.
- [ ] Apply the smallest separation cleanup needed; do not refactor Recent into a new service in this phase unless required for correctness.
- [ ] Run `make verify`; expected GREEN.
- [ ] Commit `Separate Recent history from GameSwitcher sessions`.

---

## Phase 6F — Full Software and Hardware Gate

### Task 10: Full software candidate and roadmap software-ready state

**Files:**
- Modify: `Makefile`
- Modify: `tests/test_verify_target_architecture.py`
- Modify: `docs/ROADMAP.md` only to record software-ready status before hardware acceptance

- [ ] Ensure `RUNTIME_OBJS` includes `flip_session_manager.o` and frontend includes `flip_gameswitcher_ui.o`.
- [ ] Ensure `make verify` includes `test-session-manager`, interrupted-recovery tests and new GameSwitcher architecture/model tests.
- [ ] Update `tests/test_verify_target_architecture.py` so future changes cannot silently drop Phase 6 gates.
- [ ] Run `make clean && make verify`; require exact compact `VERIFY: PASS`.
- [ ] Run `git diff --check`.
- [ ] Confirm `git status --short` contains only the six preserved diagnostic files after commits.
- [ ] Record exact candidate HEAD plus `sha256sum build/flip-gb build/flip-frontend`.
- [ ] Update roadmap to mark Phase 6 software candidate only; do not mark real-hardware acceptance yet.
- [ ] Commit `Prepare Phase 6 hardware candidate`.

### Task 11: Real-Miyoo-Flip multi-session hardware acceptance

**Files:**
- Create hardware evidence doc under `docs/superpowers/specs/` after testing
- Modify: `docs/ROADMAP.md` only after all acceptance gates pass

**Pre-test:** atomically deploy exact candidate `flip-gb` and `flip-frontend`, verify device SHA equals local SHA, then `adb reboot` so the fresh frontend executable is definitely running.

- [ ] Use Pokémon Yellow plus a second installed GB/GBC title; do not require a particular second ROM in code or tests.
- [ ] Launch Pokémon, request GameSwitcher with MENU, and prove cartridge + state persistence succeeds before emulator exits.
- [ ] Prove Pokémon appears in retained GameSwitcher Sessions.
- [ ] Choose `NEW GAME`, launch the second game from normal library flow, request GameSwitcher, and prove both sessions remain listed.
- [ ] Confirm ordering reflects most recently active session.
- [ ] Resume Pokémon from GameSwitcher and visually verify its exact previous state/progress.
- [ ] GameSwitcher out of Pokémon, resume the second game, and visually verify its exact prior state/progress.
- [ ] Reboot FlipOS and prove both valid sessions remain in GameSwitcher with no automatic eviction.
- [ ] Resume both after reboot through normal generic launch paths.
- [ ] Close one retained session with confirmation; prove it disappears from GameSwitcher while the ROM's `.sav`/`.state` hashes/files remain unchanged by the close action itself.
- [ ] Exercise safe interrupted-session recovery (e.g. controlled stale current-process metadata fixture when no emulator is live) and prove retained-session reconciliation without save/state deletion.
- [ ] Exercise a Phase 5C `restore-pending` scenario or controlled fixture and prove GameSwitcher does not falsely offer direct Resume while blocked.
- [ ] Verify START+SELECT remains normal exit/save behavior and does not create a retained session.
- [ ] Verify lid close -> deep suspend -> lid open resumes directly to the game with **no GameSwitcher and no Game Options menu**.
- [ ] Capture `FRONTEND_STANDBY auto resume`, `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME`, and `FLIP_STATE: RESTORE_CURRENT_OK` on lid wake.
- [ ] Verify RK817 exact broken signature recovery remains correct when encountered: `17=06 38=00 40=69` only.
- [ ] Verify audio still reports Gambatte source 32768 Hz -> RK817 output 32000 Hz and is audibly synchronized after resume.
- [ ] Verify frame pacing remains around the established 59.728 FPS target with no resync storm or renewed audio backlog.
- [ ] Record exact candidate HEAD, binary SHAs, test ROM paths and compact hardware logs in a Phase 6 hardware acceptance document.
- [ ] Mark all Phase 6 roadmap responsibilities complete only after hardware evidence is committed.
- [ ] Run fresh `make verify` once more after documentation-only closeout.
- [ ] Commit `Complete Phase 6 hardware acceptance`.

## Required Phase 6 Acceptance Mapping

The final implementation must prove every stated Phase 6 responsibility explicitly:

- **suspend current game** -> existing shared lifecycle request/save path remains successful.
- **save state** -> State Manager/Core Host persistence completes before GameSwitcher handoff.
- **record active session** -> true GameSwitcher handoff records/updates durable generic metadata.
- **switch games** -> retained A -> library/new B -> retained B -> A/B switching works.
- **resume previous game** -> generic system/session adapter path resumes a selected retained session.
- **remove/close sessions** -> metadata-only explicit close.
- **prune stale sessions** -> malformed/unsupported/missing-ROM metadata removed without touching game data.
- **recover interrupted sessions** -> stale current-process metadata reconciled safely.
- **coordinate emulator lifecycle** -> Session Manager consumes semantic handoff policy while persistence remains in shared hosts.
- **exit-code cleanup** -> raw `20` compatibility is centralized and no longer spread through per-system/frontend behavior.
- **Game Boy independence** -> architecture tests forbid GB/Gambatte persistence assumptions in Session Manager/GameSwitcher.

## Plan self-check criteria

Before execution starts, verify:

- no unresolved `TBD`/`TODO` placeholders;
- every spec milestone 6A-6F maps to at least one task above;
- no public type is defined differently in two tasks;
- no step requires Session Manager to parse/save emulator bytes;
- no step records a retained session solely from raw exit code 20 without lid/GameSwitcher classification;
- no close/prune/recovery step deletes player save/state data;
- every production-code task has an observable RED step before implementation and a GREEN command after it.
