import { $ } from './$';
import { INTERNAL_SYMBOL } from './internal';
import { assertInternalConstructor, createInternal, def, stub } from './utils';
import { CryptoKey, type CryptoKeyPair } from './crypto/crypto-key';
import type {
	AesCbcParams,
	AesCtrParams,
	AesDerivedKeyParams,
	AesGcmParams,
	AesKeyGenParams,
	ArrayBufferView,
	BufferSource,
	EncryptionAlgorithm,
	EcKeyGenParams,
	EcKeyImportParams,
	EcdhKeyDeriveParams,
	EcdsaParams,
	HmacImportParams,
	HmacKeyGenParams,
	JsonWebKey,
	KeyAlgorithmIdentifier,
	KeyFormat,
	KeyImportParams,
	KeyUsage,
} from './types';

export * from './crypto/crypto-key';

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
		stub();
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
$.cryptoInit(Crypto);
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

	/**
	 * Decrypts some encrypted data.
	 *
	 * It takes as arguments a key to decrypt with, some optional extra parameters, and the data to decrypt (also known as "ciphertext").
	 *
	 * @returns A Promise which will be fulfilled with the decrypted data (also known as "plaintext") as an `ArrayBuffer`.
	 * @see https://developer.mozilla.org/docs/Web/API/SubtleCrypto/decrypt
	 */
	decrypt<Cipher extends KeyAlgorithmIdentifier>(
		algorithm: EncryptionAlgorithm<Cipher>,
		key: CryptoKey<Cipher>,
		data: BufferSource,
	): Promise<ArrayBuffer> {
		stub();
	}

	async deriveBits(
		algorithm:
			| AlgorithmIdentifier
			| EcdhKeyDeriveParams
			| HkdfParams
			| Pbkdf2Params,
		baseKey: CryptoKey<never>,
		length: number,
	): Promise<ArrayBuffer> {
		const algo = normalizeAlgorithm(algorithm);
		// Normalize hash inside algorithm params
		if ('hash' in algo) {
			(algo as any).hash = normalizeHashAlgorithm(
				(algo as any).hash as HashAlgorithmIdentifier,
			);
		}
		return $.cryptoDeriveBits(algo, baseKey, length);
	}

	async deriveKey(
		algorithm:
			| AlgorithmIdentifier
			| EcdhKeyDeriveParams
			| HkdfParams
			| Pbkdf2Params,
		baseKey: CryptoKey<never>,
		derivedKeyType:
			| AlgorithmIdentifier
			| HkdfParams
			| Pbkdf2Params
			| AesDerivedKeyParams
			| HmacImportParams,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<never>> {
		const dkt =
			typeof derivedKeyType === 'string'
				? { name: derivedKeyType }
				: derivedKeyType;

		// Determine the key length in bits
		let lengthBits: number;
		if ('length' in dkt && typeof (dkt as any).length === 'number') {
			lengthBits = (dkt as any).length;
		} else if (dkt.name === 'HMAC') {
			const hmacDkt = dkt as HmacImportParams;
			lengthBits = getHashLength(hmacDkt.hash);
		} else {
			throw new DOMException(
				'Cannot determine key length for derived key type',
				'OperationError',
			);
		}

		const bits = await this.deriveBits(algorithm, baseKey, lengthBits);
		return this.importKey(
			'raw',
			bits,
			dkt as any,
			extractable,
			keyUsages,
		) as Promise<CryptoKey<never>>;
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
		data: BufferSource,
	): Promise<ArrayBuffer> {
		return $.cryptoDigest(
			typeof algorithm === 'string' ? algorithm : algorithm.name,
			data,
		);
	}

	/**
	 * Encrypts plaintext data.
	 *
	 * It takes as its arguments a key to encrypt with, some algorithm-specific parameters, and the data to encrypt (also known as "plaintext").
	 *
	 * @returns A Promise which will be fulfilled with the encrypted data (also known as "ciphertext") as an `ArrayBuffer`.
	 * @see https://developer.mozilla.org/docs/Web/API/SubtleCrypto/encrypt
	 */
	async encrypt<Cipher extends KeyAlgorithmIdentifier>(
		algorithm: EncryptionAlgorithm<Cipher>,
		key: CryptoKey<Cipher>,
		data: BufferSource,
	): Promise<ArrayBuffer> {
		return $.cryptoEncrypt(normalizeAlgorithm(algorithm), key, data);
	}

	exportKey(format: 'jwk', key: CryptoKey<never>): Promise<JsonWebKey>;
	exportKey(
		format: 'pkcs8' | 'raw' | 'spki',
		key: CryptoKey<never>,
	): Promise<ArrayBuffer>;
	async exportKey(
		format: KeyFormat,
		key: CryptoKey<never>,
	): Promise<ArrayBuffer | JsonWebKey> {
		if (format === 'raw') {
			return $.cryptoExportKey(format, key);
		}
		throw new DOMException(
			`Unsupported export format: ${format}`,
			'NotSupportedError',
		);
	}

	generateKey(
		algorithm: 'Ed25519',
		extractable: boolean,
		keyUsages: readonly ('sign' | 'verify')[],
	): Promise<CryptoKeyPair<never>>;
	generateKey(
		algorithm: RsaHashedKeyGenParams | EcKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[],
	): Promise<CryptoKeyPair<never>>;
	generateKey(
		algorithm: AesKeyGenParams | HmacKeyGenParams,
		extractable: boolean,
		keyUsages: readonly KeyUsage[],
	): Promise<CryptoKey<never>>;
	async generateKey(
		algorithm: unknown,
		extractable: unknown,
		keyUsages: unknown,
	): Promise<CryptoKey<any> | CryptoKeyPair<any>> {
		const algo =
			typeof algorithm === 'string'
				? { name: algorithm }
				: (algorithm as Algorithm);

		if (
			algo.name === 'AES-CBC' ||
			algo.name === 'AES-CTR' ||
			algo.name === 'AES-GCM'
		) {
			const length = (algo as AesKeyGenParams).length;
			if (length !== 128 && length !== 192 && length !== 256) {
				throw new DOMException(
					'AES key length must be 128, 192, or 256 bits',
					'OperationError',
				);
			}
			const keyData = new Uint8Array(length / 8);
			crypto.getRandomValues(keyData);
			return this.importKey(
				'raw',
				keyData,
				algo as any,
				extractable as boolean,
				keyUsages as KeyUsage[],
			);
		}

		if (algo.name === 'HMAC') {
			const hmacAlgo = algo as HmacKeyGenParams;
			const hashLength = getHashLength(hmacAlgo.hash);
			const length = hmacAlgo.length || hashLength;
			const keyData = new Uint8Array(Math.ceil(length / 8));
			crypto.getRandomValues(keyData);
			// Normalize hash to object form for importKey
			const importAlgo = {
				...hmacAlgo,
				hash: normalizeHashAlgorithm(hmacAlgo.hash),
			};
			return this.importKey(
				'raw',
				keyData,
				importAlgo as any,
				extractable as boolean,
				keyUsages as KeyUsage[],
			);
		}

		throw new DOMException(
			`Unsupported algorithm: ${algo.name}`,
			'NotSupportedError',
		);
	}

	/**
	 * Takes as input a key in an external, portable format and returns a
	 * {@link CryptoKey | `CryptoKey`} instance which can be used in the Web Crypto API.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/SubtleCrypto/importKey
	 */
	importKey<Cipher extends KeyAlgorithmIdentifier>(
		format: 'jwk',
		keyData: JsonWebKey,
		algorithm: Cipher | KeyImportParams<Cipher>,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<Cipher>>;
	importKey<Cipher extends KeyAlgorithmIdentifier>(
		format: 'pkcs8' | 'raw' | 'spki',
		keyData: BufferSource,
		algorithm: Cipher | KeyImportParams<Cipher>,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<Cipher>>;
	async importKey<Cipher extends KeyAlgorithmIdentifier>(
		format: KeyFormat,
		keyData: BufferSource | JsonWebKey,
		algorithm: Cipher | KeyImportParams<Cipher>,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<Cipher>> {
		if (format !== 'raw') {
			// Only "raw" format is supported at this time
			throw new TypeError(
				`Failed to execute 'importKey' on 'SubtleCrypto': 1st argument value '${format}' is not a valid enum value of type KeyFormat.`,
			);
		}
		const algo =
			typeof algorithm === 'string' ? { name: algorithm } : algorithm;
		const key = new CryptoKey<Cipher>(
			// @ts-expect-error Internal constructor
			INTERNAL_SYMBOL,
			algo,
			keyData,
			extractable,
			keyUsages,
		);
		return key;
	}

	async sign(
		algorithm: AlgorithmIdentifier | RsaPssParams | EcdsaParams,
		key: CryptoKey<never>,
		data: BufferSource,
	): Promise<ArrayBuffer> {
		return $.cryptoSign(normalizeAlgorithm(algorithm), key, data);
	}

	unwrapKey(
		format: KeyFormat,
		wrappedKey: BufferSource,
		unwrappingKey: CryptoKey<never>,
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
			| EcKeyImportParams,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<never>> {
		throw new Error('Method not implemented.');
	}

	async verify(
		algorithm: AlgorithmIdentifier | RsaPssParams | EcdsaParams,
		key: CryptoKey<never>,
		signature: BufferSource,
		data: BufferSource,
	): Promise<boolean> {
		return $.cryptoVerify(
			normalizeAlgorithm(algorithm),
			key,
			signature,
			data,
		);
	}

	wrapKey(
		format: KeyFormat,
		key: CryptoKey<never>,
		wrappingKey: CryptoKey<never>,
		wrapAlgorithm:
			| AlgorithmIdentifier
			| RsaOaepParams
			| AesCtrParams
			| AesCbcParams
			| AesGcmParams,
	): Promise<ArrayBuffer> {
		throw new Error('Method not implemented.');
	}
}
$.cryptoSubtleInit(SubtleCrypto);
def(SubtleCrypto);

function normalizeAlgorithm(algorithm: AlgorithmIdentifier): Algorithm {
	return typeof algorithm === 'string' ? { name: algorithm } : algorithm;
}

function normalizeHashAlgorithm(
	hash: HashAlgorithmIdentifier,
): { name: string } {
	return typeof hash === 'string' ? { name: hash } : hash;
}

function getHashLength(hash: HashAlgorithmIdentifier): number {
	const name = typeof hash === 'string' ? hash : hash.name;
	switch (name) {
		case 'SHA-1':
			return 160;
		case 'SHA-256':
			return 256;
		case 'SHA-384':
			return 384;
		case 'SHA-512':
			return 512;
		default:
			throw new DOMException(
				`Unrecognized hash algorithm: ${name}`,
				'NotSupportedError',
			);
	}
}

type HashAlgorithmIdentifier = AlgorithmIdentifier;
