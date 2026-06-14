import { $, type USBNativeDevice } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import type { BufferSource } from '../types';
import { assertInternalConstructor, def } from '../utils';
import { setTimeout } from '../timers';

type USBControlTransferType = 'standard' | 'class' | 'vendor';
type USBRequestType = USBControlTransferType | 'reserved';
type USBRecipient = 'device' | 'interface' | 'endpoint' | 'other';
type USBTransferStatus = 'ok' | 'stall' | 'babble';
type USBDirection = 'in' | 'out';
type USBEndpointType = 'bulk' | 'interrupt' | 'isochronous';

export interface USBDeviceFilter {
	vendorId?: number;
	productId?: number;
	classCode?: number;
	subclassCode?: number;
	protocolCode?: number;
	interfaceClass?: number;
	interfaceSubclass?: number;
	interfaceProtocol?: number;
}

export interface USBDeviceRequestOptions {
	filters?: USBDeviceFilter[];
}

export interface USBControlTransferParameters {
	requestType: USBRequestType;
	recipient: USBRecipient;
	request: number;
	value: number;
	index: number;
}

export interface USBInTransferResult {
	data?: DataView;
	status: USBTransferStatus;
}

export interface USBOutTransferResult {
	bytesWritten: number;
	status: USBTransferStatus;
}

interface USBEndpointDescriptor {
	endpointNumber: number;
	direction: USBDirection;
	type: USBEndpointType;
	packetSize: number;
}

interface USBAlternateInterfaceDescriptor {
	alternateSetting: number;
	interfaceClass: number;
	interfaceSubclass: number;
	interfaceProtocol: number;
	endpoints: USBEndpointDescriptor[];
}

interface USBInterfaceDescriptor {
	interfaceNumber: number;
	claimed?: boolean | number;
	alternates: USBAlternateInterfaceDescriptor[];
}

interface USBConfigurationDescriptor {
	configurationValue: number;
	interfaces: USBInterfaceDescriptor[];
}

interface USBNativeDescriptor extends USBNativeDevice {
	usbVersionMajor: number;
	usbVersionMinor: number;
	usbVersionSubminor: number;
	deviceClass: number;
	deviceSubclass: number;
	deviceProtocol: number;
	vendorId: number;
	productId: number;
	deviceVersionMajor: number;
	deviceVersionMinor: number;
	deviceVersionSubminor: number;
	manufacturerName?: string;
	productName?: string;
	serialNumber?: string;
	configurations: USBConfigurationDescriptor[];
}

interface USBDeviceState {
	native: USBNativeDescriptor;
	opened: boolean;
	configuration?: USBConfiguration;
}

const INTERNAL = INTERNAL_SYMBOL;
const _device = new WeakMap<USBDevice, USBDeviceState>();
let initialized = false;

function ensureInit() {
	if (initialized) return;
	$.usbInit();
	initialized = true;
	addEventListener('unload', () => {
		$.usbExit();
		initialized = false;
	});
}

function yieldToLoop() {
	return new Promise<void>((resolve) => setTimeout(resolve, 0));
}

function validateByte(name: string, value: number) {
	if (!Number.isInteger(value) || value < 0 || value > 0xff) {
		throw new TypeError(`${name} must be an integer in the range 0-255`);
	}
}

function validateWord(name: string, value: number) {
	if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
		throw new TypeError(`${name} must be an integer in the range 0-65535`);
	}
}

function validateFilter(filter: USBDeviceFilter) {
	for (const key of [
		'vendorId',
		'productId',
		'classCode',
		'subclassCode',
		'protocolCode',
		'interfaceClass',
		'interfaceSubclass',
		'interfaceProtocol',
	] as const) {
		const value = filter[key];
		if (typeof value !== 'undefined') {
			validateWord(key, value);
		}
	}
}

function deviceFromNative(native: USBNativeDescriptor): USBDevice {
	return new USBDevice(INTERNAL, native);
}

/** A USB endpoint descriptor exposed by {@link USBAlternateInterface}. */
export class USBEndpoint {
	readonly endpointNumber: number;
	readonly direction: USBDirection;
	readonly type: USBEndpointType;
	readonly packetSize: number;

	constructor(_internal: symbol, desc: USBEndpointDescriptor) {
		assertInternalConstructor(arguments);
		this.endpointNumber = desc.endpointNumber;
		this.direction = desc.direction;
		this.type = desc.type;
		this.packetSize = desc.packetSize;
	}
}
def(USBEndpoint);

