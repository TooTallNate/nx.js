import {
	readDirSync,
	readFileSync,
	removeSync,
	statSync,
	writeFileSync,
} from './fs';
import { console } from './console';
import { Application } from './switch/ns';
import { INTERNAL_SYMBOL } from './internal';
import { currentProfile } from './switch/profile';
import { assertInternalConstructor, createInternal, def } from './utils';
import type { FsDev } from './switch/fsdev';

interface StorageImpl {
	clear(): void;
	getItem(key: string): string | null;
	key(index: number): string | null;
	removeItem(key: string): void;
	setItem(key: string, value: string): void;
	length(): number;
}

const _ = createInternal<Storage, StorageImpl>();

/**
 * @see https://developer.mozilla.org/docs/Web/API/Storage
 */
export class Storage implements globalThis.Storage {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		_.set(this, arguments[1]);
	}
	[name: string]: any;

	/**
	 * The number of data items stored in the data store.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/length
	 */
	get length() {
		return _(this).length();
	}

	/**
	 * Clears all keys stored in a data store.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/clear
	 */
	clear(): void {
		_(this).clear();
	}

	/**
	 * Get an item from the provided key from the data store.
	 *
	 * @param key The name of the key you want to retrieve the value of.
	 * @returns A string containing the value of the key. If the key does not exist, `null` is returned.
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/getItem
	 */
	getItem(key: string): string | null {
		return _(this).getItem(key);
	}

	/**
	 * Retrieves the name of the nth key in the data store. The order
	 * of keys is user-agent defined, so you should not rely on it.
	 *
	 * @param index The number of the key you want to get the name of. This is a zero-based index.
	 * @returns  A string containing the name of the key. If the index does not exist, `null` is returned.
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/key
	 */
	key(index: number): string | null {
		return _(this).key(index);
	}

	/**
	 * Removes the provided key from the data store, if it exists.
	 *
	 * If there is no item associated with the given key, this method will do nothing.
	 *
	 * @param key The name of the key you want to remove.
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/removeItem
	 */
	removeItem(key: string): void {
		_(this).removeItem(key);
	}

	/**
	 * Adds or updates the key to the data store with the provided value.
	 *
	 * @param key The name of the key you want to create / update.
	 * @param value The value you want to give the key you are creating / updating.
	 * @see https://developer.mozilla.org/docs/Web/API/Storage/setItem
	 */
	setItem(key: string, value: string): void {
		_(this).setItem(key, value);
	}
}
def(Storage);

/**
 * `localStorage` is a {@link Storage} instance which persists to the system's Save Data filesystem.
 *
 * When `localStorage` is accessed for the first time, the currently selected user profile is checked
 * to determine the profile which the data will be stored for. If no user profile is currently selected,
 * then the profile selection interface is shown.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/localStorage
 */
export declare var localStorage: Storage;

Object.defineProperty(globalThis, 'localStorage', {
	enumerable: true,
	configurable: true,
	get() {
		let nacp = readFileSync('romfs:/.nacp');
		if (!nacp) {
			console.debug(
				`No NACP file found in romfs. Using default values. Please set "titleId" property in \`package.json\``
			);
			nacp = new ArrayBuffer(16384);
			const view = new DataView(nacp);
			view.setBigUint64(0x3078, 0x016de8dbc0c70000n, true); // SaveDataOwnerId
			view.setBigUint64(0x3080, 0x3e00000n, true); // UserAccountSaveDataSize
			view.setBigUint64(0x3088, 0x180000n, true); // UserAccountSaveDataJournalSize
		}
		const app = Object.create(Application.prototype) as Application;
		app.nacp = nacp;

		const profile = currentProfile({ required: true });
		if (!profile) {
			throw new Error(
				'`localStorage` could not be initialized because a user was not selected'
			);
		}

		const name = 'localstorage';
		const base = `${name}:/localStorage/`;

		let dev: FsDev;
		try {
			dev = app.mountSaveData(name, profile);
		} catch (err: any) {
			if (err.description !== 1002) throw err;
			app.createSaveData(profile);
			dev = app.mountSaveData(name, profile);
		}

		const impl: StorageImpl = {
			clear() {
				removeSync(base);
			},
			getItem(key: string): string | null {
				const b = readFileSync(new URL(key, base));
				if (!b) return null;
				return new TextDecoder().decode(b);
			},
			key(index: number): string | null {
				const keys = readDirSync(base) || [];
				return keys[index] ?? null;
			},
			removeItem(key: string): void {
				removeSync(new URL(key, base));
				dev.commit();
			},
			setItem(key: string, value: string): void {
				writeFileSync(new URL(key, base), value);
				dev.commit();
			},
			length(): number {
				const keys = readDirSync(base);
				return keys ? keys.length : 0;
			},
		};
		// @ts-expect-error internal constructor
		const storage = new Storage(INTERNAL_SYMBOL, impl);
		const proxy = new Proxy(storage, {
			has(_, p) {
				if (typeof p !== 'string') return false;
				return statSync(new URL(p, base)) !== null;
			},
			get(target, p) {
				if (typeof p !== 'string') return undefined;
				if (typeof target[p] !== 'undefined') {
					return target[p];
				}
				return target.getItem(p);
			},
			set(target, p, newValue) {
				if (typeof p !== 'string') return false;
				if (typeof target[p] !== 'undefined') {
					return false;
				}
				target.setItem(p, newValue);
				return true;
			},
			deleteProperty(target, p) {
				if (typeof p !== 'string') return false;
				target.removeItem(p);
				return true;
			},
			ownKeys() {
				return readDirSync(base) || [];
			},
			getOwnPropertyDescriptor(target, p) {
				if (typeof p !== 'string') return;
				if (typeof target[p] !== 'undefined') return;
				const v = this.get!(target, p, target);
				if (typeof v === 'undefined') return;
				return {
					enumerable: true,
					configurable: true,
					writable: true,
					value: v,
				};
			},
		});
		_.set(proxy, impl);
		Object.defineProperty(globalThis, 'localStorage', { value: proxy });
		return proxy;
	},
});
