import { ENOENT } from '@nx.js/constants';
import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Switch');

test('Switch namespace', () => {
	const desc = Object.getOwnPropertyDescriptor(globalThis, 'Switch')!;
	assert.equal(desc.writable, true);
	assert.equal(desc.enumerable, false);
	assert.equal(desc.configurable, true);
	assert.equal(Object.prototype.toString.call(Switch), '[object Switch]');
});

test('`Switch.entrypoint` is a string', () => {
	assert.type(Switch.entrypoint, 'string');
	assert.ok(Switch.entrypoint.length > 0);
});

test('`Switch.cwd()` is a URL string representation', async () => {
	const cwd = Switch.cwd();
	assert.type(cwd, 'string');
	assert.ok(cwd.startsWith('sdmc:/'));
	assert.ok(cwd.endsWith('/'));
});

test('`Switch.chdir()` can change directory to romfs:/', async () => {
	const cwd = Switch.cwd();
	try {
		Switch.chdir('romfs:/');
		assert.equal(Switch.cwd(), 'romfs:/');
	} finally {
		Switch.chdir(cwd);
	}
});

test('`Switch.readDirSync()` works on relative path', () => {
	const files = Switch.readDirSync('.');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readDirSync()` works on `sdmc:/` path', () => {
	const files = Switch.readDirSync('sdmc:/');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readDirSync()` works on `romfs:/` path', () => {
	const files = Switch.readDirSync('romfs:/');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readFileSync()` returns `null` when file does not exist', () => {
	assert.equal(Switch.readFileSync('romfs:/__does_not_exist__'), null);
});

test('`Switch.readDirSync()` returns `null` when directory does not exist', () => {
	assert.equal(Switch.readDirSync('romfs:/__does_not_exist__'), null);
});

test('`Switch.statSync()` returns `null` when file does not exist', () => {
	assert.equal(Switch.statSync('romfs:/__does_not_exist__'), null);
});

test('`Switch.resolveDns()` works', async () => {
	const result = await Switch.resolveDns('n8.io');
	assert.ok(Array.isArray(result));
	assert.ok(result.length > 0);
});

test('`Switch.resolveDns()` rejects with invalid hostname', async () => {
	let err: Error | undefined;
	try {
		await Switch.resolveDns('example-that-definitely-does-not-exist.nxjs');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`Switch.readFile()` works with string path', async () => {
	const data = await Switch.readFile('romfs:/runtime.js.map');
	assert.ok(data);
	assert.instance(data, ArrayBuffer);
	assert.ok(data.byteLength > 0);
});

test('`Switch.readFile()` works with URL path', async () => {
	const data = await Switch.readFile(new URL(Switch.entrypoint));
	assert.ok(data);
	assert.instance(data, ArrayBuffer);
	assert.ok(data.byteLength > 0);
});

test('`Switch.readFile()` returns null when file does not exist', async () => {
	const data = await Switch.readFile('romfs:/__does_not_exist__');
	assert.equal(data, null);
});

test('`Switch.readFile()` returns null when attempting to read a directory', async () => {
	const data = await Switch.readFile('.');
	assert.equal(data, null);
});

test('`Switch.readFile()` works with `start` and `end` options', async () => {
	const data = await Switch.readFile('romfs:/runtime.js.map', {
		start: 10,
		end: 20,
	});
	assert.ok(data);
	assert.instance(data, ArrayBuffer);
	assert.equal(data.byteLength, 10);
	assert.equal(new TextDecoder().decode(data), 'on": 3,\n  ');
});

test('`Switch.readFileSync()` works with `start` and `end` options', async () => {
	const data = Switch.readFileSync('romfs:/runtime.js.map', {
		start: 10,
		end: 20,
	});
	assert.ok(data);
	assert.instance(data, ArrayBuffer);
	assert.equal(data.byteLength, 10);
	assert.equal(new TextDecoder().decode(data), 'on": 3,\n  ');
});

test('`Switch.stat()` returns file information', async () => {
	const stat = await Switch.stat(Switch.entrypoint);
	assert.ok(stat);
	assert.ok(stat.size > 0);
});

test('`Switch.stat()` returns `null` when file does not exist', async () => {
	const stat = await Switch.stat('romfs:/__does_not_exist__');
	assert.equal(stat, null);
});

test("`Switch.removeSync()` doesn't throw when path does not exist", async () => {
	const path = 'sdmc:/__does_not_exist';
	Switch.removeSync(path);
	assert.ok(true);
});

test('`Switch.removeSync()` removes file', async () => {
	const path = 'sdmc:/__nxjs-test.txt';
	const uuid = crypto.randomUUID();
	Switch.writeFileSync(path, uuid);
	const data = Switch.readFileSync(path);
	assert.ok(data);
	assert.equal(new TextDecoder().decode(data), uuid);

	Switch.removeSync(path);
	assert.equal(Switch.statSync(path), null);
});

test('`Switch.removeSync()` removes directory', async () => {
	const path = 'sdmc:/__nxjs-test';
	Switch.mkdirSync(path);
	assert.ok(Switch.statSync(path));

	Switch.removeSync(path);
	assert.equal(Switch.statSync(path), null);
});

test('`Switch.removeSync()` removes nested directory', async () => {
	const dir = 'sdmc:/__nested';
	const path = `${dir}/another/nxjs-test/file.txt`;
	Switch.writeFileSync(path, 'hello world');
	assert.ok(Switch.statSync(dir));
	assert.ok(Switch.statSync(path));

	Switch.removeSync(dir);
	assert.equal(Switch.statSync(dir), null);
	assert.equal(Switch.statSync(path), null);
});

test('`Switch.file()` read text', async () => {
	const file = Switch.file('romfs:/file.txt');
	const expected = 'this is a text file\n';
	const data = await file.text();
	assert.equal(data.length, expected.length);
	assert.equal(data, expected);
});

test('`Switch.slice()` recursively slices a file', async () => {
	const file = Switch.file('romfs:/file.txt');
	assert.instance(file, Switch.FsFile);
	assert.equal(file.size, 20);

	{
		// Slice with start and end
		const slice = file.slice(1, 10);
		assert.instance(slice, Switch.FsFile);
		assert.equal(slice.start, 1);
		assert.equal(slice.end, 10);
		assert.equal(slice.size, 9);

		const data = await slice.text();
		assert.equal(data.length, 9);
		assert.equal(data, 'his is a ');
	}

	{
		// Slice with no end
		const slice = file.slice(5);
		assert.instance(slice, Switch.FsFile);
		assert.equal(slice.start, 5);
		assert.equal(slice.end, Infinity);
		assert.equal(slice.size, 15);

		const data = await slice.text();
		assert.equal(data.length, 15);
		assert.equal(data, 'is a text file\n');
	}

	{
		// Inner slice with start and end
		const slice = file.slice(1, 10).slice(2, 5);
		assert.instance(slice, Switch.FsFile);
		assert.equal(slice.start, 3);
		assert.equal(slice.end, 6);
		assert.equal(slice.size, 3);

		const data = await slice.text();
		assert.equal(data.length, 3);
		assert.equal(data, 's i');
	}

	{
		// Inner slice with no end
		const slice = file.slice(1, 10).slice(2);
		assert.instance(slice, Switch.FsFile);
		assert.equal(slice.start, 3);
		assert.equal(slice.end, 10);
		assert.equal(slice.size, 7);

		const data = await slice.text();
		assert.equal(data.length, 7);
		assert.equal(data, 's is a ');
	}

	{
		// Inner slice with no end on both
		const slice = file.slice(5).slice(2);
		assert.instance(slice, Switch.FsFile);
		assert.equal(slice.start, 7);
		assert.equal(slice.end, Infinity);
		assert.equal(slice.size, 13);

		const data = await slice.text();
		assert.equal(data.length, 13);
		assert.equal(data, ' a text file\n');
	}
});

test('`Switch.file()` read json', async () => {
	const file = Switch.file('romfs:/file.json');
	const data = await file.json();
	assert.equal(data, { foo: 'bar' });
});

test('`Switch.file()` read stream', async () => {
	const file = Switch.file('romfs:/file.txt');
	const chunks: string[] = [];
	const decoder = new TextDecoder();
	for await (const chunk of file.stream({ chunkSize: 3 })) {
		chunks.push(decoder.decode(chunk));
	}
	assert.equal(chunks, ['thi', 's i', 's a', ' te', 'xt ', 'fil', 'e\n']);
});

test('`Switch.file()` read stream with slice', async () => {
	const file = Switch.file('romfs:/file.txt').slice(1, 11);
	const chunks: string[] = [];
	const decoder = new TextDecoder();
	for await (const chunk of file.stream({ chunkSize: 3 })) {
		chunks.push(decoder.decode(chunk));
	}
	assert.equal(chunks, ['his', ' is', ' a ', 't']);
	assert.equal(chunks.join('').length, 10);
});

test('`Switch.file()` read stream throws on ENOENT', async () => {
	let err: Error | undefined;
	const file = Switch.file('romfs:/does_not_exist');
	try {
		for await (const _ of file.stream()) {
		}
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
	assert.equal((err as any).errno, ENOENT);
});

test('`Switch.file()` write stream', async () => {
	const path = 'sdmc:/nxjs-test-file.txt';
	try {
		const file = Switch.file(path);

		const writer = file.writable.getWriter();
		writer.write('write');
		writer.write(' a ');
		writer.write('file ');
		writer.write('streaming');
		await writer.close();

		assert.equal(await file.text(), 'write a file streaming');
	} finally {
		await Switch.remove(path);
	}
});

test('`Switch.file()` creates parent directories', async () => {
	const dir = 'sdmc:/__nxjs_test__/';

	// Ensure directory does not exist
	assert.equal(Switch.statSync(dir), null);

	const filePath = new URL('test/file.txt', dir);
	try {
		const file = Switch.file(filePath);

		const writer = file.writable.getWriter();
		writer.write('a');
		await writer.close();

		// Ensure directory exists
		assert.ok(Switch.statSync(dir));
	} finally {
		await Switch.remove(dir);
	}
});

test.run();
