import {
	$,
	type BtleCharacteristic,
	type BtleEvent,
	type BtleService,
} from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL as INTERNAL } from '../internal';
import { Event } from '../polyfills/event';
import { EventTarget } from '../polyfills/event-target';
import { clearInterval, clearTimeout, setInterval, setTimeout } from '../timers';
import { assertInternalConstructor, createInternal, def } from '../utils';

// Client Characteristic Configuration Descriptor.
const CCCD_UUID = '00002902-0000-1000-8000-00805f9b34fb';

export interface BluetoothManufacturerDataFilter {
	companyIdentifier: number;
	dataPrefix?: BufferSource;
	mask?: BufferSource;
}

export interface BluetoothLEScanFilter {
	name?: string;
	namePrefix?: string;
	services?: (string | number)[];
	manufacturerData?: BluetoothManufacturerDataFilter[];
}

export interface RequestDeviceOptions {
	filters?: BluetoothLEScanFilter[];
	optionalServices?: (string | number)[];
	acceptAllDevices?: boolean;
	/**
	 * nx.js extension: connect to a device by its known Bluetooth address
	 * (`"xx:xx:xx:xx:xx:xx"`), skipping the scan/selection step entirely.
	 * Useful when the application targets a specific accessory whose address
	 * is already known (the Switch's application BLE scanner is filter-based
	 * and cannot perform the unfiltered discovery browsers use).
	 */
	deviceId?: string;
}

// ---------------------------------------------------------------------------
// BluetoothUUID
// ---------------------------------------------------------------------------

const UUID_RE =
	/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

/**
 * Utility for converting between 16/32-bit Bluetooth SIG aliases and
 * canonical 128-bit UUID strings.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothUUID
 */
export class BluetoothUUID {
	/**
	 * Returns the canonical 128-bit UUID for a 16/32-bit Bluetooth alias.
	 */
	static canonicalUUID(alias: number): string {
		return `${(alias >>> 0).toString(16).padStart(8, '0')}-0000-1000-8000-00805f9b34fb`;
	}

	static getService(name: string | number): string {
		return toUUID(name);
	}

	static getCharacteristic(name: string | number): string {
		return toUUID(name);
	}

	static getDescriptor(name: string | number): string {
		return toUUID(name);
	}
}
def(BluetoothUUID);

function toUUID(value: string | number): string {
	if (typeof value === 'number') {
		return BluetoothUUID.canonicalUUID(value);
	}
	const lower = String(value).toLowerCase();
	if (!UUID_RE.test(lower)) {
		throw new TypeError(`Invalid Bluetooth UUID: "${value}"`);
	}
	return lower;
}

// ---------------------------------------------------------------------------
// Native init + event pump
// ---------------------------------------------------------------------------

let initialized = false;
function ensureInit() {
	if (initialized) return;
	$.btleInit();
	initialized = true;
	addEventListener('unload', () => {
		$.btleExit();
		initialized = false;
	});
}

interface PendingOp {
	kind: 'read' | 'write';
	resolve: (data: ArrayBuffer | undefined) => void;
	reject: (err: Error) => void;
	timer: ReturnType<typeof setTimeout>;
}

interface PumpState {
	interval: ReturnType<typeof setInterval> | null;
	refs: number;
	/** `${connId}:${charUuid}` -> FIFO of in-flight GATT operations. */
	pending: Map<string, PendingOp[]>;
	/** `${connId}:${charUuid}` -> characteristics with active notifications. */
	notify: Map<string, Set<BluetoothRemoteGATTCharacteristic>>;
	/** Devices with a connected (or connecting) GATT server, by address. */
	devices: Map<string, BluetoothDevice>;
	/** Callbacks invoked on every pump tick (connection/discovery waiters). */
	waiters: Set<() => void>;
}

const pump: PumpState = {
	interval: null,
	refs: 0,
	pending: new Map(),
	notify: new Map(),
	devices: new Map(),
	waiters: new Set(),
};

function pumpRef() {
	pump.refs++;
	if (pump.interval === null) {
		pump.interval = setInterval(pumpTick, 25);
	}
}

function pumpUnref() {
	pump.refs--;
	if (pump.refs <= 0 && pump.interval !== null) {
		clearInterval(pump.interval);
		pump.interval = null;
		pump.refs = 0;
	}
}

