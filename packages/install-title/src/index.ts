import { FsFileSystemType } from '@nx.js/constants';
import {
	NcmApplicationMetaExtendedHeader,
	NcmContentId,
	NcmContentInfo,
	NcmContentInstallType,
	NcmContentMetaDatabase,
	NcmContentMetaHeader,
	NcmContentMetaKey,
	NcmContentMetaType,
	NcmContentStorage,
	NcmContentStorageRecord,
	NcmContentType,
	NcmPackagedContentInfo,
	NcmPatchMetaExtendedHeader,
	NcmStorageId,
} from '@nx.js/ncm';
import {
	NsApplicationRecordType,
	nsDeleteApplicationRecord,
	nsInvalidateApplicationControlCache,
	nsListApplicationRecordContentMeta,
	nsPushApplicationRecord,
} from '@nx.js/ns';
import { u8 } from '@nx.js/util';
import { decompressNcz } from '@tootallnate/ncz';
import { parseNsp } from '@tootallnate/nsp';
import { parseXci } from '@tootallnate/xci';
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

/**
 * Unified file entry that works for both PFS0 (NSP/NSZ) and HFS0 (XCI/XCZ) files.
 */
interface FileEntry {
	data: Blob;
	size: bigint;
}

export type ContainerFormat = 'nsp' | 'nsz' | 'xci' | 'xcz';

/**
 * Warning codes emitted during installation validation.
 */
export type InstallWarningCode =
	| 'already-installed'
	| 'nca-already-registered'
	| 'missing-ticket';

/**
 * A validation warning encountered during title installation.
 */
export interface InstallWarning {
	/** Machine-readable warning code. */
	code: InstallWarningCode;
	/** Human-readable description of the warning. */
	message: string;
	/** Additional context about the warning. */
	details: Record<string, unknown>;
}

/**
 * Callback invoked when a validation warning is encountered during installation.
 *
 * Return `true` to continue installation despite the warning, or `false` to abort.
 * If this callback is not provided, all warnings are treated as fatal errors.
 */
export type OnWarningCallback = (
	warning: InstallWarning,
) => Promise<boolean> | boolean;

export interface InstallOptions {
	/**
	 * Hint for the container format. If not provided, the format will be
	 * auto-detected from the magic bytes of the data.
	 */
	format?: ContainerFormat;

	/**
	 * Called when a validation warning is encountered during installation.
	 *
	 * Return `true` to continue, `false` to abort. If not provided,
	 * warnings are treated as fatal errors and abort the installation.
	 *
	 * @example
	 * ```typescript
	 * install(file, storageId, {
	 *   onWarning: async (warning) => {
	 *     console.warn(warning.message);
	 *     return confirm('Continue anyway?');
	 *   },
	 * });
	 * ```
	 */
	onWarning?: OnWarningCallback;
}

/**
 * Emit a warning via the `onWarning` callback. If the callback returns `false`
 * (or is not provided), throws an error to abort the installation.
 */
async function emitWarning(
	onWarning: OnWarningCallback | undefined,
	warning: InstallWarning,
): Promise<void> {
	if (onWarning) {
		const shouldContinue = await onWarning(warning);
		if (!shouldContinue) {
			throw new Error(`Installation aborted: ${warning.message}`);
		}
	} else {
		throw new Error(warning.message);
	}
}

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

/**
 * Detect the container format by reading magic bytes from the Blob.
 *
 * - PFS0 magic `0x50465330` at offset 0 → NSP/NSZ
 * - "HEAD" magic `0x48454144` at offset 0x100 → XCI/XCZ
 */
async function detectFormat(data: Blob): Promise<'pfs0' | 'xci'> {
	// Check for PFS0 magic at offset 0 ("PFS0" = 0x50465330)
	const pfs0Buf = await data.slice(0, 4).arrayBuffer();
	const pfs0Magic = new DataView(pfs0Buf).getUint32(0, false);
	if (pfs0Magic === 0x50465330) {
		return 'pfs0';
	}

	// Check for XCI "HEAD" magic at offset 0x100 (0x48454144)
	if (data.size >= 0x104) {
		const xciBuf = await data.slice(0x100, 0x104).arrayBuffer();
		const xciMagic = new DataView(xciBuf).getUint32(0, false);
		if (xciMagic === 0x48454144) {
			return 'xci';
		}
	}

	throw new Error('Unrecognized file format (expected NSP/NSZ or XCI/XCZ)');
}

/**
 * Parse a container (NSP/NSZ or XCI/XCZ) and return a unified file map.
 */
async function parseContainer(
	data: Blob,
	format: 'pfs0' | 'xci',
): Promise<Map<string, FileEntry>> {
	if (format === 'pfs0') {
		const nsp = await parseNsp(data);
		// Convert NspFileEntry to our unified FileEntry
		const files = new Map<string, FileEntry>();
		for (const [name, entry] of nsp.files) {
			files.set(name, { data: entry.data, size: entry.size });
		}
		return files;
	} else {
		const xci = await parseXci(data);
		// Convert Hfs0FileEntry to our unified FileEntry
		const files = new Map<string, FileEntry>();
		for (const [name, entry] of xci.files) {
			files.set(name, { data: entry.data, size: entry.size });
		}
		return files;
	}
}

