import { test } from '../src/tap';

// Inline .wasm bytes so tests work in both QuickJS and Chrome environments

// prettier-ignore
const ADD_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,7,1,96,2,127,127,1,127,3,2,1,0,7,7,1,3,97,100,100,0,0,10,9,1,7,0,32,0,32,1,106,11]);

// prettier-ignore
const FIBONACCI_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,6,1,96,1,127,1,127,3,2,1,0,7,7,1,3,102,105,98,0,0,10,30,1,28,0,32,0,65,2,72,4,127,32,0,5,32,0,65,1,107,16,0,32,0,65,2,107,16,0,106,11,11]);

// prettier-ignore
const GLOBALS_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,3,2,1,0,6,6,1,127,1,65,0,11,7,23,2,7,99,111,117,110,116,101,114,3,0,9,105,110,99,114,101,109,101,110,116,0,0,10,13,1,11,0,35,0,65,1,106,36,0,35,0,11]);

// prettier-ignore
const IMPORTS_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,1,127,0,2,11,1,3,101,110,118,3,108,111,103,0,0,3,2,1,0,7,11,1,7,99,97,108,108,76,111,103,0,1,10,8,1,6,0,32,0,16,0,11]);

// prettier-ignore
const MEMORY_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,11,2,96,2,127,127,0,96,1,127,1,127,3,3,2,0,1,5,3,1,0,1,7,25,3,6,109,101,109,111,114,121,2,0,5,115,116,111,114,101,0,0,4,108,111,97,100,0,1,10,19,2,9,0,32,0,32,1,54,2,0,11,7,0,32,0,40,2,0,11]);

// prettier-ignore
const TABLE_WASM = new Uint8Array([0,97,115,109,1,0,0,0,1,14,2,96,2,127,127,1,127,96,3,127,127,127,1,127,3,4,3,0,0,1,4,4,1,112,0,2,7,24,2,5,116,97,98,108,101,1,0,12,99,97,108,108,73,110,100,105,114,101,99,116,0,2,9,8,1,0,65,0,11,2,0,1,10,29,3,7,0,32,0,32,1,106,11,7,0,32,0,32,1,107,11,11,0,32,0,32,1,32,2,17,0,0,11]);

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
