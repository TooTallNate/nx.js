import { def } from '../utils';
import * as streams from 'web-streams-polyfill/ponyfill/es2018';
import type { AbortSignal } from './abort-controller';

for (const k of Object.keys(streams)) {
	def(k, streams[k as keyof typeof streams]);
}

export type ReadableStreamController<T> =
	| ReadableStreamDefaultController<T>
	| ReadableByteStreamController;
export type ReadableStreamReadResult<T> =
	| ReadableStreamReadValueResult<T>
	| ReadableStreamReadDoneResult<T>;
export type ReadableStreamReader<T> =
	| ReadableStreamDefaultReader<T>
	| ReadableStreamBYOBReader;

export interface ReadableStreamReadDoneResult<T> {
	done: true;
	value?: T;
}

export interface ReadableStreamReadValueResult<T> {
	done: false;
	value: T;
}

export interface UnderlyingByteSource {
	autoAllocateChunkSize?: number;
	cancel?: UnderlyingSourceCancelCallback;
	pull?: (
		controller: ReadableByteStreamController
	) => void | PromiseLike<void>;
	start?: (controller: ReadableByteStreamController) => any;
	type: 'bytes';
}

export interface UnderlyingDefaultSource<R = any> {
	cancel?: UnderlyingSourceCancelCallback;
	pull?: (
		controller: ReadableStreamDefaultController<R>
	) => void | PromiseLike<void>;
	start?: (controller: ReadableStreamDefaultController<R>) => any;
	type?: undefined;
}

export interface TransformerFlushCallback<O> {
	(controller: TransformStreamDefaultController<O>): void | PromiseLike<void>;
}

export interface TransformerStartCallback<O> {
	(controller: TransformStreamDefaultController<O>): any;
}

export interface TransformerTransformCallback<I, O> {
	(
		chunk: I,
		controller: TransformStreamDefaultController<O>
	): void | PromiseLike<void>;
}

export interface UnderlyingSinkAbortCallback {
	(reason?: any): void | PromiseLike<void>;
}

export interface UnderlyingSinkCloseCallback {
	(): void | PromiseLike<void>;
}

export interface UnderlyingSinkStartCallback {
	(controller: WritableStreamDefaultController): any;
}

export interface UnderlyingSinkWriteCallback<W> {
	(
		chunk: W,
		controller: WritableStreamDefaultController
	): void | PromiseLike<void>;
}

export interface UnderlyingSourceCancelCallback {
	(reason?: any): void | PromiseLike<void>;
}

export interface UnderlyingSourcePullCallback<R> {
	(controller: ReadableStreamController<R>): void | PromiseLike<void>;
}

export interface UnderlyingSourceStartCallback<R> {
	(controller: ReadableStreamController<R>): any;
}

export interface UnderlyingSink<W = any> {
	abort?: UnderlyingSinkAbortCallback;
	close?: UnderlyingSinkCloseCallback;
	start?: UnderlyingSinkStartCallback;
	type?: undefined;
	write?: UnderlyingSinkWriteCallback<W>;
}

export interface UnderlyingSource<R = any> {
	autoAllocateChunkSize?: number;
	cancel?: UnderlyingSourceCancelCallback;
	pull?: UnderlyingSourcePullCallback<R>;
	start?: UnderlyingSourceStartCallback<R>;
	type?: ReadableStreamType;
}

export interface QueuingStrategySize<T = any> {
	(chunk: T): number;
}

export interface QueuingStrategy<T = any> {
	highWaterMark?: number;
	size?: QueuingStrategySize<T>;
}

export interface QueuingStrategyInit {
	/**
	 * Creates a new ByteLengthQueuingStrategy with the provided high water mark.
	 *
	 * Note that the provided high water mark will not be validated ahead of time. Instead, if it is negative, NaN, or not a number, the resulting ByteLengthQueuingStrategy will cause the corresponding stream constructor to throw.
	 */
	highWaterMark: number;
}

export interface ReadableStreamGenericReader {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBReader/closed) */
	readonly closed: Promise<undefined>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBReader/cancel) */
	cancel(reason?: any): Promise<void>;
}

export interface Transformer<I = any, O = any> {
	flush?: TransformerFlushCallback<O>;
	readableType?: undefined;
	start?: TransformerStartCallback<O>;
	transform?: TransformerTransformCallback<I, O>;
	writableType?: undefined;
}

/**
 * This Streams API interface provides a built-in byte length queuing strategy that can be used when constructing streams.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/ByteLengthQueuingStrategy)
 */
export declare class ByteLengthQueuingStrategy
	implements
		QueuingStrategy<ArrayBufferView>,
		globalThis.ByteLengthQueuingStrategy
{
	constructor(init: QueuingStrategyInit);
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ByteLengthQueuingStrategy/highWaterMark) */
	readonly highWaterMark: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ByteLengthQueuingStrategy/size) */
	readonly size: QueuingStrategySize<ArrayBufferView>;
}

/**
 * This Streams API interface provides a built-in byte length queuing strategy that can be used when constructing streams.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/CountQueuingStrategy)
 */
export declare class CountQueuingStrategy
	implements QueuingStrategy, globalThis.CountQueuingStrategy
{
	constructor(init: QueuingStrategyInit);
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/CountQueuingStrategy/highWaterMark) */
	readonly highWaterMark: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/CountQueuingStrategy/size) */
	readonly size: QueuingStrategySize;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController) */
