import { test } from '../src/tap';
import ADD_WASM from './wasm/add.wasm';
import FIBONACCI_WASM from './wasm/fibonacci.wasm';
import GLOBALS_WASM from './wasm/globals.wasm';
import IMPORTS_WASM from './wasm/imports.wasm';
import MEMORY_WASM from './wasm/memory.wasm';
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
