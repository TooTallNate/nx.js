import { def } from '../utils';
import { Blob, type BlobPart } from './blob';

export class File extends Blob implements globalThis.File {
	name: string;
	lastModified: number;
	webkitRelativePath: string;

	constructor(
		fileParts: BlobPart[],
		fileName: string,
		options: FilePropertyBag = {}
	) {
		super(fileParts, options);
		this.name = fileName.replace(/\//g, ':');
		this.webkitRelativePath = '';
		this.lastModified = +(options.lastModified
			? new Date(options.lastModified)
			: new Date());
	}
}

def('File', File);
