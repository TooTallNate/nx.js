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
