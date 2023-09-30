import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('WebAssembly');

test('simple.wasm', async () => {
	const bin = await Switch.readFile(
		new URL('simple.wasm', Switch.entrypoint)
	);
	const mod = new WebAssembly.Module(bin);

	assert.equal(WebAssembly.Module.imports(mod), [
		{ module: 'imports', name: 'imported_func', kind: 'function' },
	]);

	assert.equal(WebAssembly.Module.exports(mod), [
		{ name: 'exported_func', kind: 'function' },
	]);

	let ifArg = -1;
	const instance = await WebAssembly.instantiate(mod, {
		imports: {
			imported_func(arg: number) {
				ifArg = arg;
			},
		},
	});
	const exported_func = instance.exports.exported_func as Function;
	assert.type(exported_func, 'function');
	exported_func();
	assert.equal(ifArg, 42);
});

test('add.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch(new URL('add.wasm', Switch.entrypoint))
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);
	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'add', kind: 'function' },
	]);
	const add = instance.exports.add as Function;
	assert.type(add, 'function');
	assert.equal(add(1, 2), 3);
	assert.equal(add(39, 3), 42);
});

test('global.wasm', async () => {
	const g = new WebAssembly.Global({ value: 'i32', mutable: true }, 6);
	assert.equal(g.value, 6, 'getting initial value from JS');

	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch(new URL('global.wasm', Switch.entrypoint)),
		{
			js: { global: g },
		}
	);
	assert.equal(WebAssembly.Module.imports(module), [
		{ module: 'js', name: 'global', kind: 'global' },
	]);
	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'getGlobal', kind: 'function' },
		{ name: 'incGlobal', kind: 'function' },
	]);
	const getGlobal = instance.exports.getGlobal as Function;
	assert.type(getGlobal, 'function');
	const incGlobal = instance.exports.incGlobal as Function;
	assert.type(incGlobal, 'function');

	assert.equal(g.value, 6, 'getting initial value from JS after init');
	assert.equal(getGlobal(), 6, 'getting initial value from wasm');

	g.value = 42;
	assert.equal(getGlobal(), 42, 'getting JS-updated value from wasm');

	incGlobal();
	assert.equal(g.value, 43, 'getting wasm-updated value from JS');
});

test('global-export.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch(new URL('global-export.wasm', Switch.entrypoint))
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);
	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'incGlobal', kind: 'function' },
		{ name: 'getGlobal', kind: 'function' },
		{ name: 'myGlobal', kind: 'global' },
	]);

	const getGlobal = instance.exports.getGlobal as Function;
	assert.type(getGlobal, 'function');
	const incGlobal = instance.exports.incGlobal as Function;
	assert.type(incGlobal, 'function');
	const myGlobal = instance.exports.myGlobal as WebAssembly.Global<'i32'>;
	assert.instance(myGlobal, WebAssembly.Global);

	assert.equal(myGlobal.value, 42, 'getting initial value from JS');
	assert.equal(getGlobal(), 42, 'getting initial value from WASM');

	// Modify global from WASM
	incGlobal();
	assert.equal(myGlobal.value, 43, 'getting updated value from JS');
	assert.equal(getGlobal(), 43, 'getting updated value from WASM');

	// Modify global from JS
	myGlobal.value = 6;
	assert.equal(myGlobal.value, 6, 'getting updated value from JS');
	assert.equal(getGlobal(), 6, 'getting updated value from WASM');
});

test('memory-export.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch(new URL('memory-export.wasm', Switch.entrypoint))
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);

	// TODO: this needs an `instance`
	//assert.equal(WebAssembly.Module.exports(module), [
	//	{ name: 'memory', kind: 'memory' },
	//]);

	const memory = instance.exports.memory as WebAssembly.Memory;
	assert.instance(memory, WebAssembly.Memory);

	let sum = 0;
	const values = new Uint32Array(memory.buffer);

	// Test the first ten elements of the WASM memory
	for (let i = 0; i < 10; i++) {
		assert.equal(values[i], i);
		sum += values[i];
	}

	assert.equal(sum, 45);
});

test('Imported function throws an Error is propagated', async () => {
	const e = new Error('will be thrown');
	const { instance } = await WebAssembly.instantiateStreaming(
		fetch(new URL('simple.wasm', Switch.entrypoint)),
		{
			imports: {
				imported_func() {
					throw e;
				},
			},
		}
	);
	let err: Error | undefined;
	const exported_func = instance.exports.exported_func as Function;
	assert.type(exported_func, 'function');
	try {
		exported_func();
	} catch (err_: any) {
		err = err_;
	}
	assert.ok(err);
	assert.equal(err.message, e.message);
	// Should be the exact same reference
	assert.ok(err === e);
});

test.run();