function opKey(connId: number, charUuid: string): string {
	return `${connId}:${charUuid}`;
}

function pumpTick() {
	let events: BtleEvent[];
	try {
		events = $.btlePollEvents();
	} catch {
		return;
	}
	for (const ev of events) {
		if (ev.type === 'gatt') {
			// `op` is the LeEventInfo flags byte: bit0 = IsWrite,
			// bit1 = IsDescriptor, bit2 = IsNotify (verified on-device:
			// write completions arrive as event type 8 too, so the flags —
			// not the event type — discriminate notifications).
			if (ev.op & 0x4) {
				routeNotification(ev.connId, ev.characteristicUuid, ev.data);
			} else {
				routeCompletion(ev.connId, ev.characteristicUuid, ev.data);
			}
		} else if (ev.type === 'connection') {
			syncConnections();
		}
	}
	for (const waiter of [...pump.waiters]) {
		waiter();
	}
}

function routeNotification(
	connId: number,
	charUuid: string,
	data: ArrayBuffer,
) {
	const set = pump.notify.get(opKey(connId, charUuid));
	if (!set) return;
	for (const char of set) {
		charInternal(char).value = new DataView(data.slice(0));
		char.dispatchEvent(new Event('characteristicvaluechanged'));
	}
}

function routeCompletion(connId: number, charUuid: string, data: ArrayBuffer) {
	const queue = pump.pending.get(opKey(connId, charUuid));
	const op = queue?.shift();
	if (!op) return;
	clearTimeout(op.timer);
	op.resolve(data);
}

// Reconcile JS connection state with btmu's, firing
// `gattserverdisconnected` for devices that dropped.
function syncConnections() {
	let connections;
	try {
		connections = $.btleConnections();
	} catch {
		return;
	}
	const byAddress = new Map(connections.map((c) => [c.address, c.handle]));
	for (const [address, device] of pump.devices) {
		const i = deviceInternal(device);
		const handle = byAddress.get(address);
		if (typeof handle === 'number') {
			i.connHandle = handle;
			i.connected = true;
		} else if (i.connected) {
			i.connected = false;
			i.connHandle = -1;
			pump.devices.delete(address);
			pumpUnref(); // ref held while connected
			device.dispatchEvent(new Event('gattserverdisconnected'));
		}
	}
}

// Wait (pump-driven) until `check` returns a value or the timeout elapses.
function waitFor<T>(
	check: () => T | undefined,
	timeoutMs: number,
	timeoutMessage: string,
): Promise<T> {
	return new Promise<T>((resolve, reject) => {
		const initial = check();
		if (typeof initial !== 'undefined') {
			resolve(initial);
			return;
		}
		pumpRef();
		const waiter = () => {
			const value = check();
			if (typeof value !== 'undefined') {
				cleanup();
				resolve(value);
			}
		};
		const timer = setTimeout(() => {
			cleanup();
			reject(new DOMException(timeoutMessage, 'NetworkError'));
		}, timeoutMs);
		const cleanup = () => {
			clearTimeout(timer);
			pump.waiters.delete(waiter);
			pumpUnref();
		};
		pump.waiters.add(waiter);
	});
}

// ---------------------------------------------------------------------------
// BluetoothDevice
// ---------------------------------------------------------------------------

interface BluetoothDeviceInternal {
	id: string; // Bluetooth address
	name: string | undefined;
	gatt: BluetoothRemoteGATTServer;
	connHandle: number;
	connected: boolean;
	/**
	 * True when the device was created by explicit address (the `deviceId`
	 * extension) and has not been seen by a scan: before connecting, an
	 * unfiltered btdrv-level scan primes the Bluetooth stack's device cache
	 * so `BleConnect` can resolve the peer's address type.
	 */
	needsCachePrime: boolean;
	/**
	 * Service UUIDs from the requestDevice() filters + optionalServices.
	 * Registered as BLE GATT data paths with btm before connecting (btm maps
	 * data paths to the GATT client interfaces that connections are keyed
	 * on).
	 */
	allowedServices: string[];
	dataPathsRegistered: boolean;
}

const deviceInternal = createInternal<
	BluetoothDevice,
	BluetoothDeviceInternal
>();

/**
 * Represents a single Bluetooth LE device, as returned by
 * {@link Bluetooth.requestDevice | `navigator.bluetooth.requestDevice()`}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothDevice
 */
