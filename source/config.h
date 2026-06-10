#pragma once

// nx.js application configuration (`nxjs.ini`).
//
// An optional INI file located next to the entrypoint (e.g. `romfs:/nxjs.ini`
// for a standalone/bootstrap app, or `sdmc:/switch/app.ini`-style next to a
// loose `.js`). It lets an app override runtime knobs that otherwise default
// from the memory regime:
//
//   [v8]
//   jit   = auto            ; auto (regime-based) | on | off
//   flags = --expose-gc     ; appended after the runtime's default V8 flags
//
//   [memory]
//   heap_limit = 256MiB     ; KiB/MiB/GiB suffix or raw bytes; clamped to fit
//
//   [renderer]
//   mode = auto             ; auto | cpu | gpu
//   gpu_cache = auto        ; GPU texture cache budget (MiB): auto | default |
//                           ;   <number>. auto = 512 in full-memory mode, Skia
//                           ;   default (~96) in applet mode. default = always
//                           ;   Skia default. A number sets an explicit cap.
//
//   [console]               ; on-screen console / terminal styling
//   font_size      = 22
//   line_height    = 1.25
//   scrollback     = 1000
//   cursor_style   = bar    ; block | underline | bar
//   cursor_opacity = 0.5
//   background     = #002b36
//   foreground     = #839496
//   cursor         = #93a1a1
//   black = #073642   red = #dc322f   green = #859900   ...  white = #eee8d5
//   bright_black = #586e75  ...  bright_white = #fdf6e3
//
//   [socket]                ; overrides on the regime-selected SocketInitConfig
//   tcp_tx_buf_size     = 256KiB
//   tcp_rx_buf_size     = 256KiB
//   tcp_tx_buf_max_size = 1MiB
//   tcp_rx_buf_max_size = 1MiB
//   udp_tx_buf_size     = 9KiB
//   udp_rx_buf_size     = 42KiB
//   sb_efficiency       = 6
//   num_bsd_sessions    = 3
//   service_type        = auto   ; auto | user | system
//
//   [runtime]                    ; consumed by the slim bootstrap LAUNCHER, not
//   version = ^1                 ; the runtime; recognized+ignored here.
//
// Parsing happens VERY early in main() (before V8::Initialize), using plain
// fopen via the bundled inih parser, because the V8 flag/heap settings must be
// applied before the isolate is created. The JS `fetch`/`readFileSync` layer
// does not exist that early.
//
// Whenever a requested value cannot be honored (clamped, invalid, or an init
// fell back), the runtime logs a `[config] ... not honored: <reason>` line to
// stderr (which is redirected to `sdmc:/switch/nxjs-debug.log`).

#include <stdbool.h>
#include <stdint.h>
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NX_JIT_AUTO = 0, // regime-based (today's behavior): JIT off in applet mode
	NX_JIT_ON,
	NX_JIT_OFF,
} nx_jit_mode_t;

// Sentinel for nx_config_t::code_headroom_mb meaning "unset — pick a regime
// default" (application: 64 MiB WASM headroom; applet: 0). UINT32_MAX so an
// explicit `code_headroom_mb = 0` (disable) is distinguishable from unset.
#define NX_CODE_HEADROOM_AUTO 0xFFFFFFFFu

// Sentinel for nx_config_t::gpu_cache_mib meaning "unset — pick a regime
// default" (application: 512 MiB; applet: 0 = Skia's own ~96 MiB default).
// UINT32_MAX so an explicit `gpu_cache = 0` (force Skia default) is
// distinguishable from unset.
#define NX_GPU_CACHE_AUTO 0xFFFFFFFFu

typedef enum {
	NX_RENDER_AUTO = 0, // regime-based: raster in applet, GPU (w/ fallback) in app
	NX_RENDER_CPU,      // force raster
	NX_RENDER_GPU,      // attempt GPU regardless of regime (falls back to raster)
} nx_render_mode_t;

// Console / on-screen terminal styling overrides (`[console]` section). The
// runtime does not consume these in C — they are exposed verbatim on
// `$.config.console` and the JS console layer seeds the global console's
// options from them (an explicit `console.options =` assignment overrides).
// Each string is strdup'd (or NULL when unset); numeric/enum fields use a
// `has_*` flag. Color strings are passed through as-is (e.g. "#002b36").
typedef enum {
	NX_CURSOR_UNSET = 0,
	NX_CURSOR_BLOCK,
	NX_CURSOR_UNDERLINE,
	NX_CURSOR_BAR,
} nx_cursor_style_t;

typedef struct {
	bool has_font_size;
	double font_size;
	bool has_line_height;
	double line_height;
	bool has_scrollback;
	uint32_t scrollback;
	nx_cursor_style_t cursor_style;
	bool has_cursor_opacity;
	double cursor_opacity;
	// Theme colors (NULL = unset). Index 0..15 are the ANSI palette
	// (black..brightWhite); the three named base colors are separate.
	char *background;
	char *foreground;
	char *cursor;
	char *ansi[16];
} nx_console_config_t;

