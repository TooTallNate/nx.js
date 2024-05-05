import { def } from '../utils';
import { Blob, type BlobPart, type BlobPropertyBag } from './blob';

export interface FilePropertyBag extends BlobPropertyBag {
	lastModified?: number;
}

export class File extends Blob implements globalThis.File {
	name: string;
	lastModified: number;
	webkitRelativePath: string;

	constructor(
		fileParts: BlobPart[],
		name: string,
		options: FilePropertyBag = {},
	) {
		super(fileParts, options);
		this.name = name;
		this.webkitRelativePath = '';
		this.lastModified = +(options.lastModified
			? new Date(options.lastModified)
			: new Date());
	}
}

def(File);