export class BluetoothDevice extends EventTarget {
	ongattserverdisconnected:
		| ((this: BluetoothDevice, ev: Event) => any)
		| null;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		this.ongattserverdisconnected = null;
		const [, id, name, needsCachePrime, allowedServices] = arguments;
		// @ts-expect-error internal constructor
		const gatt = new BluetoothRemoteGATTServer(INTERNAL, this);
		deviceInternal.set(this, {
			id,
			name,
			gatt,
			connHandle: -1,
			connected: false,
			needsCachePrime: needsCachePrime === true,
			allowedServices: allowedServices ?? [],
			dataPathsRegistered: false,
		});
	}

	/**
	 * A unique identifier for the device (its Bluetooth address).
	 */
	get id(): string {
		return deviceInternal(this).id;
	}

	get name(): string | undefined {
		return deviceInternal(this).name;
	}

	get gatt(): BluetoothRemoteGATTServer {
		return deviceInternal(this).gatt;
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'gattserverdisconnected') {
			this.ongattserverdisconnected?.(event);
		}
		return super.dispatchEvent(event);
	}
}
def(BluetoothDevice);

// ---------------------------------------------------------------------------
// BluetoothRemoteGATTServer
// ---------------------------------------------------------------------------

interface GATTServerInternal {
	device: BluetoothDevice;
}

const serverInternal = createInternal<
	BluetoothRemoteGATTServer,
	GATTServerInternal
>();

/**
 * The GATT server of a remote Bluetooth LE device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTServer
 */
export class BluetoothRemoteGATTServer {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		serverInternal.set(this, { device: arguments[1] });
	}

	get device(): BluetoothDevice {
		return serverInternal(this).device;
	}

	get connected(): boolean {
		return deviceInternal(this.device).connected;
	}

	/**
	 * Connects to the device's GATT server and waits for the system to
	 * complete service discovery.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTServer/connect
	 */
	async connect(): Promise<BluetoothRemoteGATTServer> {
		const device = this.device;
		const i = deviceInternal(device);
		if (i.connected) return this;
		ensureInit();
		if (!i.dataPathsRegistered) {
			// Associate this app with the GATT services it intends to use —
			// btm maps registered data paths to the GATT client interfaces
			// that connections are keyed on.
			for (const uuid of i.allowedServices) {
				try {
					$.btleRegisterDataPath(uuid, true);
				} catch {}
			}
			i.dataPathsRegistered = true;
		}
		if (i.needsCachePrime) {
			// Device addressed explicitly (never seen by a scan): run an
			// unfiltered btdrv-level scan so the Bluetooth stack caches the
			// peer (and its address type — required for devices advertising
			// with random addresses) before BleConnect.
			$.btleRawScanStart();
			try {
				await new Promise((r) => setTimeout(r, 5000));
			} finally {
				$.btleRawScanStop();
			}
			i.needsCachePrime = false;
		}
		try {
			$.btleConnect(i.id);
		} catch {
			// May already be connected/connecting at the system level (e.g.
			// a racing or system-initiated connection) — the wait below
			// resolves from the connection list either way.
		}
		pump.devices.set(i.id, device);
		try {
			const handle = await waitFor(
				() => {
					const conn = $.btleConnections().find(
						(c) => c.address === i.id,
					);
					return conn?.handle;
				},
				15000,
				'Connection attempt timed out',
			);
			i.connHandle = handle;
			i.connected = true;
			// Hold a pump ref while connected, so disconnects/notifications
			// are observed.
			pumpRef();
			// Negotiate a large ATT MTU like browsers do (the default of 23
			// caps characteristic writes at 20 bytes, which Web Bluetooth
			// apps do not expect). Best-effort; the peer may cap it lower.
			try {
				$.btleConfigureMtu(handle, 247);
				await new Promise((r) => setTimeout(r, 500));
			} catch {}
			// Wait for GATT service discovery to populate (the system
			// performs discovery automatically after connecting).
			await waitFor(
				() => {
					try {
						const services = $.btleGetServices(handle);
						return services.length > 0 ? true : undefined;
					} catch {
						return undefined;
					}
				},
				10000,
				'GATT service discovery timed out',
			).catch(() => {
				// Some devices expose no services until first use — proceed.
			});
			return this;
		} catch (err) {
			pump.devices.delete(i.id);
			throw err;
		}
	}

	/**
	 * Disconnects from the device's GATT server.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTServer/disconnect
	 */
	disconnect(): void {
		const i = deviceInternal(this.device);
		if (!i.connected) return;
		try {
			$.btleDisconnect(i.connHandle);
		} catch {}
		// `gattserverdisconnected` fires via the connection event sync.
	}

	/**
	 * Returns the primary GATT service matching `service`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTServer/getPrimaryService
	 */
	async getPrimaryService(
		service: string | number,
	): Promise<BluetoothRemoteGATTService> {
		const services = await this.getPrimaryServices(service);
		return services[0];
	}

	/**
	 * Returns the primary GATT services, optionally filtered by UUID.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTServer/getPrimaryServices
	 */
	async getPrimaryServices(
		service?: string | number,
	): Promise<BluetoothRemoteGATTService[]> {
		const i = deviceInternal(this.device);
		if (!i.connected) {
			throw new DOMException(
				'GATT Server is disconnected. Cannot retrieve services.',
				'NetworkError',
			);
		}
		const uuid = typeof service !== 'undefined' ? toUUID(service) : null;
		const all = $.btleGetServices(i.connHandle);
		const matched = all.filter(
			(s) => s.primary && (uuid === null || s.uuid === uuid),
		);
		if (matched.length === 0) {
			throw new DOMException(
				uuid
					? `No Services matching UUID ${uuid} found in Device.`
					: 'No Services found in Device.',
				'NotFoundError',
			);
		}
		return matched.map((s) => {
			// @ts-expect-error internal constructor
			return new BluetoothRemoteGATTService(INTERNAL, this.device, s);
		});
	}
}
def(BluetoothRemoteGATTServer);

