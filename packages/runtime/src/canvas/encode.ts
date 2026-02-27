/**
 * Map MIME type string to integer type code for canvasToBuffer.
 * 0 = PNG (default), 1 = JPEG, 2 = WebP
 */
export function mimeToTypeCode(type?: string): number {
	switch (type) {
		case 'image/jpeg':
			return 1;
		case 'image/webp':
			return 2;
		default:
			return 0;
	}
}

/**
 * Return the actual MIME type that will be produced.
 * Unsupported types fall back to image/png per spec.
 */
export function resolvedMimeType(type?: string): string {
	switch (type) {
		case 'image/jpeg':
		case 'image/webp':
			return type;
		default:
			return 'image/png';
	}
}