// Install from any supported format (NSP, NSZ, XCI, XCZ)
/**
 * Install a title from a `Blob` containing an NSP, NSZ, XCI, or XCZ file.
 *
 * The format is auto-detected from magic bytes unless `options.format` is specified.
 * Yields `Step` progress events as the installation proceeds.
 *
 * NSZ and XCZ files contain NCZ-compressed NCAs, which are transparently
 * decompressed and re-encrypted during installation.
 *
 * @param data The file data as a `Blob`.
 * @param storageId Target storage (e.g. `NcmStorageId.SdCard`).
 * @param options Optional configuration.
 */
export async function* install(
	data: Blob,
	storageId: NcmStorageId,
	options?: InstallOptions,
): AsyncIterable<Step, void> {
	// Determine format
	let containerFormat: 'pfs0' | 'xci';
	if (options?.format) {
		containerFormat =
			options.format === 'xci' || options.format === 'xcz' ? 'xci' : 'pfs0';
	} else {
		containerFormat = await detectFormat(data);
	}

	const formatName = containerFormat === 'pfs0' ? 'NSP/NSZ' : 'XCI/XCZ';

	const { onWarning } = options ?? {};
	const contentStorage = NcmContentStorage.open(storageId);
	const contentMetaDatabase = NcmContentMetaDatabase.open(storageId);

	// Also check the other storage for already-installed detection
	const otherStorageId =
		storageId === NcmStorageId.SdCard
			? NcmStorageId.BuiltInUser
			: NcmStorageId.SdCard;
	let otherContentMetaDatabase: NcmContentMetaDatabase | null = null;
	try {
		otherContentMetaDatabase = NcmContentMetaDatabase.open(otherStorageId);
	} catch {
		// May not be available (e.g. no SD card inserted)
	}

	// The NCA files that will be installed (by name, e.g. "abc123.nca")
	const ncaFiles = new Set<string>();

	// Map of title IDs → NcmContentMetaKey instances for application records
	const titleIdToContentMetaKeysMap = new Map<bigint, NcmContentMetaKey[]>();

	// Parse the container
	const files = yield* step(`Parse ${formatName}`, async function* () {
		return parseContainer(data, containerFormat);
	});

	// Prepare metadata from the `.cnmt.nca` / `.cnmt.ncz` file(s)
	const cnmtNcaFiles = files
		.entries()
		.filter(
			([name]) => name.endsWith('.cnmt.nca') || name.endsWith('.cnmt.ncz'),
		);
	for (const [name, entry] of cnmtNcaFiles) {
		yield* step(`Install content meta "${name}"`, async function* () {
			// Install the NCA so that we can mount the
			// filesystem to read the `.cnmt` file inside.
			// Returns the actual installed NCA size (decompressed for NCZ).
			const installedSize = await installNca(name, entry, contentStorage);

			// The name registered in content storage is always `.nca`
			const ncaName = name.replace(/\.ncz$/, '.nca');

			// Read the decrypted `.cnmt` file
			const cnmtData = await readContentMeta(ncaName, contentStorage);

			const cnmtHeader = new PackagedContentMetaHeader(cnmtData);
			const contentMetaKey = createContentMetaKey(cnmtHeader);
			const titleIdHex = cnmtHeader.titleId.toString(16).padStart(16, '0');

			// --- Validation: already-installed check ---
			const isInstalledHere = contentMetaDatabase.has(contentMetaKey);
			const isInstalledOther =
				otherContentMetaDatabase?.has(contentMetaKey) ?? false;
			if (isInstalledHere || isInstalledOther) {
				const where = isInstalledHere
					? storageId === NcmStorageId.SdCard
						? 'SD card'
						: 'NAND'
					: otherStorageId === NcmStorageId.SdCard
						? 'SD card'
						: 'NAND';
				await emitWarning(onWarning, {
					code: 'already-installed',
					message: `Title ${titleIdHex} v${cnmtHeader.version} (${NcmContentMetaType[cnmtHeader.type]}) is already installed on ${where}`,
					details: {
						titleId: cnmtHeader.titleId,
						version: cnmtHeader.version,
						type: cnmtHeader.type,
						storage: isInstalledHere ? storageId : otherStorageId,
					},
				});
			}

			// Use the actual installed size (not compressed size) for meta content info
			const metaEntry = { ...entry, size: installedSize };
			const installedContentMetaKey = await installContentMetaRecords(
				contentMetaDatabase,
				cnmtData,
				createMetaContentInfo(ncaName, metaEntry),
				ncaFiles,
			);

			// Add the content meta key for the application record
			const titleId = getBaseTitleId(cnmtHeader.titleId, cnmtHeader.type);
			let contentMetaKeys = titleIdToContentMetaKeysMap.get(titleId);
			if (!contentMetaKeys) {
				contentMetaKeys = [];
				titleIdToContentMetaKeysMap.set(titleId, contentMetaKeys);
			}
			contentMetaKeys.push(installedContentMetaKey);
		});
	}

	// --- Validation: ticket/certificate consistency check ---
	// Ensure that for every .tik file in the container there is a
	// corresponding .cert file with the same base name. This does not
	// inspect NCA rights_ids or verify that tickets exist for specific
	// NCAs; it only checks that any shipped tickets have their matching
	// certificates present.
	const ticketNames = new Set(
		files.keys().filter((name) => name.endsWith('.tik')),
	);
	for (const tikName of ticketNames) {
		const certName = `${tikName.slice(0, -4)}.cert`;
		if (!files.has(certName)) {
			await emitWarning(onWarning, {
				code: 'missing-ticket',
				message: `Ticket "${tikName}" is missing its corresponding certificate "${certName}"`,
				details: { tikName, certName },
			});
		}
	}

	// Install Tickets / Certificates
	yield* importTicketCerts(files);

	// Install NCA files
	for (const ncaName of ncaFiles) {
		// The CNMT references files as `.nca`, but in NSZ/XCZ containers
		// they may be stored as `.ncz`. Try both.
		const nczName = ncaName.replace(/\.nca$/, '.ncz');
		const entry = files.get(ncaName) ?? files.get(nczName);
		if (!entry) {
			throw new Error(`Missing "${ncaName}" file`);
		}
		const actualName = files.has(ncaName) ? ncaName : nczName;

		// --- Validation: NCA already-registered check ---
		const contentId = NcmContentId.from(ncaName);
		if (contentStorage.has(contentId)) {
			await emitWarning(onWarning, {
				code: 'nca-already-registered',
				message: `NCA "${ncaName}" is already registered in content storage`,
				details: { ncaName, contentId: contentId.toString() },
			});
		}

		yield* step(`Install "${actualName}"`, async function* () {
			await installNca(actualName, entry, contentStorage);
		});
	}

	// Push application records
	for (const [titleId, contentMetaKeys] of titleIdToContentMetaKeysMap) {
		yield* step(
			`Push record for ${titleId.toString(16).padStart(16, '0')}`,
			async function* () {
				await pushApplicationRecord(titleId, contentMetaKeys, storageId);
			},
		);
	}
}

