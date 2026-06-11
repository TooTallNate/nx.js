// Web Bluetooth Demo - nx.js
//
// A small GATT explorer: finds a nearby Bluetooth LE device, connects, and
// dumps its GATT table (services, characteristics, properties), reading the
// value of every readable characteristic.
//
// ### Device discovery on Switch
//
// The Switch's application BLE scanner is filter-based — devices are
// discovered by an **advertised 128-bit service UUID** (from the `services`
// filters / `optionalServices`). Many hobbyist/maker devices advertise one
// (the defaults below cover common BLE-serial modules); consumer devices
// that only advertise manufacturer data (e.g. AirPods) cannot be discovered
// by scanning — connect to those by address instead with the nx.js
// `deviceId` extension:
//
//   navigator.bluetooth.requestDevice({ deviceId: 'aa:bb:cc:dd:ee:ff' })
//
// Tip: find a device's address and advertised services with a phone app
// like "nRF Connect", then customize the constants below.

// Set to your device's Bluetooth address to skip scanning entirely:
const DEVICE_ADDRESS: string | null = null; // e.g. '12:0c:01:e6:59:cf'

// Otherwise, scan for devices advertising any of these service UUIDs:
const SCAN_SERVICES = [
	'6e400001-b5a3-f393-e0a9-e50e24dcca9e', // Nordic UART (Arduino/ESP32/Adafruit)
	'49535343-fe7d-4ae5-8fa9-9fafd205e455', // Microchip Transparent UART
	'e7810a71-73ae-499d-8c15-faa9aef0c3f2', // Transparent serial (Niimbot, etc.)
];

console.log('Web Bluetooth — GATT explorer');
console.log('');

const hex = (view: DataView) =>
	[...new Uint8Array(view.buffer, view.byteOffset, view.byteLength)]
		.map((b) => b.toString(16).padStart(2, '0'))
		.join(' ');

async function main() {
	if (!(await navigator.bluetooth.getAvailability())) {
		console.log('Bluetooth LE is not available on this system.');
		return;
	}

	let device: BluetoothDevice;
	if (DEVICE_ADDRESS) {
		console.log(`Connecting by address: ${DEVICE_ADDRESS}`);
		device = await navigator.bluetooth.requestDevice({
			deviceId: DEVICE_ADDRESS,
			optionalServices: SCAN_SERVICES,
		});
	} else {
		console.log('Scanning for devices advertising:');
		for (const s of SCAN_SERVICES) console.log(`  ${s}`);
		console.log('');
		device = await navigator.bluetooth.requestDevice({
			filters: SCAN_SERVICES.map((s) => ({ services: [s] })),
		});
	}
	console.log(`Found: ${device.name ?? '(unnamed)'} [${device.id}]`);

	device.addEventListener('gattserverdisconnected', () => {
		console.log('Device disconnected.');
	});

	console.log('Connecting...');
	const server = await device.gatt.connect();

	for (const service of await server.getPrimaryServices()) {
		console.log(`service ${service.uuid}`);
		for (const c of await service.getCharacteristics()) {
			const p = c.properties;
			const props = [
				p.read && 'read',
				p.write && 'write',
				p.writeWithoutResponse && 'writeWithoutResponse',
				p.notify && 'notify',
				p.indicate && 'indicate',
			]
				.filter(Boolean)
				.join(',');
			console.log(`  characteristic ${c.uuid} [${props}]`);
			if (p.read) {
				try {
					const value = await c.readValue();
					console.log(`    value: ${hex(value)}`);
				} catch (err: any) {
					console.log(`    read failed: ${err.message}`);
				}
			}
		}
	}

	server.disconnect();
}

main()
	.catch((err) => {
		console.log(`${err.name}: ${err.message}`);
	})
	.finally(() => {
		console.log('');
		console.log('Press + to exit...');
	});
