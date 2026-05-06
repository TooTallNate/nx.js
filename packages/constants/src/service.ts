/**
 * Bitmask of buffer attributes for an IPC service-framework (sf) call.
 * Used by [`Switch.Service`](/runtime/api/namespaces/Switch/classes/Service)
 * and [`ServiceDispatchParams`](/runtime/api/namespaces/Switch/interfaces/ServiceDispatchParams).
 */
export enum SfBufferAttr {
	/** Buffer is an input (sent from client to server). */
	In = 1 << 0,
	/** Buffer is an output (filled in by the server). */
	Out = 1 << 1,
	/** Buffer is sent via HIPC map alias (kernel maps the buffer into the server's address space). */
	HipcMapAlias = 1 << 2,
	/** Buffer is sent via HIPC pointer (small buffer copied through pointer descriptors). */
	HipcPointer = 1 << 3,
	/** Buffer has a fixed size known at compile time. */
	FixedSize = 1 << 4,
	/** Buffer transfer mode is auto-selected by the framework (alias vs. pointer). */
	HipcAutoSelect = 1 << 5,
	/** Map-alias transfer is allowed to be non-secure. */
	HipcMapTransferAllowsNonSecure = 1 << 6,
	/** Map-alias transfer is allowed to be non-device. */
	HipcMapTransferAllowsNonDevice = 1 << 7,
}

/**
 * How an output handle in an IPC reply is attributed to the client.
 */
export enum SfOutHandleAttr {
	/** No output handle. */
	None = 0,
	/** Output handle is a copy (server keeps its own reference). */
	HipcCopy = 1,
	/** Output handle is moved (transferred to client; server loses its reference). */
	HipcMove = 2,
}