/** A USB alternate interface descriptor. */
export class USBAlternateInterface {
	readonly alternateSetting: number;
	readonly interfaceClass: number;
	readonly interfaceSubclass: number;
	readonly interfaceProtocol: number;
	readonly endpoints: USBEndpoint[];

	constructor(_internal: symbol, desc: USBAlternateInterfaceDescriptor) {
		assertInternalConstructor(arguments);
		this.alternateSetting = desc.alternateSetting;
		this.interfaceClass = desc.interfaceClass;
		this.interfaceSubclass = desc.interfaceSubclass;
		this.interfaceProtocol = desc.interfaceProtocol;
		this.endpoints = desc.endpoints.map((e) => new USBEndpoint(INTERNAL, e));
	}
}
def(USBAlternateInterface);

/** A USB interface descriptor. */
export class USBInterface {
	readonly interfaceNumber: number;
	claimed: boolean;
	readonly alternates: USBAlternateInterface[];
	alternate: USBAlternateInterface;

	constructor(_internal: symbol, desc: USBInterfaceDescriptor) {
		assertInternalConstructor(arguments);
		this.interfaceNumber = desc.interfaceNumber;
		this.claimed = !!desc.claimed;
		this.alternates = desc.alternates.map(
			(a) => new USBAlternateInterface(INTERNAL, a),
		);
		this.alternate = this.alternates[0];
	}
}
def(USBInterface);

/** A USB configuration descriptor. */
export class USBConfiguration {
	readonly configurationValue: number;
	readonly interfaces: USBInterface[];

	constructor(_internal: symbol, desc: USBConfigurationDescriptor) {
		assertInternalConstructor(arguments);
		this.configurationValue = desc.configurationValue;
		this.interfaces = desc.interfaces.map((i) => new USBInterface(INTERNAL, i));
	}
}
def(USBConfiguration);

/** A USB device returned from {@link USB.requestDevice}. */
export class USBDevice {
	readonly usbVersionMajor: number;
	readonly usbVersionMinor: number;
	readonly usbVersionSubminor: number;
	readonly deviceClass: number;
	readonly deviceSubclass: number;
	readonly deviceProtocol: number;
	readonly vendorId: number;
	readonly productId: number;
	readonly deviceVersionMajor: number;
	readonly deviceVersionMinor: number;
	readonly deviceVersionSubminor: number;
	readonly manufacturerName?: string;
	readonly productName?: string;
	readonly serialNumber?: string;
	readonly configurations: USBConfiguration[];

	constructor(_internal: symbol, native: USBNativeDescriptor) {
		assertInternalConstructor(arguments);
		this.usbVersionMajor = native.usbVersionMajor;
		this.usbVersionMinor = native.usbVersionMinor;
		this.usbVersionSubminor = native.usbVersionSubminor;
		this.deviceClass = native.deviceClass;
		this.deviceSubclass = native.deviceSubclass;
		this.deviceProtocol = native.deviceProtocol;
		this.vendorId = native.vendorId;
		this.productId = native.productId;
		this.deviceVersionMajor = native.deviceVersionMajor;
		this.deviceVersionMinor = native.deviceVersionMinor;
		this.deviceVersionSubminor = native.deviceVersionSubminor;
		this.manufacturerName = native.manufacturerName;
		this.productName = native.productName;
		this.serialNumber = native.serialNumber;
		this.configurations = native.configurations.map(
			(c) => new USBConfiguration(INTERNAL, c),
		);
		_device.set(this, {
			native,
			opened: false,
			configuration: this.configurations[0],
		});
	}

	get opened() {
		return _device.get(this)?.opened ?? false;
	}

	get configuration() {
		return _device.get(this)?.configuration;
	}

	async open(): Promise<void> {
		const state = _device.get(this)!;
		if (!state.opened) {
			await yieldToLoop();
			$.usbDeviceOpen(state.native);
			state.opened = true;
		}
	}

	async close(): Promise<void> {
		const state = _device.get(this)!;
		if (state.opened) {
			await yieldToLoop();
			$.usbDeviceClose(state.native);
			state.opened = false;
			for (const config of this.configurations) {
				for (const iface of config.interfaces) iface.claimed = false;
			}
		}
	}

