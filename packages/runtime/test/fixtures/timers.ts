import { test } from '../src/tap';

test('setTimeout fires', async (t) => {
	const result = await new Promise<string>((resolve) => {
		setTimeout(() => resolve('done'), 10);
	});
	t.equal(result, 'done', 'setTimeout callback ran');
});

test('setTimeout ordering', async (t) => {
	const order: number[] = [];
	await new Promise<void>((resolve) => {
		setTimeout(() => {
			order.push(1);
		}, 50);
		setTimeout(() => {
			order.push(2);
		}, 100);
		setTimeout(() => {
			order.push(3);
			resolve();
		}, 200);
	});
	t.deepEqual(order, [1, 2, 3], 'timers fire in delay order');
});

test('clearTimeout cancels', async (t) => {
	let fired = false;
	const id = setTimeout(() => {
		fired = true;
	}, 10);
	clearTimeout(id);
	await new Promise<void>((resolve) => setTimeout(resolve, 50));
	t.notOk(fired, 'cleared timer did not fire');
});

test('setTimeout with zero delay', async (t) => {
	const result = await new Promise<string>((resolve) => {
		setTimeout(() => resolve('immediate'), 0);
	});
	t.equal(result, 'immediate', 'zero-delay timer fires');
});

test('setInterval fires multiple times', async (t) => {
	let count = 0;
	await new Promise<void>((resolve) => {
		const id = setInterval(() => {
			count++;
			if (count >= 3) {
				clearInterval(id);
				resolve();
			}
		}, 10);
	});
	t.equal(count, 3, 'interval fired 3 times');
});

test('clearInterval stops', async (t) => {
	let count = 0;
	const id = setInterval(() => {
		count++;
	}, 10);
	await new Promise<void>((resolve) => setTimeout(resolve, 35));
	clearInterval(id);
	const countAfterClear = count;
	await new Promise<void>((resolve) => setTimeout(resolve, 30));
	t.equal(count, countAfterClear, 'no more fires after clearInterval');
});
