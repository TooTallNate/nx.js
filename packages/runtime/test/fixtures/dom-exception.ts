import { test } from '../src/tap';

test('DOMException is a global', (t) => {
	t.ok(typeof DOMException === 'function', 'DOMException is a function');
});

test('new DOMException() defaults', (t) => {
	const e = new DOMException();
	t.equal(e.message, '', 'default message is empty');
	t.equal(e.name, 'Error', 'default name is Error');
});

test('new DOMException(msg)', (t) => {
	const e = new DOMException('msg');
	t.equal(e.message, 'msg', 'message is msg');
	t.equal(e.name, 'Error', 'name is Error');
});

test('new DOMException(msg, NotFoundError)', (t) => {
	const e = new DOMException('msg', 'NotFoundError');
	t.equal(e.name, 'NotFoundError', 'name is NotFoundError');
	t.equal(e.code, 8, 'code is 8');
});

test('DOMException instance is an Error', (t) => {
	const e = new DOMException();
	t.ok(e instanceof Error, 'instanceof Error');
});

test('DOMException.prototype has error code constants', (t) => {
	t.equal(DOMException.prototype.INDEX_SIZE_ERR, 1, 'INDEX_SIZE_ERR is 1');
	t.equal(DOMException.prototype.NOT_FOUND_ERR, 8, 'NOT_FOUND_ERR is 8');
	t.equal(DOMException.prototype.SYNTAX_ERR, 12, 'SYNTAX_ERR is 12');
	t.equal(
		DOMException.prototype.INVALID_STATE_ERR,
		11,
		'INVALID_STATE_ERR is 11',
	);
	t.equal(DOMException.prototype.ABORT_ERR, 20, 'ABORT_ERR is 20');
});

test('Instance has error code constants', (t) => {
	const e = new DOMException();
	t.equal(e.INDEX_SIZE_ERR, 1, 'INDEX_SIZE_ERR is 1');
	t.equal(e.NOT_FOUND_ERR, 8, 'NOT_FOUND_ERR is 8');
	t.equal(e.ABORT_ERR, 20, 'ABORT_ERR is 20');
});

test('Static constants on DOMException', (t) => {
	t.equal(DOMException.INDEX_SIZE_ERR, 1, 'INDEX_SIZE_ERR is 1');
	t.equal(DOMException.NOT_FOUND_ERR, 8, 'NOT_FOUND_ERR is 8');
	t.equal(DOMException.ABORT_ERR, 20, 'ABORT_ERR is 20');
	t.equal(DOMException.QUOTA_EXCEEDED_ERR, 22, 'QUOTA_EXCEEDED_ERR is 22');
});

test('AbortError code is 20', (t) => {
	const e = new DOMException('msg', 'AbortError');
	t.equal(e.code, 20, 'code is 20');
});

test('Unknown name gives code 0', (t) => {
	const e = new DOMException('msg', 'UnknownName');
	t.equal(e.code, 0, 'code is 0');
});

test('toString includes the name', (t) => {
	const e = new DOMException('msg', 'NotFoundError');
	t.ok(
		e.toString().includes('NotFoundError'),
		'toString includes NotFoundError',
	);
});
