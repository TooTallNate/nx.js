import { bold, cyan, green, magenta, red, yellow } from 'kleur/colors';

export const inspect = (v: unknown): string => {
	if (v && typeof (v as any)[inspect.custom] === 'function') {
		return (v as any)[inspect.custom]();
	}
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
		return `\x1b[38;2;100;100;100m${v}\x1b[39m`;
	}
	if (v === null) {
		return bold(String(v));
	}
	if (typeof v === 'function') {
		const { name } = v;
		return cyan(`[Function${name ? `: ${name}` : ' (anonymous)'}]`);
	}
	if (Array.isArray(v)) {
		const contents =
			v.length === 0 ? '' : ` ${v.map((e) => inspect(e)).join(', ')} `;
		return `[${contents}]`;
	}
	if (isRegExp(v)) {
		return bold(red(String(v)));
	}
	if (isDate(v)) {
		return magenta(v.toISOString());
	}
	if (isError(v)) {
		const stack = v.stack?.trim();
		const obj = Object.keys(v).length > 0 ? ` ${printObject(v)}` : '';
		return stack ? `${v}\n${stack}${obj}` : `[${v}]${obj}`;
	}
	if (isArrayBuffer(v)) {
		const contents = new Uint8Array(v);
		const b = [];
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
		const b = [];
		for (let i = 0; i < Math.min(50, v.length); i++) {
			b.push(inspect(v[i]));
		}
		let c = b.length === 0 ? '' : ` ${b.join(', ')} `;
		if (v.length > 50) c += '...';
		return `${getClass(v)}(${v.length}) [${c}]`;
	}
	if (typeof v === 'object') {
		return printObject(v);
	}
	return `? ${v}`;
};

inspect.custom = Symbol('Switch.inspect.custom');

function printObject(v: any) {
	const keys = Object.keys(v);
	const contents =
		keys.length === 0
			? ''
			: ` ${keys.map((k) => `${k}: ${inspect(v[k])}`).join(', ')} `;
	return `{${contents}}`;
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
