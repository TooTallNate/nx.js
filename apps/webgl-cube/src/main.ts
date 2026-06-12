/**
 * WebGL2 example: a spinning colored cube over a textured (checkerboard)
 * floor, rendered with `screen.getContext('webgl2')` — a real OpenGL ES 3
 * context on the Switch GPU.
 */

const gl = screen.getContext('webgl2');
if (!gl) {
	throw new Error('Failed to create WebGL2 context');
}

console.log('%s', gl.getParameter(gl.VERSION));
console.log('Renderer: %s', gl.getParameter(gl.RENDERER));

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

const vsSrc = `#version 300 es
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
out vec3 v_color;
out vec2 v_uv;
void main() {
	v_color = a_color;
	v_uv = a_uv;
	gl_Position = u_mvp * vec4(a_position, 1.0);
}`;

const fsSrc = `#version 300 es
precision mediump float;
in vec3 v_color;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_useTex;
out vec4 outColor;
void main() {
	vec4 tex = texture(u_tex, v_uv);
	outColor = mix(vec4(v_color, 1.0), tex, u_useTex);
}`;

function compile(type: number, src: string): WebGLShader {
	const shader = gl!.createShader(type)!;
	gl!.shaderSource(shader, src);
	gl!.compileShader(shader);
	if (!gl!.getShaderParameter(shader, gl!.COMPILE_STATUS)) {
		throw new Error(`Shader compile failed: ${gl!.getShaderInfoLog(shader)}`);
	}
	return shader;
}

const prog = gl.createProgram()!;
gl.attachShader(prog, compile(gl.VERTEX_SHADER, vsSrc));
gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fsSrc));
gl.linkProgram(prog);
if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
	throw new Error(`Program link failed: ${gl.getProgramInfoLog(prog)}`);
}

const uMvp = gl.getUniformLocation(prog, 'u_mvp');
const uTex = gl.getUniformLocation(prog, 'u_tex');
const uUseTex = gl.getUniformLocation(prog, 'u_useTex');

// ---------------------------------------------------------------------------
// Cube geometry (pos3 + color3 + uv2 per vertex)
// ---------------------------------------------------------------------------

const faces = [
	{ n: [0, 0, 1], c: [1, 0.2, 0.2] }, // +Z red
	{ n: [0, 0, -1], c: [0.2, 1, 0.2] }, // -Z green
	{ n: [1, 0, 0], c: [0.3, 0.5, 1] }, // +X blue
	{ n: [-1, 0, 0], c: [1, 1, 0.2] }, // -X yellow
	{ n: [0, 1, 0], c: [1, 0.2, 1] }, // +Y magenta
	{ n: [0, -1, 0], c: [0.2, 1, 1] }, // -Y cyan
];
const verts: number[] = [];
const idx: number[] = [];
for (const { n, c } of faces) {
	const u = n[0] !== 0 ? [0, 1, 0] : [1, 0, 0];
	const v = [
		n[1] * u[2] - n[2] * u[1],
		n[2] * u[0] - n[0] * u[2],
		n[0] * u[1] - n[1] * u[0],
	];
	const base = verts.length / 8;
	for (let i = 0; i < 4; i++) {
		const su = i === 1 || i === 2 ? 1 : -1;
		const sv = i >= 2 ? 1 : -1;
		verts.push(
			n[0] + su * u[0] + sv * v[0],
			n[1] + su * u[1] + sv * v[1],
			n[2] + su * u[2] + sv * v[2],
			c[0],
			c[1],
			c[2],
			(su + 1) / 2,
			(sv + 1) / 2,
		);
	}
	idx.push(base, base + 1, base + 2, base, base + 2, base + 3);
}

const cubeVao = gl.createVertexArray();
gl.bindVertexArray(cubeVao);
gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gl.createBuffer());
gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(idx), gl.STATIC_DRAW);
gl.enableVertexAttribArray(0);
gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 32, 0);
gl.enableVertexAttribArray(1);
gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 32, 12);
gl.enableVertexAttribArray(2);
gl.vertexAttribPointer(2, 2, gl.FLOAT, false, 32, 24);

// ---------------------------------------------------------------------------
// Floor quad (textured; CCW when viewed from above)
// ---------------------------------------------------------------------------

