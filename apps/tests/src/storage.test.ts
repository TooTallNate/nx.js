import { suite } from 'uvu';
import * as assert from 'uvu/assert';

// Set the current profile to the first profile
// so that the profile selector is not shown
const firstProfile = Array.from(Switch.Profile)[0];
Switch.Profile.current = firstProfile;

const test = suite('Storage');
const storage = localStorage;
assert.ok(storage);

test('localStorage builtins', () => {
	storage.clear();
	assert.equal(storage.length, 0, 'storage.length');

	var builtins = ['key', 'getItem', 'setItem', 'removeItem', 'clear'];
	var origBuiltins = builtins.map(function (b) {
		return Storage.prototype[b];
	});
	assert.equal(
		builtins.map(function (b) {
			return storage[b];
		}),
		origBuiltins,
		'a',
	);
	builtins.forEach(function (b) {
		storage[b] = b;
	});
	assert.equal(
		builtins.map(function (b) {
			return storage[b];
		}),
		origBuiltins,
		'b',
	);
	assert.equal(
		builtins.map(function (b) {
			return storage.getItem(b);
		}),
		builtins,
		'c',
	);

	assert.equal(storage.length, builtins.length, 'storage.length');
});

test('localStorage.clear()', () => {
	storage.clear();

	storage.setItem('name', 'user1');
	assert.equal(storage.getItem('name'), 'user1');
	assert.equal(storage.name, 'user1');
	assert.equal(storage.length, 1);

	storage.clear();
	assert.equal(storage.getItem('name'), null, "storage.getItem('name')");
	assert.equal(storage.name, undefined, 'storage.name');
	assert.equal(storage.length, 0, 'storage.length');
});

test('localStorage enumerate', () => {
	storage.clear();

	try {
		Storage.prototype.prototypeTestKey = 'prototypeTestValue';
		storage.foo = 'bar';
		storage.fu = 'baz';
		storage.batman = 'bin suparman';
		storage.bar = 'foo';
		storage.alpha = 'beta';
		storage.zeta = 'gamma';

		const enumeratedArray = Object.keys(storage);
		enumeratedArray.sort(); // Storage order is implementation-defined.

		const expectArray = ['alpha', 'bar', 'batman', 'foo', 'fu', 'zeta'];
		assert.equal(enumeratedArray, expectArray);

		// 'prototypeTestKey' is not an actual storage key, it is just a
		// property set on Storage's prototype object.
		assert.equal(storage.length, 6);
		assert.equal(storage.getItem('prototypeTestKey'), null);
		assert.equal(storage.prototypeTestKey, 'prototypeTestValue');
	} finally {
		delete Storage.prototype.prototypeTestKey;
	}
});

test('localStorage.getItem()', () => {
	storage.clear();
	storage.setItem('name', 'x');
	storage.setItem('undefined', 'foo');
	storage.setItem('null', 'bar');
	storage.setItem('', 'baz');

	assert.equal(storage.length, 4);

	assert.equal(storage['unknown'], undefined, "storage['unknown']");
	assert.equal(storage['name'], 'x', "storage['name']");
	assert.equal(storage['undefined'], 'foo', "storage['undefined']");
	assert.equal(storage['null'], 'bar', "storage['null']");
	// @ts-expect-error
	assert.equal(storage[undefined], 'foo', 'storage[undefined]');
	// @ts-expect-error
	assert.equal(storage[null], 'bar', 'storage[null]');
	assert.equal(storage[''], 'baz', "storage['']");

	assert.equal(storage.getItem('unknown'), null, "storage.getItem('unknown')");
	assert.equal(storage.getItem('name'), 'x', "storage.getItem('name')");
	assert.equal(
		storage.getItem('undefined'),
		'foo',
		"storage.getItem('undefined')",
	);
	assert.equal(storage.getItem('null'), 'bar', "storage.getItem('null')");
	assert.equal(
		// @ts-expect-error
		storage.getItem(undefined),
		'foo',
		'storage.getItem(undefined)',
	);
	// @ts-expect-error
	assert.equal(storage.getItem(null), 'bar', 'storage.getItem(null)');
	assert.equal(storage.getItem(''), 'baz', "storage.getItem('')");
});

test('localStorage `in` operator', () => {
	// property access
	storage.clear();
	assert.not('name' in storage);
	storage['name'] = 'user1';
	assert.ok('name' in storage);

	// method access
	storage.clear();
	assert.not('name' in storage);
	storage.setItem('name', 'user1');
	assert.ok('name' in storage);
	assert.equal(storage.name, 'user1');
	storage.removeItem('name');
	assert.not('name' in storage);
});

