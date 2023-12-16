import type { connect } from './tcp';
import type { SocketOptions } from './switch';

export const INTERNAL_SYMBOL = Symbol('Internal');

export type Opaque<T> = { __type: T };
export type WasmModuleOpaque = Opaque<'WasmModuleOpaque'>;
export type WasmInstanceOpaque = Opaque<'WasmInstanceOpaque'>;
export type WasmGlobalOpaque = Opaque<'WasmGlobalOpaque'>;

export type Callback<T> = (err: Error | null, result: T) => void;

export type CallbackReturnType<T> = T extends (
	fn: Callback<infer U>,
	...args: any[]
) => any
	? U
	: never;

export type CallbackArguments<T> = T extends (
	fn: Callback<any>,
	...args: infer U
) => any
	? U
	: never;

export interface SocketOptionsInternal extends SocketOptions {
	connect: typeof connect;
}

export type RGBA = [number, number, number, number];

export type Keys = {
	modifiers: bigint;
	[i: number]: bigint;
};
