// Phase 2.0 Skia GPU spike. Just acquiring the 2D context flips the runtime
// into canvas mode, which (in the NX_SKIA_GPU_SPIKE build) brings up EGL +
// Skia Ganesh GL and draws a native test scene each frame. The JS draw calls
// here are ignored by the spike present path; this only drives canvas mode +
// the frame loop.
const ctx = screen.getContext('2d');
function frame() {
	ctx.fillStyle = 'white';
	ctx.fillRect(0, 0, 10, 10);
	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
