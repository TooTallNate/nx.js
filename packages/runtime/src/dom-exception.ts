/**
 * `DOMException` polyfill.
 *
 * QuickJS-ng provided `DOMException` natively; V8 does not, so nx.js ships this
 * pure-JS implementation (registered as a global in `index.ts`).
 */

const LEGACY_CODES: Record<string, number> = {
	IndexSizeError: 1,
	HierarchyRequestError: 3,
	WrongDocumentError: 4,
	InvalidCharacterError: 5,
	NoModificationAllowedError: 7,
	NotFoundError: 8,
	NotSupportedError: 9,
	InUseAttributeError: 10,
	InvalidStateError: 11,
	SyntaxError: 12,
	InvalidModificationError: 13,
	NamespaceError: 14,
	InvalidAccessError: 15,
	TypeMismatchError: 17,
	SecurityError: 18,
	NetworkError: 19,
	AbortError: 20,
	URLMismatchError: 21,
	QuotaExceededError: 22,
	TimeoutError: 23,
	InvalidNodeTypeError: 24,
	DataCloneError: 25,
};

export class DOMException extends Error {
	readonly code: number;

	constructor(message: string = '', name: string = 'Error') {
		super(message);
		this.name = name;
		this.code = LEGACY_CODES[name] ?? 0;
	}

	static readonly INDEX_SIZE_ERR = 1;
	static readonly DOMSTRING_SIZE_ERR = 2;
	static readonly HIERARCHY_REQUEST_ERR = 3;
	static readonly WRONG_DOCUMENT_ERR = 4;
	static readonly INVALID_CHARACTER_ERR = 5;
	static readonly NO_DATA_ALLOWED_ERR = 6;
	static readonly NO_MODIFICATION_ALLOWED_ERR = 7;
	static readonly NOT_FOUND_ERR = 8;
	static readonly NOT_SUPPORTED_ERR = 9;
	static readonly INUSE_ATTRIBUTE_ERR = 10;
	static readonly INVALID_STATE_ERR = 11;
	static readonly SYNTAX_ERR = 12;
	static readonly INVALID_MODIFICATION_ERR = 13;
	static readonly NAMESPACE_ERR = 14;
	static readonly INVALID_ACCESS_ERR = 15;
	static readonly VALIDATION_ERR = 16;
	static readonly TYPE_MISMATCH_ERR = 17;
	static readonly SECURITY_ERR = 18;
	static readonly NETWORK_ERR = 19;
	static readonly ABORT_ERR = 20;
	static readonly URL_MISMATCH_ERR = 21;
	static readonly QUOTA_EXCEEDED_ERR = 22;
	static readonly TIMEOUT_ERR = 23;
	static readonly INVALID_NODE_TYPE_ERR = 24;
	static readonly DATA_CLONE_ERR = 25;
}

// Instance-side numeric constants (mirror the statics).
const proto = DOMException.prototype as any;
for (const [k, v] of Object.entries({
	INDEX_SIZE_ERR: 1,
	DOMSTRING_SIZE_ERR: 2,
	HIERARCHY_REQUEST_ERR: 3,
	WRONG_DOCUMENT_ERR: 4,
	INVALID_CHARACTER_ERR: 5,
	NO_DATA_ALLOWED_ERR: 6,
	NO_MODIFICATION_ALLOWED_ERR: 7,
	NOT_FOUND_ERR: 8,
	NOT_SUPPORTED_ERR: 9,
	INUSE_ATTRIBUTE_ERR: 10,
	INVALID_STATE_ERR: 11,
	SYNTAX_ERR: 12,
	INVALID_MODIFICATION_ERR: 13,
	NAMESPACE_ERR: 14,
	INVALID_ACCESS_ERR: 15,
	VALIDATION_ERR: 16,
	TYPE_MISMATCH_ERR: 17,
	SECURITY_ERR: 18,
	NETWORK_ERR: 19,
	ABORT_ERR: 20,
	URL_MISMATCH_ERR: 21,
	QUOTA_EXCEEDED_ERR: 22,
	TIMEOUT_ERR: 23,
	INVALID_NODE_TYPE_ERR: 24,
	DATA_CLONE_ERR: 25,
})) {
	proto[k] = v;
}
