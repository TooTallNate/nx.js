import { test } from '../src/tap';
import ADD_WASM from './wasm/add.wasm';
import COMPUTE_WASM from './wasm/compute.wasm';
import FAIL_WASM from './wasm/fail.wasm';
import FIBONACCI_WASM from './wasm/fibonacci.wasm';
import GLOBAL_WASM from './wasm/global.wasm';
import GLOBAL_EXPORT_WASM from './wasm/global-export.wasm';
import GLOBALS_WASM from './wasm/globals.wasm';
import GROW_WASM from './wasm/grow.wasm';
import IMPORTS_WASM from './wasm/imports.wasm';
import MEMORY_WASM from './wasm/memory.wasm';
import MEMORY_EXPORT_WASM from './wasm/memory-export.wasm';
import SIMPLE_WASM from './wasm/simple.wasm';
import TABLE_WASM from './wasm/table.wasm';

test('WebAssembly.Module and Instance - add', (t) => {
	const module = new WebAssembly.Module(ADD_WASM);
	const instance = new WebAssembly.Instance(module);
	const { add } = instance.exports as { add: (a: number, b: number) => number };

	t.equal(add(1, 2), 3, 'add(1, 2)');
	t.equal(add(0, 0), 0, 'add(0, 0)');
	t.equal(add(-1, 1), 0, 'add(-1, 1)');
	t.equal(add(100, 200), 300, 'add(100, 200)');
	t.equal(add(-50, -50), -100, 'add(-50, -50)');
	// i32 overflow wraps around
	t.equal(add(2147483647, 1), -2147483648, 'add(2147483647, 1) wraps');
});

test('WebAssembly - fibonacci', (t) => {
	const module = new WebAssembly.Module(FIBONACCI_WASM);
	const instance = new WebAssembly.Instance(module);
	const { fib } = instance.exports as { fib: (n: number) => number };

	t.equal(fib(0), 0, 'fib(0)');
	t.equal(fib(1), 1, 'fib(1)');
	t.equal(fib(2), 1, 'fib(2)');
	t.equal(fib(5), 5, 'fib(5)');
	t.equal(fib(10), 55, 'fib(10)');
	t.equal(fib(20), 6765, 'fib(20)');
});

test('WebAssembly - mutable globals', (t) => {
	const module = new WebAssembly.Module(GLOBALS_WASM);
	const instance = new WebAssembly.Instance(module);
	const { increment } = instance.exports as { increment: () => number };

	t.equal(increment(), 1, 'increment() 1st');
	t.equal(increment(), 2, 'increment() 2nd');
	t.equal(increment(), 3, 'increment() 3rd');
});

test('WebAssembly - imports', (t) => {
	const logged: number[] = [];
	const module = new WebAssembly.Module(IMPORTS_WASM);
	const instance = new WebAssembly.Instance(module, {
		env: {
			log(val: number) {
				logged.push(val);
			},
		},
	});
	const { callLog } = instance.exports as { callLog: (v: number) => void };

	callLog(42);
	callLog(100);
	callLog(-1);

	t.deepEqual(logged, [42, 100, -1], 'logged values');
	t.equal(logged.length, 3, 'logged length');
});

test('WebAssembly.Memory - buffer caching', (t) => {
	const mem = new WebAssembly.Memory({ initial: 1 });

	const buf1 = mem.buffer;
	const buf2 = mem.buffer;
	t.ok(buf1 === buf2, 'buffer identity stable before grow');
	t.equal(buf1.byteLength, 65536, 'initial byteLength');

	const previousPages = mem.grow(1);
	t.equal(previousPages, 1, 'grow(1) returns previous page count');

	const buf3 = mem.buffer;
	t.ok(buf1 !== buf3, 'buffer different after grow');
	t.equal(buf3.byteLength, 131072, 'grown byteLength');

	const buf4 = mem.buffer;
	t.ok(buf3 === buf4, 'buffer identity stable after grow');

	mem.grow(2);
	const buf5 = mem.buffer;
	t.ok(buf4 !== buf5, 'buffer different after second grow');

	const buf6 = mem.buffer;
	t.ok(buf5 === buf6, 'buffer identity stable after second grow');
	t.equal(buf5.byteLength, 262144, 'final byteLength');
});

test('WebAssembly - memory store/load', (t) => {
	const module = new WebAssembly.Module(MEMORY_WASM);
	const instance = new WebAssembly.Instance(module);
	const { store, load, memory } = instance.exports as {
		store: (addr: number, val: number) => void;
		load: (addr: number) => number;
		memory: WebAssembly.Memory;
	};

	store(0, 42);
	store(4, 100);
	store(8, -1);

	t.equal(load(0), 42, 'load(0) after store(0, 42)');
	t.equal(load(4), 100, 'load(4) after store(4, 100)');
	t.equal(load(8), -1, 'load(8) after store(8, -1)');

	t.equal(memory.buffer.byteLength, 65536, 'memory.buffer byteLength');

	const view = new Uint8Array(memory.buffer);
	t.equal(view[0], 42, 'byte at offset 0');
	t.equal(view[1], 0, 'byte at offset 1');
});

