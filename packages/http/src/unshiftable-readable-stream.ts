import { Deferred } from './deferred';

export class UnshiftableStream {
	buffer: Uint8Array;
	reader: ReadableStreamDefaultReader<Uint8Array>;
	readable: ReadableStream<Uint8Array>;
	paused?: Deferred<void>;

	constructor(sourceStream: ReadableStream<Uint8Array>) {
		this.buffer = new Uint8Array();
		this.reader = sourceStream.getReader();
		this.pause();

		// Wrap the read method in a ReadableStream
		this.readable = new ReadableStream<Uint8Array>({
			pull: async (controller) => {
				const { done, value } = await this.read();
				if (done) {
					controller.close();
				} else {
					controller.enqueue(value);
				}
			},
			cancel: () => {
				this.reader.cancel();
			},
		});
	}

	// Method to unshift data back to the stream
	unshift = (data: Uint8Array) => {
		const newData = new Uint8Array(this.buffer.length + data.length);
		newData.set(data, 0);
		newData.set(this.buffer, data.length);
		this.buffer = newData;
	};

	pause() {
		if (this.paused) return;
		//console.log('pause');
		this.paused = new Deferred();
	}

	resume() {
		const p = this.paused;
		if (!p) return;
		//console.log('resume');
		this.paused = undefined;
		p.resolve();
	}

	// Read method that checks the buffer first
	async read() {
		//console.log('read')
		if (this.paused) {
			//console.log('read paused')
			await this.paused.promise;
		}
		//console.log('read resume')
		if (this.buffer.length > 0) {
			const value = this.buffer;
			//console.log('buffer', { value })
			this.buffer = new Uint8Array(); // Clear the buffer after reading
			return { done: false, value };
		} else {
			const result = await this.reader.read();
			//console.log({ result })
			return result; // Return data from the source stream
		}
	}
}
