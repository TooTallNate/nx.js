import { $ } from '../$';
import { inspect } from '../switch/inspect';
import { assertInternalConstructor, def, proto } from '../utils';
import type {
	KeyAlgorithmIdentifier,
	KeyGenParams,
	KeyType,
	KeyUsage,
} from '../types';

export interface CryptoKeyPair<Cipher extends KeyAlgorithmIdentifier> {
	privateKey: CryptoKey<Cipher>;
	publicKey: CryptoKey<Cipher>;
}

export class CryptoKey<Cipher extends KeyAlgorithmIdentifier>
	implements globalThis.CryptoKey
{
	declare readonly algorithm: KeyGenParams<Cipher>;
	declare readonly extractable: boolean;
	declare readonly type: KeyType;
	declare readonly usages: KeyUsage[];

	/**
	 * @private
	 */
	constructor() {
		assertInternalConstructor(arguments);
		return proto(
			$.cryptoKeyNew(arguments[1], arguments[2], arguments[3], arguments[4]),
			CryptoKey<Cipher>,
		);
	}
}
$.cryptoKeyInit(CryptoKey);
def(CryptoKey);

Object.defineProperty(CryptoKey.prototype, inspect.keys, {
	enumerable: false,
	value: () => ['type', 'extractable', 'algorithm', 'usages'],
});
