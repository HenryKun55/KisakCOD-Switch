# Deferred risks and skipped work

Things the port is intentionally papering over, or files we tried to bring
in but had to defer. Each entry documents **what** was skipped, **why**,
and **when** it must be addressed.

Living document — append when something gets deferred; remove when it gets
fixed.

---

## 🔴 Will break before the game can run

### 1. 64-bit struct layout drift

**What:** The upstream binary is 32-bit. Many `static_assert(sizeof(X) == N)`
checks hard-code layouts that contain pointers (`void*`, `char*`, function
pointers). On a 64-bit port (macOS arm64, Switch arm64, Linux x86_64) those
pointers expand from 4 to 8 bytes and the struct grows. We guard these
asserts with `UINTPTR_MAX == 0xFFFFFFFFu` so the build continues.

**Why deferred:** Fixing requires either reformatting structs with
`#pragma pack` + placeholder slots, or implementing a pointer-translation
layer in the asset loader. Both are multi-day efforts that block all
incremental progress if attempted now.

**When this bites:** The moment we read serialized assets:
- `.d3dbsp` (BSP map files)
- `.ff` (FastFile asset bundles)
- `.iwd` (IWD asset packs — ZIP containers of binary blobs)
- XModel / XAnim / ImagePack

**Sites currently guarded** (grep `UINTPTR_MAX == 0xFFFFFFFFu` in repo):
- `src/universal/q_shared.h` — `StringTable`
- `src/qcommon/qcommon.h` — `SpawnVar`
- `src/qcommon/msg_mp.h` — `usercmd_s`
- `src/game/enthandle.h` — `EntHandleInfo`
- `src/gfx_d3d/fxprimitives.h` — `FxEffectDef`, `FxImpactTable`
- `src/bgame/bg_local.h` — `scr_anim_s`

**Recommended fix:** loader-side translation. Reads 32-bit on-disk offsets,
dereferences to 64-bit pointers, rewrites in place. Preserves on-disk asset
format compatibility — needed anyway to read CoD4 assets shipped on Steam.

### 2. `Com_Error` aborts instead of `longjmp`

**What:** `posix_stubs.cpp::Com_Error` calls `std::abort()`. Upstream uses
`ERR_DROP` to drop to console with a message and keep the engine alive.

**Why deferred:** Real error recovery requires `setjmp` in the main loop
and `longjmp` from `Com_Error`. That belongs in `qcommon/common.cpp`,
which we haven't ported yet — porting `common.cpp` is a multi-file effort
(it pulls cmd, cvar, filesystem, all heavily entangled).

**When this bites:** Any non-fatal error during engine init kills the
binary instead of printing a useful diagnostic. We'll see "abort" instead
of "ERROR: file X not found".

**Recommended fix:** lands with `qcommon/common.cpp` port.

### 3. `Sys_GetValue` returns null

**What:** `posix_stubs.cpp::Sys_GetValue(int valueIndex)` always returns
`nullptr`. Upstream uses this as a TLS slot getter.

**Why deferred:** Real implementation needs `pthread_key_create` /
`pthread_setspecific` / `pthread_getspecific` (POSIX) or libnx equivalents
(Switch), and tracking of slot indices. Belongs in `qcommon/threads.cpp`.

**When this bites:** Subsystems that use TLS (parse session context per
thread, render thread context). Currently we're single-threaded so we
don't hit it; render thread or worker threads will null-deref.

**Recommended fix:** lands with `qcommon/threads.cpp` port.

### 4. `AxisToQuat` returns identity quaternion

**What:** `posix_stubs.cpp::AxisToQuat` ignores the input matrix and
returns `{0,0,0,1}` (identity). Prints a `[stub]` warning on first call.

**Why deferred:** Real impl is in `com_math.cpp`, which doesn't compile yet
(pulls `<ode/ode.h>` via `xanim/dobj.h`). ODE port is a separate effort.

**When this bites:** Bone/ragdoll/animation rotations from axis-vector
form will all be wrong. Models will appear in default pose.

**Recommended fix:** lands with `com_math.cpp` port.

---

