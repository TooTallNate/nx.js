import { $ } from './$';
import { INTERNAL_SYMBOL } from './internal';
import {
	assertInternalConstructor,
	bufferSourceToArrayBuffer,
	createInternal,
	def,
	toPromise,
} from './utils';
import type { ArrayBufferView, BufferSource } from './types';

export interface Algorithm {
	name: string;
}

export type AlgorithmIdentifier = Algorithm | string;

interface CryptoInternal {
	subtle?: SubtleCrypto;
}

const _ = createInternal<Crypto, CryptoInternal>();

/**
 * Basic cryptography features available in the current context.
 * It allows access to a cryptographically strong random number
 * generator and to cryptographic primitives.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Crypto
 */
export class Crypto implements globalThis.Crypto {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		_.set(this, {});
	}

	/**
	 * A {@link SubtleCrypto | `SubtleCrypto`} which can be
	 * used to perform low-level cryptographic operations.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Crypto/subtle
	 */
	get subtle(): SubtleCrypto {
		let i = _(this);
		if (!i.subtle) {
			// @ts-expect-error Internal constructor
			i.subtle = new SubtleCrypto(INTERNAL_SYMBOL);
		}
		return i.subtle;
	}

	/**
	 * Fills the provided `TypedArray` with cryptographically strong random values.
	 *
	 * @example
	 *
	 * ```typescript
	 * const array = new Uint32Array(10);
	 * crypto.getRandomValues(array);
	 *
	 * console.log("Your lucky numbers:");
	 * for (const num of array) {
	 *   console.log(num);
	 * }
	 * ```
	 *
	 * @param array The `TypedArray` to fill with random values.
	 * @returns The same `TypedArray` filled with random values.
	 * @see https://developer.mozilla.org/docs/Web/API/Crypto/getRandomValues
	 */
	getRandomValues<T extends ArrayBufferView | null>(array: T): T {
		if (array) {
			$.cryptoRandomBytes(
				array.buffer,
				array.byteOffset,
				array.byteLength
			);
		}
		return array;
	}

	/**
	 * Generates a cryptographically strong random unique identifier (UUID).
	 *
	 * @example
	 *
	 * ```typescript
	 * const uuid = crypto.randomUUID();
	 * console.log(uuid);
	 * // "36b8f84d-df4e-4d49-b662-bcde71a8764f"
	 * ```
	 *
	 * @returns A string representation of a UUID.
	 * @see https://developer.mozilla.org/docs/Web/API/Crypto/randomUUID
	 */
	randomUUID(): `${string}-${string}-${string}-${string}-${string}` {
		let i = 0;
		const bytes = new Uint8Array(31);
		this.getRandomValues(bytes);
		return '10000000-1000-4000-8000-100000000000'.replace(/[018]/g, (v) => {
			const c = +v;
			return (c ^ (bytes[i++] & (15 >> (c / 4)))).toString(16);
		}) as `${string}-${string}-${string}-${string}-${string}`;
	}
}
def(Crypto);

/**
 * The global `crypto` property returns the {@link Crypto} object associated to the global object.
 * This object allows your application to access to certain cryptographic related services.
 *
 * @see https://developer.mozilla.org/docs/Web/API/crypto_property
 */
// @ts-expect-error Internal constructor
export var crypto = new Crypto(INTERNAL_SYMBOL);
def(crypto, 'crypto');

