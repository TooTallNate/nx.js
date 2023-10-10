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
		fetch('add.wasm')
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

test('fib.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('fib.wasm')
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);

	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'fib', kind: 'function' },
		{ name: 'memory', kind: 'memory' },
	]);

	const fib = instance.exports.fib as (i: number) => number;
	assert.type(fib, 'function');
	assert.equal(fib(0), 0);
	assert.equal(fib(1), 1);
	assert.equal(fib(2), 1);
	assert.equal(fib(3), 2);
	assert.equal(fib(4), 3);
	assert.equal(fib(5), 5);
	assert.equal(fib(6), 8);
	assert.equal(fib(7), 13);
	assert.equal(fib(8), 21);
	assert.equal(fib(9), 34);
	assert.equal(fib(10), 55);
	assert.equal(fib(11), 89);
});

test('fail.wasm', async () => {
	const bin = await Switch.readFile(new URL('fail.wasm', Switch.entrypoint));
	const mod = new WebAssembly.Module(bin);

	assert.equal(WebAssembly.Module.imports(mod).length, 0);

	assert.equal(WebAssembly.Module.exports(mod), [
		{ name: 'fail_me', kind: 'function' },
	]);

	const instance = await WebAssembly.instantiate(mod);

	let err: Error | undefined;
	const fail_me = instance.exports.fail_me as Function;
	assert.type(fail_me, 'function');

	try {
		fail_me();
	} catch (err_: any) {
		err = err_;
	}
	assert.ok(err);
	assert.instance(err, WebAssembly.RuntimeError);
	assert.equal(err.name, 'RuntimeError');
	assert.equal(err.message, '[trap] integer divide by zero');
});

test('global.wasm', async () => {
	const g = new WebAssembly.Global({ value: 'i32', mutable: true }, 6);
	assert.equal(g.value, 6, 'getting initial value from JS');

	g.value = 12;
	assert.equal(g.value, 12, 'reading updated value from JS');

	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('global.wasm'),
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

	assert.equal(g.value, 12, 'getting initial value from JS after init');
	assert.equal(getGlobal(), 12, 'getting initial value from wasm');

	g.value = 42;
	assert.equal(getGlobal(), 42, 'getting JS-updated value from wasm');

	incGlobal();
	assert.equal(g.value, 43, 'getting wasm-updated value from JS');
});

test('global-export.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('global-export.wasm')
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

test('memory.wasm', async () => {
	const mem = new WebAssembly.Memory({ initial: 1 });
	const values = new Uint32Array(mem.buffer);

	// Fill the first ten elements of the WASM memory
	for (let i = 0; i < 10; i++) {
		values[i] = i;
	}

	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('memory.wasm'),
		{ js: { mem } }
	);

	assert.equal(WebAssembly.Module.imports(module), [
		{ module: 'js', name: 'mem', kind: 'memory' },
	]);

	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'accumulate', kind: 'function' },
	]);

	const accumulate = instance.exports.accumulate as (
		start: number,
		end: number
	) => number;
	assert.type(accumulate, 'function');

	assert.equal(accumulate(0, 10), 45);
});

test('memory-export.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('memory-export.wasm')
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);

	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'memory', kind: 'memory' },
	]);

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

test('grow.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('grow.wasm')
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);

	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'grow', kind: 'function' },
		{ name: 'getPageCount', kind: 'function' },
		{ name: 'mem', kind: 'memory' },
	]);

	const grow = instance.exports.grow as Function;
	const getPageCount = instance.exports.getPageCount as Function;
	const mem = instance.exports.mem as WebAssembly.Memory;
	assert.instance(mem, WebAssembly.Memory);

	const buf = mem.buffer;
	assert.equal(buf.byteLength, 65536, 'Initially, page size = 1');
	assert.equal(getPageCount(), 1);
	// TODO: make work
	//assert.ok(buf === mem.buffer, 'Same size, same instance (size = 1)');

	grow();
	const buf2 = mem.buffer;
	assert.equal(buf2.byteLength, 65536 * 2, 'Page size = 2');
	assert.equal(getPageCount(), 2);
	assert.ok(buf !== buf2, 'Different size, different instance (size = 2)');
	// TODO: make work
	//assert.ok(buf2 === mem.buffer, 'Same size, same instance (size = 2)');

	grow();
	const buf3 = mem.buffer;
	assert.equal(buf3.byteLength, 65536 * 3, 'Page size = 3');
	assert.equal(getPageCount(), 3);
	assert.ok(buf2 !== buf3, 'Different size, different instance (size = 3)');
	// TODO: make work
	//assert.ok(buf3 === mem.buffer, 'Same size, same instance (size = 3)');

	// Now using `mem.grow()` from the JavaScript side
	mem.grow(2);
	const buf4 = mem.buffer;
	assert.equal(buf4.byteLength, 65536 * 5, 'Page size = 5');
	assert.equal(getPageCount(), 5);
});

test('compute.wasm', async () => {
	let aVal = -1;
	let bVal = -1;
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('compute.wasm'),
		{
			env: {
				compute(a: number, b: number) {
					aVal = a;
					bVal = b;
					return Math.round(a * b);
				},
			},
		}
	);
	assert.equal(WebAssembly.Module.imports(module), [
		{ module: 'env', name: 'compute', kind: 'function' },
	]);

	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'invoke', kind: 'function' },
		{ name: 'val', kind: 'global' },
	]);

	const invoke = instance.exports.invoke as Function;
	assert.type(invoke, 'function');

	const val = instance.exports.val as WebAssembly.Global<'i32'>;
	assert.instance(val, WebAssembly.Global);

	assert.equal(val.value, 0, 'Global value starts at `0`');
	invoke(1.23, 3.45);
	assert.equal(
		aVal,
		2.46,
		'`compute()` should have been invoked with values doubled - a'
	);
	assert.equal(
		bVal,
		6.9,
		'`compute()` should have been invoked with values doubled - b'
	);
	assert.equal(
		val.value,
		17,
		'Global value should be result of `a * b` rounded to nearest'
	);
});

test('table.wasm', async () => {
	const { module, instance } = await WebAssembly.instantiateStreaming(
		fetch('table.wasm')
	);
	assert.equal(WebAssembly.Module.imports(module).length, 0);
	assert.equal(WebAssembly.Module.exports(module), [
		{ name: 'tbl', kind: 'table' },
	]);

	const tbl = instance.exports.tbl as WebAssembly.Table;
	assert.instance(tbl, WebAssembly.Table);

	assert.equal(tbl.length, 2);

	const fn0 = tbl.get(0);
	assert.equal(fn0(), 13);

	const fn1 = tbl.get(1);
	assert.equal(fn1(), 42);

	let err: Error | undefined;
	try {
		tbl.get(2);
	} catch (_err: any) {
		err = _err;
	}
	assert.ok(err);
	assert.instance(err, RangeError);
	assert.equal(
		err.message,
		'WebAssembly.Table.get(): invalid index 2 into funcref table of size 2'
	);
});

test('Imported function throws an Error is propagated', async () => {
	const e = new Error('will be thrown');
	const { instance } = await WebAssembly.instantiateStreaming(
		fetch('simple.wasm'),
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
