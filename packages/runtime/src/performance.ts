import { INTERNAL_SYMBOL } from './internal';
import { assertInternalConstructor, def } from './utils';
import type { DOMHighResTimeStamp } from './types';

/**
 * @see https://developer.mozilla.org/docs/Web/API/PerformanceEntry
 */
export class PerformanceEntry {
	readonly name: string;
	readonly entryType: string;
	readonly startTime: DOMHighResTimeStamp;
	readonly duration: DOMHighResTimeStamp;

	/** @ignore */
	constructor() {
		assertInternalConstructor(arguments);
		this.name = arguments[1];
		this.entryType = arguments[2];
		this.startTime = arguments[3];
		this.duration = arguments[4];
	}

	toJSON() {
		return {
			name: this.name,
			entryType: this.entryType,
			startTime: this.startTime,
			duration: this.duration,
		};
	}
}
def(PerformanceEntry);

/**
 * @see https://developer.mozilla.org/docs/Web/API/PerformanceMark
 */
export class PerformanceMark extends PerformanceEntry {
	readonly detail: unknown;

	/** @ignore */
	constructor() {
		assertInternalConstructor(arguments);
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL, arguments[1], 'mark', arguments[2], 0);
		this.detail = arguments[3] ?? null;
	}

	toJSON() {
		return {
			...super.toJSON(),
			detail: this.detail,
		};
	}
}
def(PerformanceMark);

/**
 * @see https://developer.mozilla.org/docs/Web/API/PerformanceMeasure
 */
export class PerformanceMeasure extends PerformanceEntry {
	readonly detail: unknown;

	/** @ignore */
	constructor() {
		assertInternalConstructor(arguments);
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL, arguments[1], 'measure', arguments[2], arguments[3]);
		this.detail = arguments[4] ?? null;
	}

	toJSON() {
		return {
			...super.toJSON(),
			detail: this.detail,
		};
	}
}
def(PerformanceMeasure);

/**
 * Note: `performance.now()` currently uses `Date.now()` internally, providing
 * millisecond resolution. A future version may use the Switch's `svcGetSystemTick`
 * for sub-millisecond precision once a native binding is available.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Performance_API
 */
export class Performance {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/timeOrigin
	 */
	readonly timeOrigin: DOMHighResTimeStamp;

	/** @internal */
	private _entries: PerformanceEntry[] = [];

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		this.timeOrigin = Date.now();
	}

	/**
	 * Returns the number of milliseconds elapsed since `timeOrigin`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/now
	 */
	now(): DOMHighResTimeStamp {
		return Date.now() - this.timeOrigin;
	}

	/**
	 * Creates a named `PerformanceMark` in the performance timeline.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/mark
	 */
	mark(
		markName: string,
		markOptions?: { startTime?: DOMHighResTimeStamp; detail?: unknown },
	): PerformanceMark {
		const startTime = markOptions?.startTime ?? this.now();
		const mark = new (
			PerformanceMark as new (
				...args: unknown[]
			) => PerformanceMark
		)(INTERNAL_SYMBOL, markName, startTime, markOptions?.detail);
		this._entries.push(mark);
		return mark;
	}

	/**
	 * Creates a named `PerformanceMeasure` between two marks or timestamps.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/measure
	 */
	measure(
		measureName: string,
		startOrMeasureOptions?:
			| string
			| {
					start?: string | DOMHighResTimeStamp;
					end?: string | DOMHighResTimeStamp;
					duration?: DOMHighResTimeStamp;
					detail?: unknown;
			  },
		endMark?: string,
	): PerformanceMeasure {
		let startTime: DOMHighResTimeStamp;
		let endTime: DOMHighResTimeStamp;
		let detail: unknown;

		if (
			typeof startOrMeasureOptions === 'object' &&
			startOrMeasureOptions !== null
		) {
			const opts = startOrMeasureOptions;
			detail = opts.detail;
			startTime = this._resolveTimestamp(opts.start ?? 0);
			if (opts.duration !== undefined) {
				if (opts.end !== undefined) {
					throw new TypeError(
						"Cannot specify both 'duration' and 'end' in measure options.",
					);
				}
				endTime = startTime + opts.duration;
			} else {
				endTime = this._resolveTimestamp(opts.end ?? this.now());
			}
		} else {
			startTime =
				startOrMeasureOptions !== undefined
					? this._resolveTimestamp(startOrMeasureOptions)
					: 0;
			endTime =
				endMark !== undefined ? this._resolveTimestamp(endMark) : this.now();
		}

		const duration = endTime - startTime;
		const measure = new (
			PerformanceMeasure as new (
				...args: unknown[]
			) => PerformanceMeasure
		)(INTERNAL_SYMBOL, measureName, startTime, duration, detail);
		this._entries.push(measure);
		return measure;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/getEntries
	 */
	getEntries(): PerformanceEntry[] {
		return [...this._entries].sort((a, b) => a.startTime - b.startTime);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/getEntriesByName
	 */
	getEntriesByName(name: string, type?: string): PerformanceEntry[] {
		return this.getEntries().filter(
			(e) => e.name === name && (type === undefined || e.entryType === type),
		);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/getEntriesByType
	 */
	getEntriesByType(type: string): PerformanceEntry[] {
		return this.getEntries().filter((e) => e.entryType === type);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/clearMarks
	 */
	clearMarks(markName?: string): void {
		this._entries = this._entries.filter(
			(e) =>
				e.entryType !== 'mark' ||
				(markName !== undefined && e.name !== markName),
		);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/clearMeasures
	 */
	clearMeasures(measureName?: string): void {
		this._entries = this._entries.filter(
			(e) =>
				e.entryType !== 'measure' ||
				(measureName !== undefined && e.name !== measureName),
		);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/toJSON
	 */
	toJSON() {
		return {
			timeOrigin: this.timeOrigin,
		};
	}

	/** @internal */
	private _resolveTimestamp(
		value: string | DOMHighResTimeStamp,
	): DOMHighResTimeStamp {
		if (typeof value === 'number') {
			return value;
		}
		// Look up mark by name
		const entries = this._entries.filter(
			(e) => e.entryType === 'mark' && e.name === value,
		);
		if (entries.length === 0) {
			throw new DOMException(
				`Failed to execute 'measure' on 'Performance': The mark '${value}' does not exist.`,
				'SyntaxError',
			);
		}
		return entries[entries.length - 1].startTime;
	}
}
def(Performance);

// @ts-expect-error Internal constructor
export var performance = new Performance(INTERNAL_SYMBOL);
def(performance, 'performance');
