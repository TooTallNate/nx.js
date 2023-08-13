/**
 * @private
 */
export const INTERNAL_SYMBOL = Symbol('Internal');

export type PathLike = string | URL;

export interface Stats {
	size: number;
	mtime: number;
	atime: number;
	ctime: number;
	mode: number;
	uid: number;
	gid: number;
}