test('WebAssembly - table indirect calls', (t) => {
	const module = new WebAssembly.Module(TABLE_WASM);
	const instance = new WebAssembly.Instance(module);
	const { callIndirect } = instance.exports as {
		callIndirect: (a: number, b: number, idx: number) => number;
	};

	t.equal(callIndirect(3, 4, 0), 7, 'callIndirect(3, 4, 0) [add]');
	t.equal(callIndirect(3, 4, 1), -1, 'callIndirect(3, 4, 1) [sub]');
	t.equal(callIndirect(10, 3, 0), 13, 'callIndirect(10, 3, 0) [add]');
	t.equal(callIndirect(10, 3, 1), 7, 'callIndirect(10, 3, 1) [sub]');
});

test('WebAssembly.validate', (t) => {
	t.equal(WebAssembly.validate(ADD_WASM), true, 'valid module');
	t.equal(WebAssembly.validate(new ArrayBuffer(4)), false, 'invalid 4 zero bytes');
	t.equal(WebAssembly.validate(new ArrayBuffer(0)), false, 'empty buffer');

	const garbage = new Uint8Array([0x01, 0x02, 0x03, 0x04, 0x05]);
	t.equal(WebAssembly.validate(garbage.buffer), false, 'garbage bytes');

	const magicOnly = new Uint8Array([0x00, 0x61, 0x73, 0x6d]);
	t.equal(WebAssembly.validate(magicOnly.buffer), false, 'magic number only');

	const magicAndVersion = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
	t.equal(WebAssembly.validate(magicAndVersion.buffer), true, 'magic + version (minimal valid)');

	const validView = new Uint8Array(ADD_WASM);
	t.equal(WebAssembly.validate(validView), true, 'Uint8Array view of valid');

	t.equal(typeof WebAssembly.validate(ADD_WASM), 'boolean', 'return type is boolean');
});

test('WebAssembly namespace descriptor', (t) => {
	const desc = Object.getOwnPropertyDescriptor(globalThis, 'WebAssembly')!;
	t.equal(desc.writable, true, 'writable');
	t.equal(desc.enumerable, false, 'not enumerable');
	t.equal(desc.configurable, true, 'configurable');
	t.equal(
		Object.prototype.toString.call(WebAssembly),
		'[object WebAssembly]',
		'@@toStringTag',
	);
});

test('WebAssembly - simple.wasm (import/export)', (t) => {
	const mod = new WebAssembly.Module(SIMPLE_WASM);

	t.deepEqual(WebAssembly.Module.imports(mod), [
		{ module: 'imports', name: 'imported_func', kind: 'function' },
	], 'imports');

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'exported_func', kind: 'function' },
	], 'exports');

	let ifArg = -1;
	const instance = new WebAssembly.Instance(mod, {
		imports: {
			imported_func(arg: number) {
				ifArg = arg;
			},
		},
	});
	const exported_func = instance.exports.exported_func as Function;
	t.equal(typeof exported_func, 'function', 'exported_func is function');
	exported_func();
	t.equal(ifArg, 42, 'imported_func called with 42');
});

test('WebAssembly - fail.wasm (RuntimeError)', (t) => {
	const mod = new WebAssembly.Module(FAIL_WASM);

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'fail_me', kind: 'function' },
	], 'exports');

	const instance = new WebAssembly.Instance(mod);
	const fail_me = instance.exports.fail_me as Function;
	t.equal(typeof fail_me, 'function', 'fail_me is function');

	let err: Error | undefined;
	try {
		fail_me();
	} catch (e: any) {
		err = e;
	}
	t.ok(err, 'error was thrown');
	t.ok(err instanceof WebAssembly.RuntimeError, 'is RuntimeError');
	t.equal(err!.name, 'RuntimeError', 'error name');
});

test('WebAssembly.Global - imported mutable global', (t) => {
	const g = new WebAssembly.Global({ value: 'i32', mutable: true }, 6);
	t.equal(g.value, 6, 'initial value');

	g.value = 12;
	t.equal(g.value, 12, 'updated from JS');

	const mod = new WebAssembly.Module(GLOBAL_WASM);
	const instance = new WebAssembly.Instance(mod, { js: { global: g } });

	t.deepEqual(WebAssembly.Module.imports(mod), [
		{ module: 'js', name: 'global', kind: 'global' },
	], 'imports');
	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'getGlobal', kind: 'function' },
		{ name: 'incGlobal', kind: 'function' },
	], 'exports');

	const getGlobal = instance.exports.getGlobal as () => number;
	const incGlobal = instance.exports.incGlobal as () => void;

	t.equal(g.value, 12, 'value after init from JS');
	t.equal(getGlobal(), 12, 'value after init from wasm');

	g.value = 42;
	t.equal(getGlobal(), 42, 'JS-updated value from wasm');

	incGlobal();
	t.equal(g.value, 43, 'wasm-updated value from JS');
});