// Socket overrides. Each field has a `has_*` flag; when false, the
// regime-selected default (lean/full) value is kept for that field.
typedef struct {
	bool has_tcp_tx_buf_size;
	uint32_t tcp_tx_buf_size;
	bool has_tcp_rx_buf_size;
	uint32_t tcp_rx_buf_size;
	bool has_tcp_tx_buf_max_size;
	uint32_t tcp_tx_buf_max_size;
	bool has_tcp_rx_buf_max_size;
	uint32_t tcp_rx_buf_max_size;
	bool has_udp_tx_buf_size;
	uint32_t udp_tx_buf_size;
	bool has_udp_rx_buf_size;
	uint32_t udp_rx_buf_size;
	bool has_sb_efficiency;
	uint32_t sb_efficiency;
	bool has_num_bsd_sessions;
	uint32_t num_bsd_sessions;
	bool has_service_type;
	BsdServiceType service_type;
} nx_socket_config_t;

typedef struct {
	nx_jit_mode_t jit;
	char *v8_flags;       // strdup'd app-provided flag string, or NULL
	uint64_t heap_limit;  // requested heap max in bytes; 0 = use computed default
	// Extra MiB of JIT code-arena space reserved for WebAssembly (WASM compiles
	// its own code region from the libnx jit_* arena, which is dual-mapped so
	// every MiB here costs ~2 MiB real). Sentinel NX_CODE_HEADROOM_AUTO means
	// "pick a regime default" (application: 64, applet: 0 — applet's ~137 MiB
	// can't afford the dual-mapped headroom, so WASM is opt-in there). A WASM
	// app in applet mode sets e.g. `code_headroom_mb = 64`. 0 disables WASM
	// code space (modules OOM at compile: "Allocate initial wasm code space").
	uint32_t code_headroom_mb;
	nx_render_mode_t renderer;
	// [renderer] gpu_cache: Ganesh GPU resource-cache budget in MiB. Skia's
	// own default is ~96 MiB; a texture-heavy app (many large atlases / baked
	// tilemap layers) whose per-frame working set exceeds that thrashes the
	// cache (LRU-evict + re-upload textures still needed next frame). Sentinel
	// NX_GPU_CACHE_AUTO = "unset": use 512 MiB in full-memory mode (plenty of
	// headroom) and Skia's default in applet mode (~137 MiB free — a big cache
	// would starve Mesa). An explicit value (incl. 0 = force Skia default)
	// overrides the regime default.
	uint32_t gpu_cache_mib;
	nx_socket_config_t socket;
	nx_console_config_t console; // [console] styling, exposed on $.config.console
	bool loaded;          // true if an nxjs.ini was found + parsed

	// Effective values, filled in by main() after clamping, for diagnostics
	// (exposed to JS via `$.config`). There is no effective_renderer field: the
	// requested mode lives in `renderer` above, and the actual GPU-vs-raster
	// outcome isn't known until the lazy framebuffer init (it's logged there).
	bool effective_jit;
	uint64_t effective_heap_limit; // bytes actually passed to V8
	uint32_t effective_code_headroom_mb; // WASM headroom actually applied (0 if !jit)
} nx_config_t;

// Initialize `cfg` to defaults (everything auto/unset).
void nx_config_defaults(nx_config_t *cfg);

// Parse the INI at `ini_path` into `cfg` (which should already hold defaults).
// A missing file is not an error (cfg keeps defaults, loaded=false). Invalid
// values are logged and left at their default. Returns true if a file was
// found and parsed (even with per-value errors), false if absent/unreadable.
bool nx_config_load(nx_config_t *cfg, const char *ini_path);

// Derive the `nxjs.ini` path that sits next to `entrypoint`. Returns a malloc'd
// string the caller must free, or NULL on allocation failure / null input.
// e.g. "romfs:/main.js" -> "romfs:/nxjs.ini"; "sdmc:/switch/app.js" ->
// "sdmc:/switch/nxjs.ini"; "romfs:/main.js" with no '/' after scheme handled.
char *nx_config_ini_path_for(const char *entrypoint);

// Apply the socket overrides in `cfg` on top of `base` (the regime-selected
// SocketInitConfig), clamping each field to a safe/valid range and logging any
// value that could not be honored. `tight_memory` selects the per-regime
// ceiling used for clamping the total buffer reservation.
void nx_config_apply_socket(SocketInitConfig *base, const nx_config_t *cfg,
                            bool tight_memory);

// Free any heap memory owned by `cfg` (the v8_flags string).
void nx_config_free(nx_config_t *cfg);

#ifdef __cplusplus
}
#endif
