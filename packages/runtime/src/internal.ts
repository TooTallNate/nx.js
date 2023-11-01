export const INTERNAL_SYMBOL = Symbol('Internal');

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