test('localStorage indexed getter', () => {
	storage.clear();
	storage['name'] = 'user1';
	storage['age'] = '42';

	assert.equal(storage[-1], undefined);
	assert.equal(storage[0], undefined);
	assert.equal(storage[1], undefined);
	assert.equal(storage[2], undefined);

	assert.equal(storage['-1'], undefined);
	assert.equal(storage['0'], undefined);
	assert.equal(storage['1'], undefined);
	assert.equal(storage['2'], undefined);

	// @ts-expect-error
	storage.setItem(1, 'number');
	assert.equal(storage[1], 'number');
	assert.equal(storage['1'], 'number');
});

test('localStorage.key()', () => {
	storage.clear();

	storage.setItem('name', 'user1');
	storage.setItem('age', '20');
	storage.setItem('a', '1');
	storage.setItem('b', '2');

	var keys = ['name', 'age', 'a', 'b'];
	function doTest(index: number) {
		assert.ok(storage);
		var key = storage.key(index);
		assert.not.equal(key, null);
		assert.ok(keys.indexOf(key!) >= 0, 'Unexpected key ' + key + ' found.');
	}
	for (var i = 0; i < keys.length; ++i) {
		doTest(i);
		doTest(i + 0x100000000);
	}

	assert.equal(storage.key(-1), null, 'storage.key(-1)');
	assert.equal(storage.key(4), null, 'storage.key(4)');
});

test('localStorage.setItem()', () => {
	var interesting_strs = [
		'\uD7FF',
		'\uD800',
		'\uDBFF',
		'\uDC00',
		'\uDFFF',
		'\uE000',
		'\uFFFD',
		'\uFFFE',
		'\uFFFF',
		'\uD83C\uDF4D',
		'\uD83Ca',
		'a\uDF4D',
		'\uDBFF\uDFFF',
	];
	for (var i = 0; i <= 0xff; i++) {
		interesting_strs.push(String.fromCharCode(i));
	}

	// [] update
	storage.clear();
	storage['name'] = 'user1';
	storage['name'] = 'user2';
	assert.ok('name' in storage);
	assert.equal(storage.length, 1, 'storage.length');
	assert.equal(storage.getItem('name'), 'user2');
	assert.equal(storage['name'], 'user2');

	// null
	storage.clear();
	// @ts-expect-error
	storage.setItem('age', null);
	assert.ok('age' in storage);
	assert.equal(storage.length, 1, 'storage.length');
	assert.equal(storage.getItem('age'), 'null');
	assert.equal(storage['age'], 'null');

	// throws
	storage.clear();
	storage.setItem('age', '10');
	assert.throws(function () {
		// @ts-expect-error
		storage.setItem('age', {
			toString: function () {
				throw new Error('throws');
			},
		});
	}, 'throws');
	assert.ok('age' in storage);
	assert.equal(storage.length, 1, 'storage.length');
	assert.equal(storage.getItem('age'), '10');
	assert.equal(storage['age'], '10');

	// key containing NULL byte
	storage.clear();
	storage['foo\0bar'] = 'user1';
	assert.ok('foo\0bar' in storage);
	assert.not('foo\0' in storage);
	assert.not('foo\0baz' in storage);
	assert.not('foo' in storage);
	assert.equal(storage.length, 1, 'storage.length');
	assert.equal(storage.getItem('foo\0bar'), 'user1');
	assert.equal(storage.getItem('foo\0'), null);
	assert.equal(storage.getItem('foo\0baz'), null);
	assert.equal(storage.getItem('foo'), null);
	assert.equal(storage['foo\0bar'], 'user1');
	assert.equal(storage['foo\0'], undefined);
	assert.equal(storage['foo\0baz'], undefined);
	assert.equal(storage['foo'], undefined);

	// value containing NULL byte
	storage.clear();
	storage['name'] = 'foo\0bar';
	assert.ok('name' in storage);
	assert.equal(storage.length, 1, 'storage.length');
	assert.equal(storage.getItem('name'), 'foo\0bar');
	assert.equal(storage['name'], 'foo\0bar');

	for (i = 0; i < interesting_strs.length; i++) {
		var str = interesting_strs[i];
		storage.clear();
		storage[str] = 'user1';
		assert.ok(str in storage);
		assert.equal(storage.length, 1, 'storage.length');
		assert.equal(storage.getItem(str), 'user1');
		assert.equal(storage[str], 'user1');

		storage.clear();
		storage['name'] = str;
		assert.equal(storage.length, 1, 'storage.length');
		assert.equal(storage.getItem('name'), str);
		assert.equal(storage['name'], str);
	}
});

test.run();