// ---------------------------------------------------------------------------
// BluetoothRemoteGATTService
// ---------------------------------------------------------------------------

interface GATTServiceInternal {
	device: BluetoothDevice;
	info: BtleService;
}

const serviceInternal = createInternal<
	BluetoothRemoteGATTService,
	GATTServiceInternal
>();

/**
 * A GATT service provided by a remote Bluetooth LE device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTService
 */
export class BluetoothRemoteGATTService {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		serviceInternal.set(this, { device: arguments[1], info: arguments[2] });
	}

	get device(): BluetoothDevice {
		return serviceInternal(this).device;
	}

	get uuid(): string {
		return serviceInternal(this).info.uuid;
	}

	get isPrimary(): boolean {
		return serviceInternal(this).info.primary;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTService/getCharacteristic
	 */
	async getCharacteristic(
		characteristic: string | number,
	): Promise<BluetoothRemoteGATTCharacteristic> {
		const chars = await this.getCharacteristics(characteristic);
		return chars[0];
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTService/getCharacteristics
	 */
	async getCharacteristics(
		characteristic?: string | number,
	): Promise<BluetoothRemoteGATTCharacteristic[]> {
		const i = serviceInternal(this);
		const dev = deviceInternal(i.device);
		if (!dev.connected) {
			throw new DOMException(
				'GATT Server is disconnected. Cannot retrieve characteristics.',
				'NetworkError',
			);
		}
		const uuid =
			typeof characteristic !== 'undefined'
				? toUUID(characteristic)
				: null;
		const all = $.btleGetCharacteristics(dev.connHandle, i.info.handle);
		const matched = all.filter((c) => uuid === null || c.uuid === uuid);
		if (matched.length === 0) {
			throw new DOMException(
				uuid
					? `No Characteristics matching UUID ${uuid} found in Service.`
					: 'No Characteristics found in Service.',
				'NotFoundError',
			);
		}
		return matched.map((c) => {
			// @ts-expect-error internal constructor
			return new BluetoothRemoteGATTCharacteristic(INTERNAL, this, c);
		});
	}
}
def(BluetoothRemoteGATTService);

// ---------------------------------------------------------------------------
// BluetoothCharacteristicProperties
// ---------------------------------------------------------------------------

const propsInternal = createInternal<
	BluetoothCharacteristicProperties,
	number
>();

/**
 * The properties of a GATT characteristic.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothCharacteristicProperties
 */
export class BluetoothCharacteristicProperties {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		propsInternal.set(this, arguments[1]);
	}

	get broadcast(): boolean {
		return (propsInternal(this) & 0x01) !== 0;
	}
	get read(): boolean {
		return (propsInternal(this) & 0x02) !== 0;
	}
	get writeWithoutResponse(): boolean {
		return (propsInternal(this) & 0x04) !== 0;
	}
	get write(): boolean {
		return (propsInternal(this) & 0x08) !== 0;
	}
	get notify(): boolean {
		return (propsInternal(this) & 0x10) !== 0;
	}
	get indicate(): boolean {
		return (propsInternal(this) & 0x20) !== 0;
	}
	get authenticatedSignedWrites(): boolean {
		return (propsInternal(this) & 0x40) !== 0;
	}
	get reliableWrite(): boolean {
		return false;
	}
	get writableAuxiliaries(): boolean {
		return false;
	}
}
def(BluetoothCharacteristicProperties);

