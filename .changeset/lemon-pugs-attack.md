---
"@nx.js/runtime": minor
---

feat: WebGL2 rendering context for the screen canvas (`screen.getContext('webgl2')`), backed by a real OpenGL ES 3 context (EGL + Mesa/nouveau) on the Switch GPU. Implements the WebGL2 core API surface (shaders, buffers, VAOs, textures, framebuffers, uniforms, queries, samplers, sync objects, transform feedback, uniform buffers, instanced drawing), the standard `WebGL*` object classes, and the full WebGL enum constant set. The `"webgl"` (WebGL 1) context id and WebGL extensions are not implemented.