## 🟡 Likely to bite, manageable

### 5. `-Wno-sign-compare`

**What:** POSIX and Switch builds suppress `-Wsign-compare`.

**Why deferred:** Code is reverse-engineered from IDA; hex-rays rarely
propagates signedness. Dozens of cosmetic `int` vs `unsigned int`
comparisons get warned without indicating real bugs.

**When this bites:** Real signedness bugs in code paths we run could hide.
Mostly low risk because the upstream binary works on Windows with
signed/unsigned implicit conversions.

**Recommended fix:** re-enable per subsystem during a signedness audit
after the engine boots.

### 6. `volatile struct ProfileReadable` → `struct ProfileReadable`

**What:** Dropped `volatile` qualifier from the struct definition in
`src/universal/profile.h` (GCC rejects `volatile struct X { ... };`).

**Why deferred:** Clang/MSVC accept the upstream form; GCC doesn't. The
quick fix preserves single-threaded behavior.

**When this bites:** If profiling becomes multi-threaded with shared
`ProfileReadable` objects, the compiler could reorder reads/writes.

**Recommended fix:** declare individual instances `volatile` at the use
site when threaded profiling lands.

### 7. `va_copy(ap, va); ap = 0;` removed in `q_parse.cpp`

**What:** Two pairs of dead lines in `Com_ScriptError` /
`Com_ScriptErrorDrop` (`ap` was declared `char*` matching MSVC `va_list`,
never read).

**Why deferred:** The lines were truly dead — the compiler would optimize
them away regardless. Removed only because GCC fails to type-check
`va_copy(char*, struct va_list)`.

**When this bites:** Should not bite. Listed for transparency.

---

## 🟢 Low risk

### 8. 32-bit pointer cast in `pool_allocator.cpp`

**What:** `(unsigned int)&pool[itemSize * (itemIndex + 1)]` truncates a
64-bit pointer to 32 bits.

**Why deferred:** `pool_allocator.cpp` is not yet in the build. We don't
hit the bug.

**When this bites:** Only if a pool's backing buffer lives above the 4 GB
mark, which is unlikely for a 2007 game's data structures.

**Recommended fix:** when we bring `pool_allocator.cpp` in, change the
cast to `(uintptr_t)`.

### 9. Win32 type stubs in `kisak_compat.h`

**What:** `LRESULT`, `WPARAM`, `LPARAM`, `OSVERSIONINFO`,
`CRITICAL_SECTION`, etc. are declared so headers parse on non-Windows.

**Why:** Headers like `win_local.h` declare functions that take these
types. Without the typedefs, the headers wouldn't parse.

**When this bites:** Functions that *take* these types are never called
on POSIX — the call sites live in Win32-only code paths. Inert.

---

## Files attempted but currently deferred

Each row: we tried to compile this file into the POSIX build, hit a
blocker we chose not to resolve immediately, deferred to a focused
session.

