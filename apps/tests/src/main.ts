import { assert, test } from './runner';

test('`Switch.entrypoint` is a string', () => {
	assert(typeof Switch.entrypoint === 'string');
	assert(Switch.entrypoint.length > 0);
});

test('`Switch.cwd()` is a URL instance', () => {
	const cwd = Switch.cwd();
	assert(cwd instanceof URL);
	assert(cwd.href.startsWith('sdmc:/'));
	assert(cwd.href.endsWith('/'));
});

test('`Switch.readDirSync()` works on relative path', () => {
	const files = Switch.readDirSync('.');
	assert(Array.isArray(files));
	assert(files.length > 0);
});

test('`Switch.readDirSync()` works on `sdmc:/` path', () => {
	const files = Switch.readDirSync('sdmc:/');
	assert(Array.isArray(files));
	assert(files.length > 0);
});

test('`Switch.readDirSync()` works on `romfs:/` path', () => {
	const files = Switch.readDirSync('romfs:/');
	assert(Array.isArray(files));
	assert(files.length > 0);
});

test('`Switch.readDirSync()` throws error upon invalid path', () => {
	let err: Error | undefined;
	try {
		Switch.readDirSync('romfs:/__does_not_exist__');
	} catch (_err) {
		err = _err as Error;
	}
	assert(err);
});

test('`Switch.resolveDns()` works', async () => {
	const result = await Switch.resolveDns('n8.io');
	assert(Array.isArray(result));
	assert(result.length > 0);
});

test('`Switch.resolveDns()` rejects with invalid hostname', async () => {
	let err: Error | undefined;
	try {
		await Switch.resolveDns('example-that-definitely-does-not-exist.nxjs');
	} catch (_err) {
		err = _err as Error;
	}
	assert(err);
});

test('`Switch.readFile()` works', async () => {
	const data = await Switch.readFile('romfs:/runtime.js');
	assert(data instanceof ArrayBuffer);
	assert(data.byteLength > 0);
});