// ---------------------------------------------------------------------------
// BluetoothRemoteGATTCharacteristic
// ---------------------------------------------------------------------------

interface GATTCharacteristicInternal {
	service: BluetoothRemoteGATTService;
	info: BtleCharacteristic;
	properties: BluetoothCharacteristicProperties;
	value: DataView | undefined;
	notifying: boolean;
}

const charInternal = createInternal<
	BluetoothRemoteGATTCharacteristic,
	GATTCharacteristicInternal
>();

function bufferSourceToArrayBuffer(value: BufferSource): ArrayBuffer {
	if (value instanceof ArrayBuffer) return value.slice(0);
	const view = value as ArrayBufferView;
	return view.buffer.slice(
		view.byteOffset,
		view.byteOffset + view.byteLength,
	) as ArrayBuffer;
}

/**
 * A GATT characteristic — the fundamental unit of data exchange with a
 * remote Bluetooth LE device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic
 */
export class BluetoothRemoteGATTCharacteristic extends EventTarget {
	oncharacteristicvaluechanged:
		| ((this: BluetoothRemoteGATTCharacteristic, ev: Event) => any)
		| null;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		this.oncharacteristicvaluechanged = null;
		const info = arguments[2] as BtleCharacteristic;
		const properties: BluetoothCharacteristicProperties =
			// @ts-expect-error internal constructor
			new BluetoothCharacteristicProperties(INTERNAL, info.properties);
		charInternal.set(this, {
			service: arguments[1],
			info,
			properties,
			value: undefined,
			notifying: false,
		});
	}

	get service(): BluetoothRemoteGATTService {
		return charInternal(this).service;
	}

	get uuid(): string {
		return charInternal(this).info.uuid;
	}

	get properties(): BluetoothCharacteristicProperties {
		return charInternal(this).properties;
	}

	/**
	 * The most recent value read from (or notified by) the characteristic.
	 */
	get value(): DataView | undefined {
		return charInternal(this).value;
	}

	#conn(): { handle: number; primary: boolean; svc: BtleService } {
		const i = charInternal(this);
		const svcI = serviceInternal(i.service);
		const dev = deviceInternal(svcI.device);
		if (!dev.connected) {
			throw new DOMException(
				'GATT Server is disconnected. Cannot perform GATT operations.',
				'NetworkError',
			);
		}
		return {
			handle: dev.connHandle,
			primary: svcI.info.primary,
			svc: svcI.info,
		};
	}

	/**
	 * Reads the characteristic's value from the remote device.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/readValue
	 */
	async readValue(): Promise<DataView> {
		const i = charInternal(this);
		const { handle, primary, svc } = this.#conn();
		const data = await this.#enqueueOp(handle, 'read', 10000, () => {
			$.btleRead(
				handle,
				primary,
				svc.uuid,
				svc.instanceId,
				i.info.uuid,
				i.info.instanceId,
			);
		});
		const view = new DataView(data ?? new ArrayBuffer(0));
		i.value = view;
		this.dispatchEvent(new Event('characteristicvaluechanged'));
		return view;
	}

	/**
	 * Writes to the characteristic using Write With Response.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/writeValueWithResponse
	 */
	async writeValueWithResponse(value: BufferSource): Promise<void> {
		await this.#write(value, true);
	}

	/**
	 * Writes to the characteristic using Write Without Response.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/writeValueWithoutResponse
	 */
	async writeValueWithoutResponse(value: BufferSource): Promise<void> {
		await this.#write(value, false);
	}

	/**
	 * Writes to the characteristic (legacy alias of
	 * {@link BluetoothRemoteGATTCharacteristic.writeValueWithResponse | `writeValueWithResponse()`}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/writeValue
	 */
	async writeValue(value: BufferSource): Promise<void> {
		await this.#write(value, true);
	}

	async #write(value: BufferSource, withResponse: boolean): Promise<void> {
		const i = charInternal(this);
		const { handle, primary, svc } = this.#conn();
		const buf = bufferSourceToArrayBuffer(value);
		const send = () => {
			$.btleWrite(
				handle,
				primary,
				svc.uuid,
				svc.instanceId,
				i.info.uuid,
				i.info.instanceId,
				buf,
				withResponse,
			);
		};
		if (!withResponse) {
			send();
			return;
		}
		// Wait for the write completion event; tolerate stacks that do not
		// surface one by resolving after a grace period.
		await this.#enqueueOp(handle, 'write', 1500, send).catch((err) => {
			if (err instanceof DOMException && err.name === 'NetworkError') {
				return undefined; // grace timeout — assume success
			}
			throw err;
		});
	}

	#enqueueOp(
		connHandle: number,
		kind: 'read' | 'write',
		timeoutMs: number,
		send: () => void,
	): Promise<ArrayBuffer | undefined> {
		const i = charInternal(this);
		const key = opKey(connHandle, i.info.uuid);
		return new Promise<ArrayBuffer | undefined>((resolve, reject) => {
			let queue = pump.pending.get(key);
			if (!queue) {
				queue = [];
				pump.pending.set(key, queue);
			}
			const op: PendingOp = {
				kind,
				resolve: (data) => {
					pumpUnref();
					resolve(data);
				},
				reject: (err) => {
					pumpUnref();
					reject(err);
				},
				timer: setTimeout(() => {
					const idx = queue.indexOf(op);
					if (idx !== -1) queue.splice(idx, 1);
					op.reject(
						new DOMException(
							`GATT ${kind} operation timed out`,
							'NetworkError',
						),
					);
				}, timeoutMs),
			};
			queue.push(op);
			pumpRef();
			try {
				send();
			} catch (err) {
				clearTimeout(op.timer);
				const idx = queue.indexOf(op);
				if (idx !== -1) queue.splice(idx, 1);
				op.reject(err as Error);
			}
		});
	}

	/**
	 * Subscribes to value change notifications/indications. Also writes the
	 * Client Characteristic Configuration descriptor, like browsers do.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/startNotifications
	 */
	async startNotifications(): Promise<BluetoothRemoteGATTCharacteristic> {
		const i = charInternal(this);
		if (i.notifying) return this;
		const { handle, primary, svc } = this.#conn();
		$.btleNotify(
			handle,
			primary,
			svc.uuid,
			svc.instanceId,
			i.info.uuid,
			i.info.instanceId,
			true,
		);
		this.#writeCCCD(handle, primary, svc, i, true);
		const key = opKey(handle, i.info.uuid);
		let set = pump.notify.get(key);
		if (!set) {
			set = new Set();
			pump.notify.set(key, set);
		}
		set.add(this);
		i.notifying = true;
		pumpRef();
		return this;
	}

	/**
	 * Unsubscribes from value change notifications.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BluetoothRemoteGATTCharacteristic/stopNotifications
	 */
	async stopNotifications(): Promise<BluetoothRemoteGATTCharacteristic> {
		const i = charInternal(this);
		if (!i.notifying) return this;
		const { handle, primary, svc } = this.#conn();
		$.btleNotify(
			handle,
			primary,
			svc.uuid,
			svc.instanceId,
			i.info.uuid,
			i.info.instanceId,
			false,
		);
		this.#writeCCCD(handle, primary, svc, i, false);
		const key = opKey(handle, i.info.uuid);
		const set = pump.notify.get(key);
		set?.delete(this);
		if (set && set.size === 0) pump.notify.delete(key);
		i.notifying = false;
		pumpUnref();
		return this;
	}

	// Write the CCCD (0x2902) to enable/disable notifications on the remote
	// (registering with the local stack alone does not flip the bit).
	#writeCCCD(
		handle: number,
		primary: boolean,
		svc: BtleService,
		i: GATTCharacteristicInternal,
		enable: boolean,
	): void {
		try {
			const descriptors = $.btleGetDescriptors(handle, i.info.handle);
			const cccd = descriptors.find((d) => d.uuid === CCCD_UUID);
			if (!cccd) return;
			const indicate = this.properties.indicate && !this.properties.notify;
			const value = new Uint8Array([
				enable ? (indicate ? 0x02 : 0x01) : 0x00,
				0x00,
			]);
			$.btleWriteDescriptor(
				handle,
				primary,
				svc.uuid,
				svc.instanceId,
				i.info.uuid,
				i.info.instanceId,
				cccd.uuid,
				0,
				value.buffer,
			);
		} catch {
			// Non-fatal: some stacks/devices handle CCCD internally.
		}
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'characteristicvaluechanged') {
			this.oncharacteristicvaluechanged?.(event);
		}
		return super.dispatchEvent(event);
	}
}
def(BluetoothRemoteGATTCharacteristic);