export declare class ReadableByteStreamController
	implements globalThis.ReadableByteStreamController
{
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController/byobRequest) */
	readonly byobRequest: ReadableStreamBYOBRequest | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController/desiredSize) */
	readonly desiredSize: number | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController/close) */
	close(): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController/enqueue) */
	enqueue(chunk: ArrayBufferView): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableByteStreamController/error) */
	error(e?: any): void;
}

/**
 * This Streams API interface represents a readable stream of byte data.
 * The Fetch API offers a concrete instance of a ReadableStream through
 * the body property of a Response object.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream)
 */
export declare class ReadableStream<R = any>
	implements globalThis.ReadableStream
{
	constructor(
		underlyingSource: UnderlyingByteSource,
		strategy?: { highWaterMark?: number }
	);
	constructor(
		underlyingSource: UnderlyingDefaultSource<R>,
		strategy?: QueuingStrategy<R>
	);
	constructor(
		underlyingSource?: UnderlyingSource<R>,
		strategy?: QueuingStrategy<R>
	);

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/locked) */
	readonly locked: boolean;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/cancel) */
	cancel(reason?: any): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/getReader) */
	getReader(options: { mode: 'byob' }): ReadableStreamBYOBReader;
	getReader(): ReadableStreamDefaultReader<R>;
	getReader(
		options?: ReadableStreamGetReaderOptions
	): ReadableStreamReader<R>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/pipeThrough) */
	pipeThrough<T>(
		transform: ReadableWritablePair<T, R>,
		options?: StreamPipeOptions
	): ReadableStream<T>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/pipeTo) */
	pipeTo(
		destination: WritableStream<R>,
		options?: StreamPipeOptions
	): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStream/tee) */
	tee(): [ReadableStream<R>, ReadableStream<R>];

	[Symbol.asyncIterator](): AsyncIterableIterator<R>;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBReader) */
export declare class ReadableStreamBYOBReader
	implements ReadableStreamGenericReader, globalThis.ReadableStreamBYOBReader
{
	constructor(stream: ReadableStream);
	closed: Promise<undefined>;
	cancel(reason?: any): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBReader/read) */
	read<T extends ArrayBufferView>(
		view: T
	): Promise<ReadableStreamReadResult<T>>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBReader/releaseLock) */
	releaseLock(): void;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBRequest) */
export declare class ReadableStreamBYOBRequest
	implements globalThis.ReadableStreamBYOBRequest
{
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBRequest/view) */
	readonly view: ArrayBufferView | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBRequest/respond) */
	respond(bytesWritten: number): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamBYOBRequest/respondWithNewView) */
	respondWithNewView(view: ArrayBufferView): void;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultController) */
export declare class ReadableStreamDefaultController<R = any>
	implements globalThis.ReadableStreamDefaultController<R>
{
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultController/desiredSize) */
	readonly desiredSize: number | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultController/close) */
	close(): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultController/enqueue) */
	enqueue(chunk?: R): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultController/error) */
	error(e?: any): void;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultReader) */
export declare class ReadableStreamDefaultReader<R = any>
	implements
		ReadableStreamGenericReader,
		globalThis.ReadableStreamDefaultReader<R>
{
	closed: Promise<undefined>;
	cancel(reason?: any): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultReader/read) */
	read(): Promise<ReadableStreamReadResult<R>>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/ReadableStreamDefaultReader/releaseLock) */
	releaseLock(): void;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStream) */
export declare class TransformStream<I = any, O = any> {
	constructor(
		transformer?: Transformer<I, O>,
		writableStrategy?: QueuingStrategy<I>,
		readableStrategy?: QueuingStrategy<O>
	);
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStream/readable) */
	readonly readable: ReadableStream<O>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStream/writable) */
	readonly writable: WritableStream<I>;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStreamDefaultController) */
export declare class TransformStreamDefaultController<O = any> {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStreamDefaultController/desiredSize) */
	readonly desiredSize: number | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStreamDefaultController/enqueue) */
	enqueue(chunk?: O): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStreamDefaultController/error) */
	error(reason?: any): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TransformStreamDefaultController/terminate) */
	terminate(): void;
}

/**
 * This Streams API interface provides a standard abstraction for writing streaming data to a destination, known as a sink. This object comes with built-in backpressure and queuing.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStream)
 */
export declare class WritableStream<W = any> {
	constructor(
		underlyingSink?: UnderlyingSink<W>,
		strategy?: QueuingStrategy<W>
	);
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStream/locked) */
	readonly locked: boolean;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStream/abort) */
	abort(reason?: any): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStream/close) */
	close(): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStream/getWriter) */
	getWriter(): WritableStreamDefaultWriter<W>;
}

/**
 * This Streams API interface represents a controller allowing control of a WritableStream's state. When constructing a WritableStream, the underlying sink is given a corresponding WritableStreamDefaultController instance to manipulate.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultController)
 */
export declare class WritableStreamDefaultController {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultController/signal) */
	readonly signal: AbortSignal;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultController/error) */
	error(e?: any): void;
}

/**
 * This Streams API interface is the object returned by WritableStream.getWriter() and once created locks the < writer to the WritableStream ensuring that no other streams can write to the underlying sink.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter)
 */
export declare class WritableStreamDefaultWriter<W = any> {
	constructor(stream: WritableStream<W>);
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/closed) */
	readonly closed: Promise<undefined>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/desiredSize) */
	readonly desiredSize: number | null;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/ready) */
	readonly ready: Promise<undefined>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/abort) */
	abort(reason?: any): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/close) */
	close(): Promise<void>;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/releaseLock) */
	releaseLock(): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/WritableStreamDefaultWriter/write) */
	write(chunk?: W): Promise<void>;
}
