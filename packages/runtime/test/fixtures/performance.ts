import { test } from '../src/tap';

// --- performance.now() ---

test('performance.now() returns a number > 0', (t) => {
	const now = performance.now();
	t.ok(typeof now === 'number', 'is a number');
	t.ok(now > 0, 'is greater than 0');
});

test('performance.now() is monotonically increasing', (t) => {
	const a = performance.now();
	// Busy-wait briefly to ensure time passes
	for (let i = 0; i < 10000; i++) { /* noop */ }
	const b = performance.now();
	t.ok(b >= a, 'second call >= first call');
});

// --- performance.mark() ---

test('performance.mark() creates a mark retrievable via getEntriesByType', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	const mark = performance.mark('test-mark-1');
	const marks = performance.getEntriesByType('mark');
	const found = marks.find((e) => e.name === 'test-mark-1');
	t.ok(found !== undefined, 'mark found in getEntriesByType("mark")');
	t.equal(found!.entryType, 'mark', 'entryType is "mark"');
	performance.clearMarks();
});

test('performance.mark() with detail option', (t) => {
	performance.clearMarks();
	const mark = performance.mark('detail-mark', { detail: { foo: 'bar' } });
	t.equal(mark.name, 'detail-mark', 'mark name matches');
	t.equal((mark as any).detail?.foo, 'bar', 'detail is preserved');
	performance.clearMarks();
});

// --- performance.measure() ---

test('performance.measure() between two marks', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('m-start');
	for (let i = 0; i < 10000; i++) { /* noop */ }
	performance.mark('m-end');
	const measure = performance.measure('my-measure', 'm-start', 'm-end');
	t.equal(measure.name, 'my-measure', 'measure name matches');
	t.equal(measure.entryType, 'measure', 'entryType is "measure"');
	t.ok(measure.duration >= 0, 'duration >= 0');
	performance.clearMarks();
	performance.clearMeasures();
});

test('performance.measure() with options object', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('opt-start');
	performance.mark('opt-end');
	const measure = performance.measure('opt-measure', {
		start: 'opt-start',
		end: 'opt-end',
	});
	t.equal(measure.name, 'opt-measure', 'measure name matches');
	t.ok(measure.duration >= 0, 'duration >= 0');
	performance.clearMarks();
	performance.clearMeasures();
});

// --- performance.getEntries() / getEntriesByName() ---

test('performance.getEntries() returns all entries', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('entry-a');
	performance.mark('entry-b');
	performance.measure('entry-m', 'entry-a', 'entry-b');
	const entries = performance.getEntries();
	const names = entries.map((e) => e.name);
	t.ok(names.includes('entry-a'), 'contains mark entry-a');
	t.ok(names.includes('entry-b'), 'contains mark entry-b');
	t.ok(names.includes('entry-m'), 'contains measure entry-m');
	performance.clearMarks();
	performance.clearMeasures();
});

test('performance.getEntriesByName() filters by name', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('unique-name');
	performance.mark('other-name');
	const filtered = performance.getEntriesByName('unique-name');
	t.equal(filtered.length, 1, 'exactly one entry');
	t.equal(filtered[0].name, 'unique-name', 'name matches');
	performance.clearMarks();
});

// --- performance.clearMarks() ---

test('performance.clearMarks() removes all marks', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('cm-1');
	performance.mark('cm-2');
	performance.clearMarks();
	const marks = performance.getEntriesByType('mark');
	t.equal(marks.length, 0, 'no marks remain');
});

test('performance.clearMarks(name) removes only named mark', (t) => {
	performance.clearMarks();
	performance.mark('keep-me');
	performance.mark('remove-me');
	performance.clearMarks('remove-me');
	const marks = performance.getEntriesByType('mark');
	t.equal(marks.length, 1, 'one mark remains');
	t.equal(marks[0].name, 'keep-me', 'correct mark kept');
	performance.clearMarks();
});

// --- performance.clearMeasures() ---

test('performance.clearMeasures() removes measures', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('cm-a');
	performance.mark('cm-b');
	performance.measure('cm-m', 'cm-a', 'cm-b');
	performance.clearMeasures();
	const measures = performance.getEntriesByType('measure');
	t.equal(measures.length, 0, 'no measures remain');
	performance.clearMarks();
});

// --- performance.toJSON() ---

test('performance.toJSON() returns object with timeOrigin', (t) => {
	const json = performance.toJSON();
	t.ok(typeof json === 'object', 'returns an object');
	t.ok(typeof json.timeOrigin === 'number', 'has numeric timeOrigin');
	t.ok(json.timeOrigin > 0, 'timeOrigin > 0');
});

// --- PerformanceMark entryType ---

test('PerformanceMark has correct entryType', (t) => {
	performance.clearMarks();
	const mark = performance.mark('type-check-mark');
	t.equal(mark.entryType, 'mark', 'entryType is "mark"');
	t.equal(mark.name, 'type-check-mark', 'name matches');
	t.ok(typeof mark.startTime === 'number', 'has numeric startTime');
	t.equal(mark.duration, 0, 'mark duration is 0');
	performance.clearMarks();
});

// --- PerformanceMeasure entryType and duration ---

test('PerformanceMeasure has correct entryType and duration', (t) => {
	performance.clearMarks();
	performance.clearMeasures();
	performance.mark('dur-start');
	for (let i = 0; i < 10000; i++) { /* noop */ }
	performance.mark('dur-end');
	const measure = performance.measure('dur-measure', 'dur-start', 'dur-end');
	t.equal(measure.entryType, 'measure', 'entryType is "measure"');
	t.ok(typeof measure.duration === 'number', 'duration is a number');
	t.ok(measure.duration >= 0, 'duration >= 0');
	t.ok(typeof measure.startTime === 'number', 'has numeric startTime');
	performance.clearMarks();
	performance.clearMeasures();
});