| File | Blocker | Reason for deferring | Unblocks |
|---|---|---|---|
| `src/universal/com_memory.cpp` | `<zlib/zlib.h>` via `database.h` (header chain pulls `xanim`, `d3d9.h`) | zlib wiring is straightforward but `database.h` then pulls `r_gfx.h` which needs the full renderer. Memory allocator alone needs many more shims. | central memory tracking |
| `src/universal/com_files.cpp` | Same `database.h` chain → `d3d9.h` | File I/O entangled with asset DB headers; needs renderer stubs first. | filesystem |
| `src/universal/com_loadutils.cpp` | `<zlib/zlib.h>` not in include path | Simplest fix; could land soon by adding `-I deps` and stubbing zlib symbols. | asset load utilities |
| `src/universal/com_stringtable.cpp` | Same as above | Same fix path. | string table parser |
| `src/universal/com_math.cpp` | `<ode/ode.h>` via `xanim/dobj.h` | ODE physics not yet built for POSIX/Switch (deps/ode/ has the source but isn't compiled). | most math helpers; would let us remove `AxisToQuat`, `ClearBounds`, `ExpandBounds`, `Vec2Normalize` stubs. |
| `src/universal/q_shared.cpp` | `<d3d9.h>` via `gfx_d3d/r_model.h` | Pulls renderer code that depends on DX9. Needs renderer abstraction first. | core string utilities; would let us remove `va`, `I_stricmp`, `I_strnicmp`, `I_strncpyz`, `I_stristr` stubs. |
| `src/universal/fft.cpp` | `<d3d9.h>` via `fft.h → r_material.h` | Same renderer entanglement. | audio FFT processing. |
| `src/universal/com_sndalias.cpp` | `<msslib/mss.h>` (Miles Sound System) | Miles is proprietary; needs OpenAL-soft replacement layer. | sound alias system. |
| `src/universal/dvar.cpp` | `<Windows.h>` + win32/win_local + gfx_d3d/r_dvars + win32/win_net + devgui | Cvar system touches everything. Centralized; tackle after common.cpp / cmd.cpp port. | **the cvar registration / lookup system — central to engine init.** |
| `src/universal/physicalmemory.cpp` | `VirtualAlloc` (Win32 page allocator) | Needs POSIX `mmap` / Switch `svcMapMemory` replacement. Limited usage so not urgent. | physical memory pool. |
| `src/universal/memfile.cpp` | `<zlib/zlib.h>` and qcommon/threads.h | zlib + threads. Defer with the zlib cluster. | in-memory file abstraction. |
| `src/universal/dvar_cmds.cpp` | `static_assert(sizeof(scr_anim_s) == 4)` (guarded but file not yet in build) | Static_assert now guarded; remaining blockers TBD on next attempt. | dvar registration commands. |

---

## Internal helper functions stubbed

Living in `src/posix/posix_stubs.cpp`. Each is provisional; should be
removed when the owning upstream file ports cleanly.

| Stub | Real owner | Behavior gap |
|---|---|---|
| `Com_Printf(channel, ...)` | `qcommon/common.cpp` | Ignores channel; everything to stdout. No log channel filtering. |
| `Com_PrintError(channel, ...)` | `qcommon/common.cpp` | Same as above, prefixed `[error]`, to stderr. |
| `Com_Error(code, ...)` | `qcommon/common.cpp` | Aborts. No `ERR_DROP` recovery. **High impact** — see entry 2. |
| `Sys_IsMainThread()` | `qcommon/threads.cpp` | Returns true unconditionally. Safe single-threaded. |
| `Sys_IsRenderThread()` | `qcommon/threads.cpp` | Returns false. Misleading once render thread spins up. |
| `Sys_IsDatabaseThread()` | `qcommon/threads.cpp` | Returns false. Same. |
| `Sys_GetValue(slot)` | `qcommon/threads.cpp` | Returns null. **High impact** — see entry 3. |
| `MyAssertHandler` | `universal/assertive.cpp` | Prints + aborts. Loses upstream's clipboard / dialog UX, but that's Win32-only anyway. |
| `_copyDWord` | `qcommon/common.cpp` (inline asm) | Plain loop, auto-vectorizes to NEON. Functionally identical. |
| `I_stricmp / I_strnicmp / I_strncpyz / I_stristr` | `universal/q_shared.cpp` | Correct portable impls. No semantic gap. |
| `va` | `universal/q_shared.cpp` | 8-slot rotating buffer (upstream is 32-slot). Functional, but call sites that chain >8 `va()` calls without consuming may get earlier slot overwritten. |
| `Vec2Normalize` | `universal/com_math.cpp` | Correct portable impl. No semantic gap. |
| `ClearBounds / ExpandBounds` | `universal/com_math.cpp` | Correct portable impls. No semantic gap. |
| `AxisToQuat` | `universal/com_math.cpp` | **Identity quaternion** — wrong result. See entry 4. |
| `QueryPerformanceCounter / QueryPerformanceFrequency` | Native Win32 | `std::chrono::steady_clock`-backed; resolution is ns. Functionally equivalent. |
| `_time64 / _localtime64` (in `kisak_compat.h`) | Win32 CRT | POSIX `time` / `localtime` bridges. Functionally equivalent for years 1970-2038+ on 64-bit time_t. |
