import { u8 } from '@nx.js/util';
import { FsFileSystemType } from '@nx.js/constants';
import {
	NcmContentId,
	NcmStorageId,
	NcmContentStorage,
	NcmContentMetaDatabase,
	NcmContentMetaKey,
	NcmContentInstallType,
	NcmContentType,
	NcmContentMetaHeader,
	NcmContentInfo,
	NcmContentStorageRecord,
	NcmContentMetaType,
	NcmPatchMetaExtendedHeader,
	NcmPackagedContentInfo,
	NcmApplicationMetaExtendedHeader,
} from '@nx.js/ncm';
import { parseNsp, type FileEntry } from '@tootallnate/nsp';
import {
	NsApplicationRecordType,
	nsDeleteApplicationRecord,
	nsInvalidateApplicationControlCache,
	nsListApplicationRecordContentMeta,
	nsPushApplicationRecord,
} from './ipc/ns';
import { esImportTicket } from './ipc/es';
import { PackagedContentMetaHeader } from './types';

export interface StepStart {
	type: 'start';
	name: string;
	timeStart: number;
}

export interface StepEnd {
	type: 'end';
	name: string;
	timeStart: number;
	timeEnd: number;
}

export interface StepProgress {
	type: 'progress';
	name: string;
	processed: number;
	total: number;
}

export type Step = StepStart | StepEnd | StepProgress;

async function* step<T>(
	name: string,
	fn: () => AsyncIterable<Step, T>,
): AsyncIterable<Step, T> {
	const timeStart = performance.now();
	yield { type: 'start', name, timeStart };
	try {
		return yield* fn();
	} finally {
		yield { type: 'end', name, timeStart, timeEnd: performance.now() };
	}
}

// Install NSP
export async function* installNsp(
	data: Blob,
	storageId: NcmStorageId,
): AsyncIterable<Step, void> {
	const contentStorage = NcmContentStorage.open(storageId);
	const contentMetaDatabase = NcmContentMetaDatabase.open(storageId);

	// The `.nca` files that will be installed
	const ncaFiles = new Set<string>();

	// Map of title IDs found in the `.cnmt.nca` files and the accompanying
	// `NcmContentMetaKey` instances that will be recorded
	const titleIdToContentMetaKeysMap = new Map<bigint, NcmContentMetaKey[]>();

	const nsp = yield* step('Parse NSP', async function* () {
		return parseNsp(data);
	});

	// Prepare metadata from the `.cnmt.nca` file(s)
	const cnmtNcaFiles = nsp.files
		.entries()
		.filter(([name]) => name.endsWith('.cnmt.nca'));
	for (const [name, entry] of cnmtNcaFiles) {
		// Install the `.nca` so that we can mount the
		// filesystem to read the `.cnmt` file inside
		await installNca(name, entry, contentStorage);

		// Read the decrypted `.cnmt` file
		const cnmtData = await readContentMeta(name, contentStorage);

		const contentMetaKey = await installContentMetaRecords(
			contentMetaDatabase,
			cnmtData,
			createMetaContentInfo(name, entry),
			ncaFiles,
		);

		// Add the content meta key to add to the application record later
		const cnmtHeader = new PackagedContentMetaHeader(cnmtData);
		const titleId = getBaseTitleId(cnmtHeader.titleId, cnmtHeader.type);
		let contentMetaKeys = titleIdToContentMetaKeysMap.get(titleId);
		if (!contentMetaKeys) {
			contentMetaKeys = [];
			titleIdToContentMetaKeysMap.set(titleId, contentMetaKeys);
		}
		contentMetaKeys.push(contentMetaKey);
	}

	// Install Tickets / Certificates
	yield* importTicketCerts(nsp.files);

	// Install NCAs files
	for (const name of ncaFiles) {
		const entry = nsp.files.get(name);
		if (!entry) {
			throw new Error(`Missing "${name}" file`);
		}
		await installNca(name, entry, contentStorage);
	}

	// Push application records
	for (const [titleId, contentMetaKeys] of titleIdToContentMetaKeysMap) {
		await pushApplicationRecord(titleId, contentMetaKeys, storageId);
	}
}

class ContentStoragePlaceholderWriteStream extends WritableStream<Uint8Array> {
	#offset: bigint;

