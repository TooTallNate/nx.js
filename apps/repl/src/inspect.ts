import { bold, cyan, green, magenta, red, yellow } from 'kleur/colors';

export function inspect(v: unknown): string {
	if (typeof v === 'number' || typeof v === 'boolean') {
		return bold(yellow(String(v)));
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
	if (typeof v === 'undefined' || v === null) {
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
		const keys = Object.keys(v);
		const contents =
			keys.length === 0
				? ''
				: ` ${keys
						.map((k) => `${k}: ${inspect((v as any)[k])}`)
						.join(', ')} `;
		return `{${contents}}`;
	}
	return `? ${v}`;
}

function getClass(v: unknown) {
	return Object.prototype.toString.call(v).slice(8, -1);
}

function isDate(v: unknown): v is Date {
	return getClass(v) === 'Date';
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
