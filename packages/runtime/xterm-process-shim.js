// esbuild `inject` shim (see bundle.mjs).
//
// `@xterm/headless` performs platform detection at module-evaluation time with:
//
//     const isNode = typeof process !== 'undefined' && 'title' in process;
//     const ua = isNode ? 'node' : navigator.userAgent;   // + navigator.platform
//
// In nx.js there is no `process` global, so xterm would take the browser branch
// and read `navigator.userAgent` *during runtime startup* — before the runtime
// globals it transitively touches (`Application.self` → ns init →
// `addEventListener`, `$.argv`) are ready. That made runtime initialization
// throw on-device and on the host test binary.
//
// Injecting this module-scoped `process` binding makes xterm's `isNode` true, so
// it uses the constant string `'node'` and never reads `navigator`. The binding
// is injected into every bundled module that references `process` as a free
// variable — that is NOT only xterm: `kleur/colors` (used by `@nx.js/inspect`
// and `console.ts` for ANSI coloring) also reads `process` at eval time:
//
//     let isTTY = true;
//     if (typeof process !== 'undefined') {
//         ({ FORCE_COLOR, NODE_DISABLE_COLORS, NO_COLOR, TERM } = process.env || {});
//         isTTY = process.stdout && process.stdout.isTTY;
//     }
//     enabled: ... && (FORCE_COLOR != null && FORCE_COLOR !== '0' || isTTY)
//
// So the shim MUST also keep kleur's color auto-detection enabled, otherwise an
// inspected object's keys/values lose their ANSI colors. We do that by exposing
// `env: {}` and a TTY-like `stdout` (`isTTY: true`). This binding is module
// scoped (injected only where `process` is referenced), NOT a real global, so
// user code's own Node feature detection is unaffected.
export const process = {
	title: 'nxjs',
	env: {},
	stdout: { isTTY: true },
};
