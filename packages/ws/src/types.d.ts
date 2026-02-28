// Type declarations for APIs provided by the nx.js runtime.
// These are declared locally to avoid a circular dependency
// between @nx.js/ws and @nx.js/runtime.

interface Uint8Array {
	toBase64(options?: {
		alphabet?: 'base64' | 'base64url';
		omitPadding?: boolean;
	}): string;
}

declare namespace Switch {
	interface SocketEvent {
		socket: Socket;
	}

	interface Socket {
		readonly readable: ReadableStream<Uint8Array>;
		readonly writable: WritableStream<Uint8Array>;
		close(): void;
	}

	interface ListenOptions {
		port: number;
		ip?: string;
		accept: (event: SocketEvent) => void;
	}

	interface Server {
		close(): void;
	}

	function listen(opts: ListenOptions): Server;
}