// ---------------------------------------------------------------------------
// Bluetooth (navigator.bluetooth)
// ---------------------------------------------------------------------------

// Every service UUID mentioned in the request (filter `services` +
// `optionalServices`) — registered as GATT data paths before connecting.
function collectServiceUuids(options?: RequestDeviceOptions): string[] {
	const uuids = new Set<string>();
	for (const filter of options?.filters ?? []) {
		for (const s of filter.services ?? []) uuids.add(toUUID(s));
	}
	for (const s of options?.optionalServices ?? []) uuids.add(toUUID(s));
	return [...uuids];
}

function matchesFilter(
	result: { name?: string },
	filter: BluetoothLEScanFilter,
): boolean {
	if (typeof filter.name === 'string') {
		if (result.name !== filter.name) return false;
	}
	if (typeof filter.namePrefix === 'string') {
		if (!result.name || !result.name.startsWith(filter.namePrefix))
			return false;
	}
	return true;
}

/**
 * Entry point to Web Bluetooth functionality — available as
 * `navigator.bluetooth`.
 *
 * Since nx.js has no permission-chooser UI, `requestDevice()` scans for
 * nearby BLE devices and resolves with the **first device matching the
 * filters** (scanning for up to 15 seconds before rejecting with
 * `NotFoundError`).
 *
 * ### Device discovery on Switch
 *
 * The Switch's application BLE scanner is **filter-based** (there is no
 * unfiltered scan like browsers use), which constrains how devices can be
 * discovered:
 *
 * - **Advertised service UUID** (recommended): every UUID in the filters'
 *   `services` and in `optionalServices` is scanned for; devices advertising
 *   one of them are found, then `name`/`namePrefix` filters are applied.
 * - **`manufacturerData` filters** map onto the system's manufacturer-data
 *   scan, but the system only matches payloads whose first byte after the
 *   company identifier is `0x01` — most third-party devices won't match.
 * - **Known address** (nx.js extension): `requestDevice({ deviceId:
 *   'aa:bb:cc:dd:ee:ff' })` skips scanning entirely and connects directly
 *   (an unfiltered low-level scan primes the stack's device cache first, so
 *   this works even for devices that advertise no service UUIDs).
 *
 * @example
 *
 * ```typescript
 * // Connect to a Niimbot label printer by address and use its serial
 * // service — the same GATT flow as Chrome's Web Bluetooth:
 * const device = await navigator.bluetooth.requestDevice({
 *   deviceId: '12:0c:01:e6:59:cf',
 *   optionalServices: ['e7810a71-73ae-499d-8c15-faa9aef0c3f2'],
 * });
 * const server = await device.gatt.connect();
 * const service = await server.getPrimaryService(
 *   'e7810a71-73ae-499d-8c15-faa9aef0c3f2',
 * );
 * const chars = await service.getCharacteristics();
 * ```
 *
 * @see https://developer.mozilla.org/docs/Web/API/Bluetooth
 */
