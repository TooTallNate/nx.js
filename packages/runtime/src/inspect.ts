import { bold, cyan, green, magenta, red, rgb, yellow } from 'kleur/colors';
import { $ } from './$';

const grey = rgb(100, 100, 100);

export interface InspectOptions {
	depth?: number;
	refs?: Map<{}, number>;
}

/**
 * Inspects a given value and returns a string representation of it.
 * The function uses ANSI color codes to highlight different parts of the output.
 * It can handle and correctly output different types of values including primitives, functions, arrays, and objects.
 *
 * @param v - The value to inspect.
 * @param opts - Options which may modify the generated string representation of the value.
 * @returns A string representation of `v` with ANSI color codes.
 */
export const inspect = (v: unknown, opts: InspectOptions = {}): string => {
	const { depth = 1 } = opts;

	// Primitives
	if (typeof v === 'number' || typeof v === 'boolean') {
		return bold(yellow(v));
	}
	if (typeof v === 'bigint') {
		return bold(yellow(`${v}n`));
	}
	if (typeof v === 'symbol') {
		return green(String(v));
	}
	if (typeof v === 'string') {
		return green(JSON.stringify(v));
	}
	if (typeof v === 'undefined') {
		return grey(String(v));
	}
	if (v === null) {
		return bold(String(v));
	}

	// After this point, all should be typeof "object" or
	// "function", so they can have additional properties
	// which may be circular references.
	const refs = opts.refs ?? new Map();
	const refIndex = refs.get(v);
	if (typeof refIndex === 'number') {
		return cyan(`[Circular *${refIndex}]`);
	}

	refs.set(v, refs.size + 1);

	// If the value defines the `inspect.custom` symbol, then invoke
	// the function. If the return value is a string, return that.
	// Otherwise, treat the return value as the value to inspect
	if (v && typeof (v as any)[inspect.custom] === 'function') {
		const c = (v as any)[inspect.custom]({ ...opts, depth });
		if (typeof c === 'string') {
			return c;
		}
		v = c;
	}

	if (typeof v === 'function') {
		const { name } = v;
		if (String(v).startsWith('class')) {
			return cyan(`[class${name ? `: ${name}` : ''}]`);
		}
		return cyan(`[Function${name ? `: ${name}` : ' (anonymous)'}]`);
	}
	if (Array.isArray(v)) {
		const contents =
			v.length === 0
				? ''
				: ` ${v
						.map((e) => inspect(e, { ...opts, refs, depth }))
						.join(', ')} `;
		return `[${contents}]`;
	}
	if (isRegExp(v)) {
		return bold(red(String(v)));
	}
	if (isDate(v)) {
		return magenta(v.toISOString());
	}
	if (isPromise(v)) {
		let val = '';
		const [state, result] = $.getInternalPromiseState(v);
		if (state === 0) {
			val = cyan('<pending>');
		} else {
			if (state === 2) {
				val = `${red('<rejected>')} `;
			}
			val += inspect(result, { ...opts, refs, depth });
		}
		return `Promise { ${val} }`;
	}
	if (isError(v)) {
		const { stack } = v;
		const obj =
			Object.keys(v).length > 0
				? ` ${printObject(v, { ...opts, refs, depth })}`
				: '';
		return stack ? `${v}\n${stack.trimEnd()}${obj}` : `[${v}]${obj}`;
	}
	if (isArrayBuffer(v)) {
		const contents = new Uint8Array(v);
		const b: string[] = [];
		for (let i = 0; i < Math.min(50, v.byteLength); i++) {
			b.push(contents[i].toString(16).padStart(2, '0'));
		}
		let c = b.join(' ');
		if (contents.length > 50) c += '...';
		const len = inspect(v.byteLength);
		return `ArrayBuffer { ${cyan(
			'[Uint8Contents]'
		)}: <${c}>, byteLength: ${len} }`;
	}
	if (isTypedArray(v)) {
		const b: string[] = [];
		for (let i = 0; i < Math.min(50, v.length); i++) {
			b.push(inspect(v[i]));
		}
		let c = b.length === 0 ? '' : ` ${b.join(', ')} `;
		if (v.length > 50) c += '...';
		return `${getClass(v)}(${v.length}) [${c}]`;
	}
	if (typeof v === 'object') {
		return printObject(v, { ...opts, refs, depth });
	}
	return `? ${v}`;
};

inspect.custom = Symbol('Switch.inspect.custom');

function printObject(v: any, opts: InspectOptions) {
	const { depth = 1 } = opts;
	const keys = Object.keys(v);
	const ctor = v.constructor;
	const className =
		ctor && ctor !== Object && typeof ctor.name === 'string'
			? `${ctor.name} `
			: '';
	let contents = '';
	let endSpace = '';
	if (keys.length > 0) {
		let len = 0;
		const parts = keys.map((k) => {
			const l = `${k}: ${inspect(v[k], { ...opts, depth: depth + 1 })}`;
			len += l.length;
			return l;
		});
		if (len > 60) {
			const space = '  '.repeat(depth);
			contents = parts.map((p) => `\n${space}${p},`).join('') + '\n';
			endSpace = '  '.repeat(depth - 1);
		} else {
			contents = ` ${parts.join(', ')} `;
		}
	}
	return `${className}{${contents}${endSpace}}`;
}

function getClass(v: unknown) {
	return Object.prototype.toString.call(v).slice(8, -1);
}

function isDate(v: unknown): v is Date {
	return getClass(v) === 'Date';
}

function isError(v: unknown): v is Error {
	return getClass(v) === 'Error';
}

function isRegExp(v: unknown): v is RegExp {
	return getClass(v) === 'RegExp';
}

function isArrayBuffer(v: unknown): v is ArrayBuffer {
	return getClass(v) === 'ArrayBuffer';
}

function isTypedArray(v: unknown): v is ArrayLike<number> {
	return getClass(v).endsWith('Array');
}

function isPromise(v: unknown): v is Promise<unknown> {
	return getClass(v) === 'Promise';
}