const S = 6;
const floorVao = gl.createVertexArray();
gl.bindVertexArray(floorVao);
gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
// prettier-ignore
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
	-S, -1.6, -S, 1, 1, 1, 0, 0,
	-S, -1.6,  S, 1, 1, 1, 0, 4,
	 S, -1.6,  S, 1, 1, 1, 4, 4,
	-S, -1.6, -S, 1, 1, 1, 0, 0,
	 S, -1.6,  S, 1, 1, 1, 4, 4,
	 S, -1.6, -S, 1, 1, 1, 4, 0,
]), gl.STATIC_DRAW);
gl.enableVertexAttribArray(0);
gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 32, 0);
gl.enableVertexAttribArray(1);
gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 32, 12);
gl.enableVertexAttribArray(2);
gl.vertexAttribPointer(2, 2, gl.FLOAT, false, 32, 24);

// ---------------------------------------------------------------------------
// Checkerboard texture
// ---------------------------------------------------------------------------

const TEX = 64;
const pixels = new Uint8Array(TEX * TEX * 4);
for (let y = 0; y < TEX; y++) {
	for (let x = 0; x < TEX; x++) {
		const on = ((x >> 3) + (y >> 3)) % 2 === 0;
		const o = (y * TEX + x) * 4;
		pixels[o] = on ? 90 : 40;
		pixels[o + 1] = on ? 90 : 40;
		pixels[o + 2] = on ? 110 : 60;
		pixels[o + 3] = 255;
	}
}
gl.activeTexture(gl.TEXTURE0);
gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
gl.texImage2D(
	gl.TEXTURE_2D,
	0,
	gl.RGBA,
	TEX,
	TEX,
	0,
	gl.RGBA,
	gl.UNSIGNED_BYTE,
	pixels,
);
gl.generateMipmap(gl.TEXTURE_2D);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);

// ---------------------------------------------------------------------------
// Matrix helpers
// ---------------------------------------------------------------------------

function mat4Mul(a: Float32Array, b: Float32Array): Float32Array {
	const out = new Float32Array(16);
	for (let i = 0; i < 4; i++) {
		for (let j = 0; j < 4; j++) {
			let s = 0;
			for (let k = 0; k < 4; k++) s += a[k * 4 + j] * b[i * 4 + k];
			out[i * 4 + j] = s;
		}
	}
	return out;
}

function perspective(
	fovy: number,
	aspect: number,
	near: number,
	far: number,
): Float32Array {
	const f = 1 / Math.tan(fovy / 2);
	const out = new Float32Array(16);
	out[0] = f / aspect;
	out[5] = f;
	out[10] = (far + near) / (near - far);
	out[11] = -1;
	out[14] = (2 * far * near) / (near - far);
	return out;
}

function rotYX(ry: number, rx: number): Float32Array {
	const cy = Math.cos(ry);
	const sy = Math.sin(ry);
	const cx = Math.cos(rx);
	const sx = Math.sin(rx);
	// prettier-ignore
	return new Float32Array([
		cy, sx * sy, -cx * sy, 0,
		0, cx, sx, 0,
		sy, -sx * cy, cx * cy, 0,
		0, 0, 0, 1,
	]);
}

function translateZ(z: number): Float32Array {
	const out = new Float32Array(16);
	out[0] = out[5] = out[10] = out[15] = 1;
	out[14] = z;
	return out;
}

// ---------------------------------------------------------------------------
// Render loop
// ---------------------------------------------------------------------------

const proj = perspective(
	Math.PI / 4,
	gl.drawingBufferWidth / gl.drawingBufferHeight,
	0.1,
	100,
);
const viewProj = mat4Mul(proj, translateZ(-7));

gl.enable(gl.DEPTH_TEST);
gl.enable(gl.CULL_FACE);
gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);

function frame() {
	const t = performance.now() / 1000;
	gl!.clearColor(0.05 + 0.03 * Math.sin(t), 0.07, 0.14, 1);
	gl!.clear(gl!.COLOR_BUFFER_BIT | gl!.DEPTH_BUFFER_BIT);

	gl!.useProgram(prog);
	gl!.uniform1i(uTex, 0);

	// Floor (textured)
	gl!.uniformMatrix4fv(uMvp, false, viewProj);
	gl!.uniform1f(uUseTex, 1);
	gl!.bindVertexArray(floorVao);
	gl!.drawArrays(gl!.TRIANGLES, 0, 6);

	// Cube (vertex colors)
	gl!.uniformMatrix4fv(
		uMvp,
		false,
		mat4Mul(viewProj, rotYX(t * 0.9, t * 0.55)),
	);
	gl!.uniform1f(uUseTex, 0);
	gl!.bindVertexArray(cubeVao);
	gl!.drawElements(gl!.TRIANGLES, 36, gl!.UNSIGNED_SHORT, 0);

	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
