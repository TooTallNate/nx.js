import colorRgba = require('color-rgba');
import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, assertInternalConstructor, def } from '../utils';

interface CanvasGradientInternal {
	opaque: any;
}

const _ = createInternal<CanvasGradient, CanvasGradientInternal>();

export class CanvasGradient {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		_.set(this, { opaque: arguments[1] });
	}

	/**
	 * Adds a new color stop, defined by an offset and a color, to a given canvas gradient.
	 *
	 * @param offset A number between `0` and `1`, inclusive, representing the position of the color stop. `0` represents the start of the gradient and `1` represents the end.
	 * @param color A CSS color string representing the color of the stop.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasGradient/addColorStop
	 */
	addColorStop(offset: number, color: string): void {
		if (offset < 0 || offset > 1) {
			throw new DOMException(
				"Failed to execute 'addColorStop' on 'CanvasGradient': The provided value (" +
					offset +
					') is outside the range (0, 1).',
				'IndexSizeError',
			);
		}
		const parsed = colorRgba(color);
		if (!parsed || parsed.length !== 4) {
			throw new SyntaxError(
				"Failed to execute 'addColorStop' on 'CanvasGradient': The value provided ('" +
					color +
					"') could not be parsed as a color.",
			);
		}
		const opaque = _(this).opaque;
		// addColorStop is a method on the native opaque object
		opaque.addColorStop(offset, parsed[0], parsed[1], parsed[2], parsed[3]);
	}

	/**
	 * @internal
	 */
	get _opaque() {
		return _(this).opaque;
	}
}
$.canvasGradientInitClass(CanvasGradient);
def(CanvasGradient);
