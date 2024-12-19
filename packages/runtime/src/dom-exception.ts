import { def } from './utils';

const names = [
	'IndexSizeError',
	'DOMStringSizeError',
	'HierarchyRequestError',
	'WrongDocumentError',
	'InvalidCharacterError',
	'NoDataAllowedError',
	'NoModificationAllowedError',
	'NotFoundError',
	'NotSupportedError',
	'InuseAttributeError',
	'InvalidStateError',
	'SyntaxError',
	'InvalidModificationError',
	'NamespaceError',
	'InvalidAccessError',
	'ValidationError',
	'TypeMismatchError',
	'SecurityError',
	'NetworkError',
	'AbortError',
	'URLMismatchError',
	'QuotaExceededError',
	'TimeoutError',
	'InvalidNodeTypeError',
	'DataCloneError',
] as const;

export class DOMException extends Error implements globalThis.DOMException {
	declare INDEX_SIZE_ERR: 1;
	declare DOMSTRING_SIZE_ERR: 2;
	declare HIERARCHY_REQUEST_ERR: 3;
	declare WRONG_DOCUMENT_ERR: 4;
	declare INVALID_CHARACTER_ERR: 5;
	declare NO_DATA_ALLOWED_ERR: 6;
	declare NO_MODIFICATION_ALLOWED_ERR: 7;
	declare NOT_FOUND_ERR: 8;
	declare NOT_SUPPORTED_ERR: 9;
	declare INUSE_ATTRIBUTE_ERR: 10;
	declare INVALID_STATE_ERR: 11;
	declare SYNTAX_ERR: 12;
	declare INVALID_MODIFICATION_ERR: 13;
	declare NAMESPACE_ERR: 14;
	declare INVALID_ACCESS_ERR: 15;
	declare VALIDATION_ERR: 16;
	declare TYPE_MISMATCH_ERR: 17;
	declare SECURITY_ERR: 18;
	declare NETWORK_ERR: 19;
	declare ABORT_ERR: 20;
	declare URL_MISMATCH_ERR: 21;
	declare QUOTA_EXCEEDED_ERR: 22;
	declare TIMEOUT_ERR: 23;
	declare INVALID_NODE_TYPE_ERR: 24;
	declare DATA_CLONE_ERR: 25;

	declare static INDEX_SIZE_ERR: 1;
	declare static DOMSTRING_SIZE_ERR: 2;
	declare static HIERARCHY_REQUEST_ERR: 3;
	declare static WRONG_DOCUMENT_ERR: 4;
	declare static INVALID_CHARACTER_ERR: 5;
	declare static NO_DATA_ALLOWED_ERR: 6;
	declare static NO_MODIFICATION_ALLOWED_ERR: 7;
	declare static NOT_FOUND_ERR: 8;
	declare static NOT_SUPPORTED_ERR: 9;
	declare static INUSE_ATTRIBUTE_ERR: 10;
	declare static INVALID_STATE_ERR: 11;
	declare static SYNTAX_ERR: 12;
	declare static INVALID_MODIFICATION_ERR: 13;
	declare static NAMESPACE_ERR: 14;
	declare static INVALID_ACCESS_ERR: 15;
	declare static VALIDATION_ERR: 16;
	declare static TYPE_MISMATCH_ERR: 17;
	declare static SECURITY_ERR: 18;
	declare static NETWORK_ERR: 19;
	declare static ABORT_ERR: 20;
	declare static URL_MISMATCH_ERR: 21;
	declare static QUOTA_EXCEEDED_ERR: 22;
	declare static TIMEOUT_ERR: 23;
	declare static INVALID_NODE_TYPE_ERR: 24;
	declare static DATA_CLONE_ERR: 25;

	code: number;

	constructor(message?: string, name?: string) {
		super(message);
		this.name = name ?? 'Error';
		this.code = (names as readonly string[]).indexOf(this.name) + 1;
	}
}
def(DOMException);
defineNames(DOMException);
defineNames(DOMException.prototype);

function defineNames(o: any) {
	Object.defineProperties(o, {
		INDEX_SIZE_ERR: { value: 1, enumerable: true },
		DOMSTRING_SIZE_ERR: { value: 2, enumerable: true },
		HIERARCHY_REQUEST_ERR: { value: 3, enumerable: true },
		WRONG_DOCUMENT_ERR: { value: 4, enumerable: true },
		INVALID_CHARACTER_ERR: { value: 5, enumerable: true },
		NO_DATA_ALLOWED_ERR: { value: 6, enumerable: true },
		NO_MODIFICATION_ALLOWED_ERR: { value: 7, enumerable: true },
		NOT_FOUND_ERR: { value: 8, enumerable: true },
		NOT_SUPPORTED_ERR: { value: 9, enumerable: true },
		INUSE_ATTRIBUTE_ERR: { value: 10, enumerable: true },
		INVALID_STATE_ERR: { value: 11, enumerable: true },
		SYNTAX_ERR: { value: 12, enumerable: true },
		INVALID_MODIFICATION_ERR: { value: 13, enumerable: true },
		NAMESPACE_ERR: { value: 14, enumerable: true },
		INVALID_ACCESS_ERR: { value: 15, enumerable: true },
		VALIDATION_ERR: { value: 16, enumerable: true },
		TYPE_MISMATCH_ERR: { value: 17, enumerable: true },
		SECURITY_ERR: { value: 18, enumerable: true },
		NETWORK_ERR: { value: 19, enumerable: true },
		ABORT_ERR: { value: 20, enumerable: true },
		URL_MISMATCH_ERR: { value: 21, enumerable: true },
		QUOTA_EXCEEDED_ERR: { value: 22, enumerable: true },
		TIMEOUT_ERR: { value: 23, enumerable: true },
		INVALID_NODE_TYPE_ERR: { value: 24, enumerable: true },
		DATA_CLONE_ERR: { value: 25, enumerable: true },
	});
}