	constructor(
		contentStorage: NcmContentStorage,
		contentId: NcmContentId,
		size: bigint,
	) {
		const placeholderId = contentStorage.generatePlaceHolderId();
		try {
			contentStorage.deletePlaceHolder(placeholderId);
		} catch {}
		contentStorage.createPlaceHolder(contentId, placeholderId, BigInt(size));

		super({
			write: (chunk) => {
				console.debug(`Writing ${chunk.byteLength} bytes (${this.#offset})`);
				contentStorage.writePlaceHolder(placeholderId, this.#offset, chunk);
				this.#offset += BigInt(chunk.byteLength);
			},
			close: () => {
				console.debug(`Finished writing ${this.#offset} bytes`);
				try {
					contentStorage.delete(contentId);
				} catch {}
				contentStorage.register(contentId, placeholderId);
			},
		});

		this.#offset = 0n;
	}
}

function createContentMetaKey(
	header: PackagedContentMetaHeader,
): NcmContentMetaKey {
	const key = new NcmContentMetaKey();
	key.id = header.titleId;
	key.version = header.version;
	key.type = header.type;
	key.installType = NcmContentInstallType.Full;
	return key;
}

function createMetaContentInfo(name: string, entry: FileEntry): NcmContentInfo {
	const info = new NcmContentInfo();
	info.contentId = NcmContentId.from(name);
	info.size = entry.size;
	info.contentType = NcmContentType.Meta;
	return info;
}

async function installNca(
	name: string,
	entry: FileEntry,
	contentStorage: NcmContentStorage,
) {
	console.debug(`Installing "${name}" (${entry.size} bytes)`);
	const contentId = NcmContentId.from(name);
	await entry.data
		.stream()
		.pipeTo(
			new ContentStoragePlaceholderWriteStream(
				contentStorage,
				contentId,
				entry.size,
			),
		);
}

async function* importTicketCerts(
	files: Map<string, FileEntry>,
): AsyncIterable<Step, void> {
	const ticketFiles = files.keys().filter((name) => name.endsWith('.tik'));
	for (const tikName of ticketFiles) {
		yield* step(`Importing ticket "${tikName}"`, async function* () {
			const hash = tikName.slice(0, -4);
			const tikEntry = files.get(tikName);
			if (!tikEntry) {
				throw new Error(`Missing "${tikName}" file`);
			}

			const certName = `${hash}.cert`;
			const certEntry = files.get(certName);
			if (!certEntry) {
				throw new Error(`Missing "${certName}" file`);
			}

			const tik = await tikEntry.data.arrayBuffer();
			const cert = await certEntry.data.arrayBuffer();

			console.debug(`Importing "${tikName}" and "${certName}" certificate`);
			esImportTicket(tik, cert);
		});
	}
}

async function readContentMeta(
	name: string,
	contentStorage: NcmContentStorage,
) {
	const contentId = NcmContentId.from(name);
	const path = contentStorage.getPath(contentId);
	console.debug(`Mounting path "${path}"`);

	const fs = Switch.FileSystem.openWithId(
		0n,
		FsFileSystemType.ContentMeta,
		path,
	);
	const url = fs.mount();
	const files = Switch.readDirSync(url);
	if (!files) {
		throw new Error('Failed to read directory');
	}
	const cnmtName = files.find((name) => name.endsWith('.cnmt'));
	if (!cnmtName) {
		throw new Error('Failed to find ".cnmt" file');
	}
	console.debug(`Reading "${cnmtName}" file`);
	const cnmt = Switch.readFileSync(new URL(cnmtName, url));
	if (!cnmt) {
		throw new Error('Failed to read ".cnmt" file');
	}
	return cnmt;
}

async function installContentMetaRecords(
	contentMetaDatabase: NcmContentMetaDatabase,
	cnmtData: ArrayBuffer,
	cnmtContentInfo: NcmContentInfo,
	ncaFilesToInstall: Set<string>,
): Promise<NcmContentMetaKey> {
	const contentInfos: NcmContentInfo[] = [];
	const cnmtHeader = new PackagedContentMetaHeader(cnmtData);
	const key = createContentMetaKey(cnmtHeader);
	const extendedHeaderSize = cnmtHeader.extendedHeaderSize;
	const type = cnmtHeader.type;
	let extendedDataSize = 0;

	// Add a `NcmContentInfo` for the `.cnmt.nca` file, since we installed it
	contentInfos.push(cnmtContentInfo);

	// Add content records from `.cnmt` file
	const contentCount = cnmtHeader.contentCount;
	for (let i = 0; i < contentCount; i++) {
		const packagedContentInfoOffset =
			PackagedContentMetaHeader.sizeof +
			extendedHeaderSize +
			i * NcmPackagedContentInfo.sizeof;
		const packagedContentInfo = new NcmPackagedContentInfo(
			cnmtData,
			packagedContentInfoOffset,
		);
		const contentInfo = packagedContentInfo.contentInfo;

		// Don't install delta fragments. Even patches don't seem to install them.
		if (contentInfo.contentType === NcmContentType.DeltaFragment) {
			continue;
		}

		contentInfos.push(contentInfo);
		ncaFilesToInstall.add(`${contentInfo.contentId}.nca`);
	}

	if (type === NcmContentMetaType.Patch) {
		const patchMetaExtendedHeader = new NcmPatchMetaExtendedHeader(
			cnmtData,
			PackagedContentMetaHeader.sizeof,
		);
		extendedDataSize = patchMetaExtendedHeader.extendedDataSize;
	}

	const contentMetaData = new Uint8Array(
		NcmContentMetaHeader.sizeof +
			extendedHeaderSize +
			contentInfos.length * NcmContentInfo.sizeof +
			extendedDataSize,
	);

	// write header
	const contentMetaHeader = new NcmContentMetaHeader(contentMetaData);
	contentMetaHeader.extendedHeaderSize = extendedHeaderSize;
	contentMetaHeader.contentCount = contentInfos.length;
	contentMetaHeader.contentMetaCount = 0;
	contentMetaHeader.attributes = 0;
	contentMetaHeader.storageId = 0;

	// write extended header
	contentMetaData.set(
		new Uint8Array(
			cnmtData,
			PackagedContentMetaHeader.sizeof,
			extendedHeaderSize,
		),
		NcmContentMetaHeader.sizeof,
	);

	// Optionally disable the required system version field
	if (type === NcmContentMetaType.Application) {
		const extendedHeader = new NcmApplicationMetaExtendedHeader(
			contentMetaData,
			NcmContentMetaHeader.sizeof,
		);
		extendedHeader.requiredSystemVersion = 0;
	} else if (type === NcmContentMetaType.Patch) {
		const extendedHeader = new NcmPatchMetaExtendedHeader(
			contentMetaData,
			NcmContentMetaHeader.sizeof,
		);
		extendedHeader.requiredSystemVersion = 0;
	}

	// write content infos
	for (let i = 0; i < contentInfos.length; i++) {
		const offset =
			NcmContentMetaHeader.sizeof +
			extendedHeaderSize +
			i * NcmContentInfo.sizeof;
		contentMetaData.set(u8(contentInfos[i]), offset);
	}

	// write extended data
	if (extendedDataSize > 0) {
		contentMetaData.set(
			new Uint8Array(
				cnmtData,
				PackagedContentMetaHeader.sizeof +
					extendedHeaderSize +
					contentInfos.length * NcmContentInfo.sizeof,
				extendedDataSize,
			),
			NcmContentMetaHeader.sizeof +
				extendedHeaderSize +
				contentInfos.length * NcmContentInfo.sizeof,
		);
	}

	contentMetaDatabase.set(key, contentMetaData);
	contentMetaDatabase.commit();

	return key;
}

async function pushApplicationRecord(
	titleId: bigint,
	contentMetaKeys: NcmContentMetaKey[],
	storageId: NcmStorageId,
) {
	console.debug(
		`Pushing application record for "${titleId
			.toString(16)
			.padStart(16, '0')}" (${contentMetaKeys.length} content meta keys)`,
	);

	// TODO: for some reason, `nsCountApplicationContentMeta()` is returning 0.
	// So skip for now and rely on `listApplicationRecordContentMeta()` instead
	//let existingRecordCount = 0;
	//try {
	//	existingRecordCount = nsCountApplicationContentMeta(titleId);
	//} catch (err: unknown) {
	//	console.log(err);
	//	const recordDoesNotExist = false; // TODO for code 0x410
	//	if (!recordDoesNotExist) {
	//		throw err;
	//	}
	//}
	//console.debug(`Found ${existingRecordCount} existing content meta records`);

	const contentStorageRecords = new Uint8Array(
		NcmContentStorageRecord.sizeof * 20, // TODO: randomly selected max array length
	);

	let entriesRead = 0;
	try {
		entriesRead = nsListApplicationRecordContentMeta(
			0n,
			titleId,
			contentStorageRecords,
		);
		console.debug(`Found ${entriesRead} existing content meta records`);
	} catch (err: unknown) {
		//console.log('nsListApplicationRecordContentMeta', err);
	}

	// Add new content meta keys to the end of the array
	for (let i = 0; i < contentMetaKeys.length; i++) {
		const contentStorageRecord = new NcmContentStorageRecord(
			contentStorageRecords,
			(entriesRead + i) * NcmContentStorageRecord.sizeof,
		);
		contentStorageRecord.key = contentMetaKeys[i];
		contentStorageRecord.storageId = storageId;
	}

	try {
		nsDeleteApplicationRecord(titleId);
	} catch {}

	nsPushApplicationRecord(
		titleId,
		NsApplicationRecordType.Installed,
		contentStorageRecords.slice(
			0,
			(entriesRead + contentMetaKeys.length) * NcmContentStorageRecord.sizeof,
		),
	);

	// force flush
	if (entriesRead > 0) {
		nsInvalidateApplicationControlCache(titleId);
	}
}

function getBaseTitleId(titleId: bigint, type: NcmContentMetaType): bigint {
	switch (type) {
		case NcmContentMetaType.Patch:
			return titleId ^ 0x800n;

		case NcmContentMetaType.AddOnContent:
			return (titleId ^ 0x1000n) & ~0xfffn;

		default:
			return titleId;
	}
}
