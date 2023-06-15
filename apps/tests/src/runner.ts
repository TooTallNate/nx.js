import { green, rgb } from 'kleur/colors';

type Test = () => void | Promise<void>;

let hasOnly = false;
const grey = rgb(100, 100, 100);
const tests: [name: string, test: Test, skipped: boolean][] = [];

export function assert(val: any): asserts val {
	if (!val) {
		throw new Error('Assertion failed');
	}
}

export function test(name: string, fn: Test) {
	if (!hasOnly) {
		tests.push([name, fn, false]);
	}
}

test.skip = function (name: string, fn: Test) {
	if (!hasOnly) {
		tests.push([name, fn, true]);
	}
};

test.only = function (name: string, fn: Test) {
	if (!hasOnly) {
		hasOnly = true;
		tests.length = 0;
	}
	tests.push([name, fn, false]);
};

async function runNextTest() {
	const next = tests.shift();
	if (!next) {
		console.log('Done!');
		return;
	}
	const [name, test, skipped] = next;
	if (skipped) {
		console.log(grey(`SKIP ${name}`));
	} else {
		try {
			await Promise.resolve().then(() => test());
			console.log(`${green('PASS')} ${name}`);
		} catch (err: unknown) {
			console.error(`FAIL ${name}`);
			console.error(err);
		}
	}
	return runNextTest();
}

// Begin tests after first tick
setTimeout(runNextTest, 0);
