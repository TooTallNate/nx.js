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
	if (typeof instance.exports.exported_func === 'function') {
		instance.exports.exported_func();
	} else {
		assert.ok(
			false,
			'Expected `instance.exports.exported_func` to be a "function"'
		);
	}
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
	if (typeof instance.exports.add === 'function') {
		assert.equal(instance.exports.add(1, 2), 3);
		assert.equal(instance.exports.add(39, 3), 42);
	} else {
		assert.ok(false, 'Expected `instance.exports.add` to be a "function"');
	}
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
	if (typeof instance.exports.exported_func === 'function') {
		try {
			instance.exports.exported_func();
		} catch (err_: any) {
			err = err_;
		}
	} else {
		assert.ok(
			false,
			'Expected `instance.exports.exported_func` to be a "function"'
		);
	}
	assert.ok(err);
	assert.equal(err.message, e.message);
	// Should be the exact same reference
	assert.ok(err === e);
});

test.run();
