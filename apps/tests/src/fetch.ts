import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('fetch');

test('Request - no `body` for `GET`', () => {
	let err: Error | undefined;
	try {
		new Request('http://example.com', { body: 'bad' });
	} catch (_err: any) {
		err = _err;
	}
	assert.ok(err);
	assert.equal(err.name, 'TypeError');
	assert.equal(err.message, 'Body not allowed for GET or HEAD requests');
});

test.run();
