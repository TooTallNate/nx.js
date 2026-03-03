import { test } from '../src/tap';

test('console.assert exists and is a function', (t) => {
	t.equal(typeof console.assert, 'function');
});

test('console.assert does not throw on truthy', (t) => {
	console.assert(true, 'should not appear');
	t.pass('no throw');
});

test('console.assert does not throw on falsy', (t) => {
	console.assert(false, 'expected failure');
	t.pass('no throw');
});

test('console.count exists and is a function', (t) => {
	t.equal(typeof console.count, 'function');
});

test('console.count returns undefined', (t) => {
	t.equal(console.count('test-label'), undefined);
});

test('console.countReset exists and is a function', (t) => {
	t.equal(typeof console.countReset, 'function');
});

test('console.countReset returns undefined', (t) => {
	t.equal(console.countReset('test-label'), undefined);
});

test('console.dir exists and is a function', (t) => {
	t.equal(typeof console.dir, 'function');
});

test('console.info exists and is a function', (t) => {
	t.equal(typeof console.info, 'function');
});

test('console.debug exists and is a function', (t) => {
	t.equal(typeof console.debug, 'function');
});

test('console.group exists and is a function', (t) => {
	t.equal(typeof console.group, 'function');
});

test('console.groupCollapsed exists and is a function', (t) => {
	t.equal(typeof console.groupCollapsed, 'function');
});

test('console.groupEnd exists and is a function', (t) => {
	t.equal(typeof console.groupEnd, 'function');
});

test('console.table exists and is a function', (t) => {
	t.equal(typeof console.table, 'function');
});

test('console.time exists and is a function', (t) => {
	t.equal(typeof console.time, 'function');
});

test('console.timeEnd exists and is a function', (t) => {
	t.equal(typeof console.timeEnd, 'function');
});

test('console.timeLog exists and is a function', (t) => {
	t.equal(typeof console.timeLog, 'function');
});

test('console.time/timeEnd do not throw', (t) => {
	console.time('test-timer');
	console.timeEnd('test-timer');
	t.pass('no throw');
});

test('console.time/timeLog do not throw', (t) => {
	console.time('test-timer-2');
	console.timeLog('test-timer-2', 'extra arg');
	console.timeEnd('test-timer-2');
	t.pass('no throw');
});

test('console.group/groupEnd do not throw', (t) => {
	console.group('test group');
	console.groupEnd();
	t.pass('no throw');
});

test('console.clear exists and is a function', (t) => {
	t.equal(typeof console.clear, 'function');
});

test('console.trace exists and is a function', (t) => {
	t.equal(typeof console.trace, 'function');
});
