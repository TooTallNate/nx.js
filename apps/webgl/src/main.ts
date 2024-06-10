const gl = screen.getContext('webgl');

gl.clearColor(0.8, 0.2, 0.6, 1.0);

function render()
{
	gl.clear(16384);
	window.requestAnimationFrame(render);
}

window.requestAnimationFrame(render);