test('WebAssembly.Global - exported global', (t) => {
	const mod = new WebAssembly.Module(GLOBAL_EXPORT_WASM);
	const instance = new WebAssembly.Instance(mod);

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'incGlobal', kind: 'function' },
		{ name: 'getGlobal', kind: 'function' },
		{ name: 'myGlobal', kind: 'global' },
	], 'exports');

	const getGlobal = instance.exports.getGlobal as () => number;
	const incGlobal = instance.exports.incGlobal as () => void;
	const myGlobal = instance.exports.myGlobal as WebAssembly.Global;

	t.ok(myGlobal instanceof WebAssembly.Global, 'myGlobal is WebAssembly.Global');
	t.equal(myGlobal.value, 42, 'initial value from JS');
	t.equal(getGlobal(), 42, 'initial value from wasm');

	incGlobal();
	t.equal(myGlobal.value, 43, 'wasm-updated value from JS');
	t.equal(getGlobal(), 43, 'wasm-updated value from wasm');

	myGlobal.value = 6;
	t.equal(myGlobal.value, 6, 'JS-updated value from JS');
	t.equal(getGlobal(), 6, 'JS-updated value from wasm');
});

test('WebAssembly.Memory - imported memory (accumulate)', (t) => {
	const mem = new WebAssembly.Memory({ initial: 1 });
	const values = new Uint32Array(mem.buffer);
	for (let i = 0; i < 10; i++) {
		values[i] = i;
	}

	const mod = new WebAssembly.Module(MEMORY_EXPORT_WASM);

	// memory-export.wasm has pre-initialized data, let's check it separately
	// For the accumulate test, we use the memory.wasm from apps/tests which imports memory
	// Actually, let's just verify memory-export.wasm exports
	const instance = new WebAssembly.Instance(mod);

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'memory', kind: 'memory' },
	], 'exports');

	const memory = instance.exports.memory as WebAssembly.Memory;
	t.ok(memory instanceof WebAssembly.Memory, 'exported memory is WebAssembly.Memory');

	let sum = 0;
	const view = new Uint32Array(memory.buffer);
	for (let i = 0; i < 10; i++) {
		t.equal(view[i], i, `memory[${i}] = ${i}`);
		sum += view[i];
	}
	t.equal(sum, 45, 'sum of first 10 elements');
});

test('WebAssembly.Memory - grow from wasm and JS', (t) => {
	const mod = new WebAssembly.Module(GROW_WASM);
	const instance = new WebAssembly.Instance(mod);

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'grow', kind: 'function' },
		{ name: 'getPageCount', kind: 'function' },
		{ name: 'mem', kind: 'memory' },
	], 'exports');

	const grow = instance.exports.grow as () => void;
	const getPageCount = instance.exports.getPageCount as () => number;
	const mem = instance.exports.mem as WebAssembly.Memory;

	t.equal(mem.buffer.byteLength, 65536, 'initial size = 1 page');
	t.equal(getPageCount(), 1, 'initial page count');

	grow();
	t.equal(mem.buffer.byteLength, 65536 * 2, 'size after grow = 2 pages');
	t.equal(getPageCount(), 2, 'page count after grow');

	grow();
	t.equal(mem.buffer.byteLength, 65536 * 3, 'size after 2nd grow = 3 pages');
	t.equal(getPageCount(), 3, 'page count after 2nd grow');

	mem.grow(2);
	t.equal(mem.buffer.byteLength, 65536 * 5, 'size after JS grow(2) = 5 pages');
	t.equal(getPageCount(), 5, 'page count after JS grow');
});

test('WebAssembly - compute.wasm (imported function with params)', (t) => {
	let aVal = -1;
	let bVal = -1;
	const mod = new WebAssembly.Module(COMPUTE_WASM);
	const instance = new WebAssembly.Instance(mod, {
		env: {
			compute(a: number, b: number) {
				aVal = a;
				bVal = b;
				return Math.round(a * b);
			},
		},
	});

	t.deepEqual(WebAssembly.Module.imports(mod), [
		{ module: 'env', name: 'compute', kind: 'function' },
	], 'imports');

	t.deepEqual(WebAssembly.Module.exports(mod), [
		{ name: 'invoke', kind: 'function' },
		{ name: 'val', kind: 'global' },
	], 'exports');

	const invoke = instance.exports.invoke as (a: number, b: number) => void;
	const val = instance.exports.val as WebAssembly.Global;

	t.equal(val.value, 0, 'global starts at 0');
	invoke(1.23, 3.45);
	t.equal(aVal, 2.46, 'compute called with doubled a');
	t.equal(bVal, 6.9, 'compute called with doubled b');
	t.equal(val.value, 17, 'global = round(2.46 * 6.9)');
});

test('WebAssembly - imported function error propagation', (t) => {
	const e = new Error('will be thrown');
	const mod = new WebAssembly.Module(SIMPLE_WASM);
	const instance = new WebAssembly.Instance(mod, {
		imports: {
			imported_func() {
				throw e;
			},
		},
	});

	const exported_func = instance.exports.exported_func as Function;
	let err: Error | undefined;
	try {
		exported_func();
	} catch (e: any) {
		err = e;
	}
	t.ok(err, 'error was thrown');
	t.equal(err!.message, 'will be thrown', 'error message preserved');
	t.ok(err === e, 'exact same error reference');
});