export class Bluetooth extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
	}

	/**
	 * Resolves with `true` when Bluetooth LE is available on this system.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Bluetooth/getAvailability
	 */
	async getAvailability(): Promise<boolean> {
		try {
			ensureInit();
			return true;
		} catch {
			return false;
		}
	}

	/**
	 * Scans for a nearby Bluetooth LE device matching the provided filters.
	 *
	 * > [!NOTE]
	 * > The Switch's application BLE scanner filters by **advertised service
	 * > UUID**, so the scan UUIDs are gathered from the `services` of each
	 * > filter plus `optionalServices`. Provide at least one service UUID
	 * > (in either place) when targeting third-party devices —
	 * > `name`/`namePrefix` matching is then applied to the scan results.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Bluetooth/requestDevice
	 */
	async requestDevice(
		options?: RequestDeviceOptions,
	): Promise<BluetoothDevice> {
		// nx.js extension: direct addressing without a scan.
		if (typeof options?.deviceId === 'string') {
			const address = options.deviceId.toLowerCase();
			if (!/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/.test(address)) {
				throw new TypeError(
					`Invalid Bluetooth address: "${options.deviceId}"`,
				);
			}
			ensureInit();
			const allowed = collectServiceUuids(options);
			// @ts-expect-error internal constructor
			return new BluetoothDevice(INTERNAL, address, undefined, true, allowed);
		}
		const filters = options?.filters;
		const acceptAll = options?.acceptAllDevices === true;
		if (acceptAll === (filters !== undefined && filters.length > 0)) {
			throw new TypeError(
				"Failed to execute 'requestDevice' on 'Bluetooth': Either 'filters' should be present or 'acceptAllDevices' should be true, but not both.",
			);
		}
		// The Switch's application BLE scanner is filter-based (no unfiltered
		// scan), so build the scan plan from everything the options give us:
		//   - `manufacturerData` filters -> "general" scans with a custom
		//     {companyIdentifier, dataPrefix} manufacturer-data filter
		//   - `services` filters + `optionalServices` -> "smart device"
		//     scans by advertised 128-bit service UUID
		type ScanMode =
			| { uuid: string }
			| { companyId: number; pattern: ArrayBuffer }
			| { general: true };
		const modes: ScanMode[] = [];
		if (filters) {
			for (const filter of filters) {
				if (
					typeof filter.name === 'undefined' &&
					typeof filter.namePrefix === 'undefined' &&
					(!filter.services || filter.services.length === 0) &&
					(!filter.manufacturerData ||
						filter.manufacturerData.length === 0)
				) {
					throw new TypeError(
						"Failed to execute 'requestDevice' on 'Bluetooth': A filter must restrict the devices in some way.",
					);
				}
				for (const s of filter.services ?? []) {
					modes.push({ uuid: toUUID(s) });
				}
				for (const m of filter.manufacturerData ?? []) {
					modes.push({
						companyId: m.companyIdentifier,
						pattern: m.dataPrefix
							? bufferSourceToArrayBuffer(m.dataPrefix)
							: new ArrayBuffer(0),
					});
				}
			}
		}
		for (const s of options?.optionalServices ?? []) {
			modes.push({ uuid: toUUID(s) });
		}
		if (modes.length === 0) {
			modes.push({ general: true });
		}
		ensureInit();

		const deadline = Date.now() + 15000;
		const sliceMs = Math.max(5000, 15000 / modes.length);
		let match: { address: string; name?: string } | undefined;
		while (!match && Date.now() < deadline) {
			for (const mode of modes) {
				if ('uuid' in mode) {
					$.btleScanStart(mode.uuid);
				} else if ('companyId' in mode) {
					$.btleScanStart(null, mode.companyId, mode.pattern);
				} else {
					$.btleScanStart();
				}
				pumpRef();
				try {
					match = await waitFor(
						() => {
							const results = $.btleScanResults();
							for (const result of results) {
								if (
									acceptAll ||
									filters!.some((f) =>
										matchesFilter(result, f),
									)
								) {
									return result;
								}
							}
							return undefined;
						},
						Math.min(sliceMs, deadline - Date.now()),
						'scan slice elapsed',
					).catch(() => undefined);
				} finally {
					try {
						$.btleScanStop();
					} catch {}
					pumpUnref();
				}
				if (match) break;
			}
		}
		if (!match) {
			throw new DOMException(
				'No Bluetooth devices matching the filters were found.',
				'NotFoundError',
			);
		}
		const Ctor = BluetoothDevice as any;
		return new Ctor(
			INTERNAL,
			match.address,
			match.name,
			false,
			collectServiceUuids(options),
		) as BluetoothDevice;
	}
}
def(Bluetooth);

/**
 * The shared `Bluetooth` instance (also exposed as `navigator.bluetooth`).
 *
 * @internal
 */
// @ts-expect-error internal constructor
export const bluetooth = new Bluetooth(INTERNAL);