	async selectConfiguration(configurationValue: number): Promise<void> {
		const state = _device.get(this)!;
		const config = this.configurations.find(
			(c) => c.configurationValue === configurationValue,
		);
		if (!config) {
			throw new DOMException('The selected configuration does not exist.', 'NotFoundError');
		}
		state.configuration = config;
	}

	async claimInterface(interfaceNumber: number): Promise<void> {
		const state = _device.get(this)!;
		if (!state.opened) {
			throw new DOMException('The device must be opened first.', 'InvalidStateError');
		}
		await yieldToLoop();
		$.usbClaimInterface(state.native, interfaceNumber);
		const iface = state.configuration?.interfaces.find(
			(i) => i.interfaceNumber === interfaceNumber,
		);
		if (iface) iface.claimed = true;
	}

	async releaseInterface(interfaceNumber: number): Promise<void> {
		const iface = this.configuration?.interfaces.find(
			(i) => i.interfaceNumber === interfaceNumber,
		);
		if (iface) iface.claimed = false;
	}

	async selectAlternateInterface(
		interfaceNumber: number,
		alternateSetting: number,
	): Promise<void> {
		const iface = this.configuration?.interfaces.find(
			(i) => i.interfaceNumber === interfaceNumber,
		);
		const alternate = iface?.alternates.find(
			(a) => a.alternateSetting === alternateSetting,
		);
		if (!iface || !alternate) {
			throw new DOMException('The selected alternate interface does not exist.', 'NotFoundError');
		}
		iface.alternate = alternate;
	}

	async transferIn(
		endpointNumber: number,
		length: number,
	): Promise<USBInTransferResult> {
		validateByte('endpointNumber', endpointNumber);
		validateWord('length', length);
		const state = _device.get(this)!;
		await yieldToLoop();
		const buffer = $.usbTransferIn(state.native, endpointNumber, length);
		return { data: new DataView(buffer), status: 'ok' };
	}

	async transferOut(
		endpointNumber: number,
		data: BufferSource,
	): Promise<USBOutTransferResult> {
		validateByte('endpointNumber', endpointNumber);
		const state = _device.get(this)!;
		await yieldToLoop();
		return {
			bytesWritten: $.usbTransferOut(state.native, endpointNumber, data),
			status: 'ok',
		};
	}

	async controlTransferIn(
		setup: USBControlTransferParameters,
		length: number,
	): Promise<USBInTransferResult> {
		if (!['standard', 'class', 'vendor', 'reserved'].includes(setup.requestType)) {
			throw new TypeError('Invalid USB requestType');
		}
		if (!['device', 'interface', 'endpoint', 'other'].includes(setup.recipient)) {
			throw new TypeError('Invalid USB recipient');
		}
		validateByte('request', setup.request);
		validateWord('value', setup.value);
		validateWord('index', setup.index);
		validateWord('length', length);
		const state = _device.get(this)!;
		await yieldToLoop();
		const buffer = $.usbControlTransferIn(state.native, setup, length);
		return { data: new DataView(buffer), status: 'ok' };
	}

	async reset(): Promise<void> {
		const state = _device.get(this)!;
		await yieldToLoop();
		$.usbResetDevice(state.native);
	}
}
def(USBDevice);

/** Entry point for WebUSB functionality, available as `navigator.usb`. */
export class USB {
	constructor() {
		assertInternalConstructor(arguments);
	}

	async getDevices(): Promise<USBDevice[]> {
		ensureInit();
		return $.usbGetDevices().map((native) =>
			deviceFromNative(native as USBNativeDescriptor),
		);
	}

	async requestDevice(options: USBDeviceRequestOptions = {}): Promise<USBDevice> {
		ensureInit();
		const filters = options.filters ?? [{}];
		if (!Array.isArray(filters)) {
			throw new TypeError('USB device filters must be an array');
		}
		for (const filter of filters) {
			validateFilter(filter);
			const devices = $.usbGetDevices(filter).map((native) =>
				deviceFromNative(native as USBNativeDescriptor),
			);
			if (devices.length > 0) return devices[0];
		}
		throw new DOMException('No USB devices matching the filters were found.', 'NotFoundError');
	}
}
def(USB);

/** The shared USB instance, also exposed as `navigator.usb`. */
// @ts-expect-error internal constructor
export const usb = new USB(INTERNAL);
