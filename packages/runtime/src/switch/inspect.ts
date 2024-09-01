import { createInspect } from '@nx.js/inspect';
import { $ } from '../$';

export type { InspectOptions } from '@nx.js/inspect';

/**
 * Inspects a given value and returns a string representation of it.
 * The function uses ANSI color codes to highlight different parts of the output.
 * It can handle and correctly output different types of values including primitives, functions, arrays, and objects.
 *
 * @param v The value to inspect.
 * @param opts Options which may modify the generated string representation of the value.
 * @returns A string representation of `v` with ANSI color codes.
 */
export const inspect = createInspect({
	getPromiseState: $.getInternalPromiseState,
});
