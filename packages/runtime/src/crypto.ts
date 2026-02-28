import { $ } from './$';
import { CryptoKey, type CryptoKeyPair } from './crypto/crypto-key';
import { DOMException } from './dom-exception';
import { INTERNAL_SYMBOL } from './internal';
import { TextDecoder } from './polyfills/text-decoder';
import { TextEncoder } from './polyfills/text-encoder';
import type {
	AesCbcParams,
	AesCtrParams,
	AesDerivedKeyParams,
	AesGcmParams,
	AesKeyGenParams,
	ArrayBufferView,
	BufferSource,
	EcdhKeyDeriveParams,
	EcdsaParams,
	EcKeyGenParams,
	EcKeyImportParams,
	EncryptionAlgorithm,
	HmacImportParams,
	HmacKeyGenParams,
	JsonWebKey,
	KeyAlgorithmIdentifier,
	KeyFormat,
	KeyImportParams,
	KeyUsage,
} from './types';
import {
	assertInternalConstructor,
	createInternal,
	def,
	proto,
	stub,
} from './utils';

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
			const algoWithHash = algo as Algorithm & {
				hash: HashAlgorithmIdentifier;
			};
			algoWithHash.hash = normalizeHashAlgorithm(algoWithHash.hash);
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
		if (
			'length' in dkt &&
			typeof (dkt as { length?: unknown }).length === 'number'
		) {
			lengthBits = (dkt as AesDerivedKeyParams).length;
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
		return this.importKey('raw', bits, dkt, extractable, keyUsages);
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
		if (format === 'pkcs8') {
			return $.cryptoExportKeyPkcs8(key);
		}
		if (format === 'spki') {
			return $.cryptoExportKeySpki(key);
		}
		if (format === 'jwk') {
			return exportKeyJwk(key);
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
				algo,
				extractable as boolean,
				keyUsages as KeyUsage[],
			);
		}

		if (algo.name === 'ECDSA' || algo.name === 'ECDH') {
			const ecAlgo = algo as EcKeyGenParams;
			const [pubRaw, privRaw] = $.cryptoGenerateKeyEc(ecAlgo.namedCurve) as [
				ArrayBuffer,
				ArrayBuffer,
			];

			// Import public key
			const publicKey = await this.importKey(
				'raw',
				pubRaw,
				{ name: algo.name, namedCurve: ecAlgo.namedCurve } as any,
				extractable as boolean,
				algo.name === 'ECDSA'
					? (keyUsages as KeyUsage[]).filter((u) => u === 'verify')
					: [],
			);

			// Create private key via native call
			const privUsages =
				algo.name === 'ECDSA'
					? (keyUsages as KeyUsage[]).filter((u) => u === 'sign')
					: (keyUsages as KeyUsage[]).filter(
							(u) => u === 'deriveBits' || u === 'deriveKey',
						);
			const privKeyRaw = $.cryptoKeyNewEcPrivate(
				{ name: algo.name, namedCurve: ecAlgo.namedCurve },
				privRaw,
				pubRaw,
				extractable as boolean,
				privUsages,
			);
			const privateKey = proto(privKeyRaw, CryptoKey);

			return {
				publicKey,
				privateKey,
			} as CryptoKeyPair<any>;
		}

		if (
			algo.name === 'RSA-OAEP' ||
			algo.name === 'RSASSA-PKCS1-v1_5' ||
			algo.name === 'RSA-PSS'
		) {
			const rsaAlgo = algo as RsaHashedKeyGenParams;
			const hashName =
				typeof rsaAlgo.hash === 'string' ? rsaAlgo.hash : rsaAlgo.hash.name;
			const pe = rsaAlgo.publicExponent;
			const peNum =
				pe instanceof Uint8Array
					? pe.reduce((acc, b) => acc * 256 + b, 0)
					: 65537;
			if (peNum > 0xffffffff) {
				throw new DOMException(
					'publicExponent exceeds 32 bits',
					'OperationError',
				);
			}

			const components = (await $.cryptoGenerateKeyRsa(
				rsaAlgo.modulusLength,
				peNum,
			)) as ArrayBuffer[];
			const [n, e, d, p, q] = components;

			const publicKey = proto(
				$.cryptoKeyNewRsa(
					algo.name,
					hashName,
					'public',
					n,
					e,
					null,
					null,
					null,
					extractable as boolean,
					algo.name === 'RSA-OAEP'
						? (keyUsages as KeyUsage[]).filter(
								(u) => u === 'encrypt' || u === 'wrapKey',
							)
						: (keyUsages as KeyUsage[]).filter((u) => u === 'verify'),
				),
				CryptoKey,
			);

			const privateKey = proto(
				$.cryptoKeyNewRsa(
					algo.name,
					hashName,
					'private',
					n,
					e,
					d,
					p,
					q,
					extractable as boolean,
					algo.name === 'RSA-OAEP'
						? (keyUsages as KeyUsage[]).filter(
								(u) => u === 'decrypt' || u === 'unwrapKey',
							)
						: (keyUsages as KeyUsage[]).filter((u) => u === 'sign'),
				),
				CryptoKey,
			);

			return {
				publicKey,
				privateKey,
			} as CryptoKeyPair<any>;
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
				importAlgo,
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
	importKey(
		format: KeyFormat,
		keyData: BufferSource | JsonWebKey,
		algorithm: Algorithm,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<never>>;
	async importKey<Cipher extends KeyAlgorithmIdentifier>(
		format: KeyFormat,
		keyData: BufferSource | JsonWebKey,
		algorithm: Cipher | KeyImportParams<Cipher>,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): Promise<CryptoKey<Cipher>> {
		const algo =
			typeof algorithm === 'string' ? { name: algorithm } : algorithm;

		if (format === 'jwk') {
			return importKeyJwk(
				keyData as JsonWebKey,
				algo as Algorithm,
				extractable,
				keyUsages,
			) as Promise<CryptoKey<Cipher>>;
		}

		if (format === 'pkcs8' || format === 'spki') {
			const algoObj = algo as Algorithm;
			let paramName: string;
			if (
				algoObj.name === 'RSA-OAEP' ||
				algoObj.name === 'RSA-PSS' ||
				algoObj.name === 'RSASSA-PKCS1-v1_5'
			) {
				const h = (algoObj as any).hash;
				paramName = typeof h === 'string' ? h : h.name;
			} else {
				paramName = (algoObj as any).namedCurve || '';
			}
			const imported = $.cryptoImportKeyPkcs8Spki(
				format,
				keyData as BufferSource,
				algoObj.name,
				paramName,
				extractable,
				keyUsages,
			);
			return proto(imported, CryptoKey) as CryptoKey<Cipher>;
		}

		if (format !== 'raw') {
			throw new TypeError(
				`Failed to execute 'importKey' on 'SubtleCrypto': 1st argument value '${format}' is not a valid enum value of type KeyFormat.`,
			);
		}
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

	async unwrapKey(
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
		const decrypted = await this.decrypt(
			unwrapAlgorithm as any,
			unwrappingKey as any,
			wrappedKey,
		);
		return this.importKey(
			format as any,
			format === 'jwk'
				? JSON.parse(new TextDecoder().decode(decrypted))
				: decrypted,
			unwrappedKeyAlgorithm as any,
			extractable,
			keyUsages,
		) as Promise<CryptoKey<never>>;
	}

	async verify(
		algorithm: AlgorithmIdentifier | RsaPssParams | EcdsaParams,
		key: CryptoKey<never>,
		signature: BufferSource,
		data: BufferSource,
	): Promise<boolean> {
		return $.cryptoVerify(normalizeAlgorithm(algorithm), key, signature, data);
	}

	async wrapKey(
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
		const exported = await this.exportKey(format as any, key);
		const data =
			format === 'jwk'
				? new TextEncoder().encode(JSON.stringify(exported))
				: (exported as ArrayBuffer);
		return this.encrypt(wrapAlgorithm as any, wrappingKey as any, data);
	}
}
$.cryptoSubtleInit(SubtleCrypto);
def(SubtleCrypto);

function normalizeAlgorithm(algorithm: AlgorithmIdentifier): Algorithm {
	return typeof algorithm === 'string' ? { name: algorithm } : algorithm;
}

function normalizeHashAlgorithm(hash: HashAlgorithmIdentifier): {
	name: string;
} {
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

const b64urlOpts = { alphabet: 'base64url' as const, omitPadding: true };

function base64urlEncode(buf: ArrayBuffer): string {
	return new Uint8Array(buf).toBase64(b64urlOpts);
}

function base64urlDecode(str: string): ArrayBuffer {
	return Uint8Array.fromBase64(str, b64urlOpts).buffer;
}

// --- JWK Export ---

function getJwkAlg(
	algoName: string,
	hashName: string,
	keyLength?: number,
): string | undefined {
	if (algoName === 'RSA-OAEP') {
		if (hashName === 'SHA-1') return 'RSA-OAEP';
		if (hashName === 'SHA-256') return 'RSA-OAEP-256';
		if (hashName === 'SHA-384') return 'RSA-OAEP-384';
		if (hashName === 'SHA-512') return 'RSA-OAEP-512';
	}
	if (algoName === 'RSASSA-PKCS1-v1_5') {
		if (hashName === 'SHA-1') return 'RS1';
		if (hashName === 'SHA-256') return 'RS256';
		if (hashName === 'SHA-384') return 'RS384';
		if (hashName === 'SHA-512') return 'RS512';
	}
	if (algoName === 'RSA-PSS') {
		if (hashName === 'SHA-1') return 'PS1';
		if (hashName === 'SHA-256') return 'PS256';
		if (hashName === 'SHA-384') return 'PS384';
		if (hashName === 'SHA-512') return 'PS512';
	}
	if (
		algoName === 'AES-CBC' ||
		algoName === 'AES-GCM' ||
		algoName === 'AES-CTR'
	) {
		const suffix =
			algoName === 'AES-CBC' ? 'CBC' : algoName === 'AES-GCM' ? 'GCM' : 'CTR';
		if (keyLength) return `A${keyLength}${suffix}`;
	}
	if (algoName === 'HMAC') {
		if (hashName === 'SHA-256') return 'HS256';
		if (hashName === 'SHA-384') return 'HS384';
		if (hashName === 'SHA-512') return 'HS512';
	}
	return undefined;
}

async function exportKeyJwk(key: CryptoKey<never>): Promise<JsonWebKey> {
	const algo = key.algorithm as any;
	const algoName = algo.name as string;

	if (
		algoName === 'RSA-OAEP' ||
		algoName === 'RSA-PSS' ||
		algoName === 'RSASSA-PKCS1-v1_5'
	) {
		const components = $.cryptoRsaExportComponents(key) as ArrayBuffer[];
		const hashName = algo.hash?.name || 'SHA-256';
		const jwk: JsonWebKey = {
			kty: 'RSA',
			n: base64urlEncode(components[0]),
			e: base64urlEncode(components[1]),
			alg: getJwkAlg(algoName, hashName),
			ext: key.extractable,
			key_ops: Array.prototype.slice.call(key.usages),
		};
		if (key.type === 'private' && components.length > 2) {
			jwk.d = base64urlEncode(components[2]);
			jwk.p = base64urlEncode(components[3]);
			jwk.q = base64urlEncode(components[4]);
			jwk.dp = base64urlEncode(components[5]);
			jwk.dq = base64urlEncode(components[6]);
			jwk.qi = base64urlEncode(components[7]);
		}
		return jwk;
	}

	if (algoName === 'ECDSA' || algoName === 'ECDH') {
		// Always get the public point (04 || x || y) via dedicated native function
		const pubRaw = $.cryptoEcExportPublicRaw(key) as ArrayBuffer;
		const pubBytes = new Uint8Array(pubRaw);
		const curveName = algo.namedCurve as string;
		const coordSize = curveName === 'P-256' ? 32 : 48;
		const jwk: JsonWebKey = {
			kty: 'EC',
			crv: curveName,
			x: base64urlEncode(pubBytes.slice(1, 1 + coordSize).buffer),
			y: base64urlEncode(pubBytes.slice(1 + coordSize).buffer),
			ext: key.extractable,
			key_ops: Array.prototype.slice.call(key.usages),
		};
		if (key.type === 'private') {
			// For private keys, raw_key_data is the scalar d
			const privRaw = $.cryptoExportKey('raw', key) as ArrayBuffer;
			jwk.d = base64urlEncode(privRaw);
		}
		return jwk;
	}

	if (
		algoName === 'AES-CBC' ||
		algoName === 'AES-CTR' ||
		algoName === 'AES-GCM'
	) {
		const raw = (await $.cryptoExportKey('raw', key)) as ArrayBuffer;
		return {
			kty: 'oct',
			k: base64urlEncode(raw),
			alg: getJwkAlg(algoName, '', algo.length),
			ext: key.extractable,
			key_ops: Array.prototype.slice.call(key.usages),
		};
	}

	if (algoName === 'HMAC') {
		const raw = (await $.cryptoExportKey('raw', key)) as ArrayBuffer;
		const hashName = algo.hash?.name || 'SHA-256';
		return {
			kty: 'oct',
			k: base64urlEncode(raw),
			alg: getJwkAlg(algoName, hashName),
			ext: key.extractable,
			key_ops: Array.prototype.slice.call(key.usages),
		};
	}

	throw new DOMException(
		`JWK export not supported for ${algoName}`,
		'NotSupportedError',
	);
}

// --- JWK Import ---

async function importKeyJwk(
	jwk: JsonWebKey,
	algo: Algorithm,
	extractable: boolean,
	keyUsages: KeyUsage[],
): Promise<CryptoKey<never>> {
	const algoName = algo.name;

	if (
		algoName === 'RSA-OAEP' ||
		algoName === 'RSA-PSS' ||
		algoName === 'RSASSA-PKCS1-v1_5'
	) {
		const hashAlgo = (algo as any).hash;
		const hashName = typeof hashAlgo === 'string' ? hashAlgo : hashAlgo.name;
		const n = base64urlDecode(jwk.n!);
		const e = base64urlDecode(jwk.e!);
		const isPrivate = !!jwk.d;
		const d = isPrivate ? base64urlDecode(jwk.d!) : null;
		const p = isPrivate && jwk.p ? base64urlDecode(jwk.p) : null;
		const q = isPrivate && jwk.q ? base64urlDecode(jwk.q) : null;

		return proto(
			$.cryptoKeyNewRsa(
				algoName,
				hashName,
				isPrivate ? 'private' : 'public',
				n,
				e,
				d,
				p,
				q,
				extractable,
				keyUsages,
			),
			CryptoKey,
		) as CryptoKey<never>;
	}

	if (algoName === 'ECDSA' || algoName === 'ECDH') {
		const namedCurve = (algo as any).namedCurve;
		if (jwk.d) {
			// Private key
			const x = base64urlDecode(jwk.x!);
			const y = base64urlDecode(jwk.y!);
			const d = base64urlDecode(jwk.d);
			const coordSize = namedCurve === 'P-256' ? 32 : 48;
			const pubRaw = new Uint8Array(1 + coordSize * 2);
			pubRaw[0] = 0x04;
			pubRaw.set(new Uint8Array(x), 1);
			pubRaw.set(new Uint8Array(y), 1 + coordSize);

			const privKey = $.cryptoKeyNewEcPrivate(
				{ name: algoName, namedCurve },
				d,
				pubRaw.buffer,
				extractable,
				keyUsages,
			);
			return proto(privKey, CryptoKey) as CryptoKey<never>;
		} else {
			// Public key - construct uncompressed point
			const x = base64urlDecode(jwk.x!);
			const y = base64urlDecode(jwk.y!);
			const coordSize = namedCurve === 'P-256' ? 32 : 48;
			const raw = new Uint8Array(1 + coordSize * 2);
			raw[0] = 0x04;
			raw.set(new Uint8Array(x), 1);
			raw.set(new Uint8Array(y), 1 + coordSize);

			const subtleCrypto = crypto.subtle;
			return subtleCrypto.importKey(
				'raw',
				raw.buffer,
				{ name: algoName, namedCurve } as any,
				extractable,
				keyUsages,
			) as Promise<CryptoKey<never>>;
		}
	}

	if (
		algoName === 'AES-CBC' ||
		algoName === 'AES-CTR' ||
		algoName === 'AES-GCM' ||
		algoName === 'HMAC'
	) {
		const raw = base64urlDecode(jwk.k!);
		const subtleCrypto = crypto.subtle;
		return subtleCrypto.importKey(
			'raw',
			raw,
			algo as any,
			extractable,
			keyUsages,
		) as Promise<CryptoKey<never>>;
	}

	if (algoName === 'PBKDF2' || algoName === 'HKDF') {
		const raw = base64urlDecode(jwk.k!);
		const subtleCrypto = crypto.subtle;
		return subtleCrypto.importKey(
			'raw',
			raw,
			algo as any,
			extractable,
			keyUsages,
		) as Promise<CryptoKey<never>>;
	}

	throw new DOMException(
		`JWK import not supported for ${algoName}`,
		'NotSupportedError',
	);
}