export class SubtleCrypto implements globalThis.SubtleCrypto {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	decrypt(
		algorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams,
		key: CryptoKey,
		data: BufferSource
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
	deriveBits(
		algorithm:
			| AlgorithmIdentifier
			| EcdhKeyDeriveParams
			| HkdfParams
			| Pbkdf2Params,
		baseKey: CryptoKey,
		length: number
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
	deriveKey(
		algorithm:
			| AlgorithmIdentifier
			| EcdhKeyDeriveParams
			| HkdfParams
			| Pbkdf2Params,
		baseKey: CryptoKey,
		derivedKeyType:
			| AlgorithmIdentifier
			| HkdfParams
			| Pbkdf2Params
			| AesDerivedKeyParams
			| HmacImportParams,
		extractable: boolean,
		keyUsages: KeyUsage[]
	): Promise<CryptoKey>;
	deriveKey(
		algorithm:
			| AlgorithmIdentifier
			| EcdhKeyDeriveParams
			| HkdfParams
			| Pbkdf2Params,
		baseKey: CryptoKey,
		derivedKeyType:
			| AlgorithmIdentifier
			| HkdfParams
			| Pbkdf2Params
			| AesDerivedKeyParams
			| HmacImportParams,
		extractable: boolean,
		keyUsages: Iterable<KeyUsage>
	): Promise<CryptoKey>;
	deriveKey(
		algorithm: unknown,
		baseKey: unknown,
		derivedKeyType: unknown,
		extractable: unknown,
		keyUsages: unknown
	): Promise<CryptoKey> {
		throw new Error('Method not implemented.');
	}

	/**
	 * Generates a digest of the given data. A digest is a short fixed-length value
	 * derived from some variable-length input. Cryptographic digests should exhibit
	 * collision-resistance, meaning that it's hard to come up with two different
	 * inputs that have the same digest value.
	 *
	 * It takes as its arguments an identifier for the digest algorithm to use and
	 * the data to digest. It returns a Promise which will be fulfilled with the digest.
	 *
	 * Note that this API does not support streaming input: you must read the entire
	 * input into memory before passing it into the digest function.
	 *
	 * @param algorithm This may be a string or an object with a single property `name`
	 * that is a string. The string names the hash function to use. Supported values are:
	 *
	 *  - `"SHA-1"` (but don't use this in cryptographic applications)
	 *  - `"SHA-256"`
	 *  - `"SHA-384"`
	 *  - `"SHA-512"`
	 * @param data An `ArrayBuffer`, a `TypedArray` or a `DataView` object containing the data to be digested
	 * @see https://developer.mozilla.org/docs/Web/API/SubtleCrypto/digest
	 */
	digest(
		algorithm: AlgorithmIdentifier,
		data: BufferSource
	): Promise<ArrayBuffer> {
		return toPromise(
			$.cryptoDigest,
			typeof algorithm === 'string' ? algorithm : algorithm.name,
			bufferSourceToArrayBuffer(data)
		);
	}

	encrypt(
		algorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams,
		key: CryptoKey,
		data: BufferSource
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
	exportKey(format: 'jwk', key: CryptoKey): Promise<JsonWebKey>;
	exportKey(
		format: 'pkcs8' | 'raw' | 'spki',
		key: CryptoKey
	): Promise<ArrayBuffer>;
	exportKey(
		format: KeyFormat,
		key: CryptoKey
	): Promise<ArrayBuffer | JsonWebKey>;
	exportKey(
		format: unknown,
		key: unknown
	):
		| Promise<ArrayBuffer>
		| Promise<JsonWebKey>
		| Promise<ArrayBuffer | JsonWebKey> {
		throw new Error('Method not implemented.');
	}
	generateKey(
		algorithm: 'Ed25519',
		extractable: boolean,
		keyUsages: readonly ('sign' | 'verify')[]
	): Promise<CryptoKeyPair>;
	generateKey(
		algorithm: RsaHashedKeyGenParams | EcKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKeyPair>;
	generateKey(
		algorithm: Pbkdf2Params | AesKeyGenParams | HmacKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKey>;
	generateKey(
		algorithm: AlgorithmIdentifier,
		extractable: boolean,
		keyUsages: KeyUsage[]
	): Promise<CryptoKey | CryptoKeyPair>;
	generateKey(
		algorithm: RsaHashedKeyGenParams | EcKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKeyPair>;
	generateKey(
		algorithm: Pbkdf2Params | AesKeyGenParams | HmacKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKey>;
	generateKey(
		algorithm: AlgorithmIdentifier,
		extractable: boolean,
		keyUsages: Iterable<KeyUsage>
	): Promise<CryptoKey | CryptoKeyPair>;
	generateKey(
		algorithm: unknown,
		extractable: unknown,
		keyUsages: unknown
	):
		| Promise<CryptoKey>
		| Promise<CryptoKeyPair>
		| Promise<CryptoKey | CryptoKeyPair> {
		throw new Error('Method not implemented.');
	}
	importKey(
		format: 'jwk',
		keyData: JsonWebKey,
		algorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKey>;
	importKey(
		format: 'pkcs8' | 'raw' | 'spki',
		keyData: BufferSource,
		algorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: KeyUsage[]
	): Promise<CryptoKey>;
	importKey(
		format: 'jwk',
		keyData: JsonWebKey,
		algorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: readonly KeyUsage[]
	): Promise<CryptoKey>;
	importKey(
		format: 'pkcs8' | 'raw' | 'spki',
		keyData: BufferSource,
		algorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: Iterable<KeyUsage>
	): Promise<CryptoKey>;
	importKey(
		format: unknown,
		keyData: unknown,
		algorithm: unknown,
		extractable: unknown,
		keyUsages: unknown
	): Promise<CryptoKey> {
		throw new Error('Method not implemented.');
	}
	sign(
		algorithm: AlgorithmIdentifier | RsaPssParams | EcdsaParams,
		key: CryptoKey,
		data: BufferSource
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
	unwrapKey(
		format: KeyFormat,
		wrappedKey: BufferSource,
		unwrappingKey: CryptoKey,
		unwrapAlgorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams,
		unwrappedKeyAlgorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: KeyUsage[]
	): Promise<CryptoKey>;
	unwrapKey(
		format: KeyFormat,
		wrappedKey: BufferSource,
		unwrappingKey: CryptoKey,
		unwrapAlgorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams,
		unwrappedKeyAlgorithm:
			| AlgorithmIdentifier
			| HmacImportParams
			| RsaHashedImportParams
			| EcKeyImportParams
			| AesKeyAlgorithm,
		extractable: boolean,
		keyUsages: Iterable<KeyUsage>
	): Promise<CryptoKey>;
	unwrapKey(
		format: unknown,
		wrappedKey: unknown,
		unwrappingKey: unknown,
		unwrapAlgorithm: unknown,
		unwrappedKeyAlgorithm: unknown,
		extractable: unknown,
		keyUsages: unknown
	): Promise<CryptoKey> {
		throw new Error('Method not implemented.');
	}
	verify(
		algorithm: AlgorithmIdentifier | RsaPssParams | EcdsaParams,
		key: CryptoKey,
		signature: BufferSource,
		data: BufferSource
	): Promise<boolean> {
		throw new Error('Method not implemented.');
	}
	wrapKey(
		format: KeyFormat,
		key: CryptoKey,
		wrappingKey: CryptoKey,
		wrapAlgorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
}
def(SubtleCrypto);
