import type { connect } from './tcp';
import type { SocketOptions, Vibration } from './switch';

export const INTERNAL_SYMBOL = Symbol('Internal');

export type Opaque<T> = { __type: T };
export type WasmModuleOpaque = Opaque<'WasmModuleOpaque'>;
export type WasmInstanceOpaque = Opaque<'WasmInstanceOpaque'>;
export type WasmGlobalOpaque = Opaque<'WasmGlobalOpaque'>;

export interface SocketOptionsInternal extends SocketOptions {
	connect: typeof connect;
}

export type RGBA = [number, number, number, number];

export type Keys = {
	modifiers: bigint;
	[i: number]: bigint;
};

export type VibrationValues = Omit<Vibration, 'duration'>;