/**
 * Install a title from a `Blob` containing an NSP file.
 *
 * This is a convenience wrapper around `install()` that explicitly
 * sets the format to `'nsp'`.
 *
 * @param data The NSP file data as a `Blob`.
 * @param storageId Target storage (e.g. `NcmStorageId.SdCard`).
 */
export async function* installNsp(
	data: Blob,
	storageId: NcmStorageId,
): AsyncIterable<Step, void> {
	yield* install(data, storageId, { format: 'nsp' });
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

/**
 * Install an NCA (or NCZ) file into content storage.
 *
 * If the file is an NCZ (detected by `.ncz` extension), it is decompressed
 * and re-encrypted on the fly via `@tootallnate/ncz`, then written as a
 * standard NCA to the content storage placeholder.
 */
async function installNca(
	name: string,
	entry: FileEntry,
	contentStorage: NcmContentStorage,
): Promise<bigint> {
	const isNcz = name.endsWith('.ncz');
	const ncaName = isNcz ? name.replace(/\.ncz$/, '.nca') : name;
	const contentId = NcmContentId.from(ncaName);

	if (isNcz) {
		console.debug(`Decompressing and installing NCZ "${name}" as "${ncaName}"`);
		const { ncaSize } = await decompressNcz(
			entry.data,
			(ncaSize) => {
				console.debug(`NCZ decompressed NCA size: ${ncaSize}`);
				return new ContentStoragePlaceholderWriteStream(
					contentStorage,
					contentId,
					ncaSize,
				);
			},
			{
				decompressStream: (input) =>
					input.pipeThrough(
						new DecompressionStream('zstd' as CompressionFormat),
					),
				decompressBlob: async (blob) => {
					const ds = new DecompressionStream('zstd' as CompressionFormat);
					const decompressed = blob.stream().pipeThrough<Uint8Array>(ds);
					const response = new Response(decompressed);
					return new Uint8Array(await response.arrayBuffer());
				},
			},
		);
		return ncaSize;
	} else {
		console.debug(`Installing "${name}" (${entry.size} bytes)`);
		await entry.data
			.stream()
			.pipeTo(
				new ContentStoragePlaceholderWriteStream(
					contentStorage,
					contentId,
					entry.size,
				),
			);
		return entry.size;
	}
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
		// Always reference as `.nca` — NCZ lookup happens at install time
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
