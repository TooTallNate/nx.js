// Web Bluetooth Demo - nx.js
//
// Scans for a nearby Bluetooth LE device advertising the standard Battery
// Service (0x180f), connects, and reads the battery level — exercising the
// full Web Bluetooth flow: requestDevice() → gatt.connect() →
// getPrimaryService() → getCharacteristic() → readValue().
//
// Note: the Switch's application BLE scanner is filter-based — devices are
// discovered by an advertised service UUID (from the `services` filters /
// `optionalServices`). To talk to a device that does not advertise its
// service UUIDs, connect by address with the nx.js `deviceId` extension:
//
//   navigator.bluetooth.requestDevice({ deviceId: 'aa:bb:cc:dd:ee:ff' })

const BATTERY_SERVICE = 0x180f;
const BATTERY_LEVEL = 0x2a19;

console.log('Web Bluetooth — Battery Service demo');
console.log('');

async function main() {
	if (!(await navigator.bluetooth.getAvailability())) {
		console.log('Bluetooth LE is not available on this system.');
		return;
	}

	console.log('Scanning for a device advertising the Battery Service...');
	console.log('(make sure a BLE device with battery reporting is nearby');
	console.log(' and advertising, e.g. headphones in pairing mode)');
	console.log('');

	const device = await navigator.bluetooth.requestDevice({
		filters: [{ services: [BATTERY_SERVICE] }],
	});
	console.log(`Found: ${device.name ?? '(unnamed)'} [${device.id}]`);

	device.addEventListener('gattserverdisconnected', () => {
		console.log('Device disconnected.');
	});

	console.log('Connecting...');
	const server = await device.gatt.connect();

	const service = await server.getPrimaryService(BATTERY_SERVICE);
	const characteristic = await service.getCharacteristic(BATTERY_LEVEL);
	const value = await characteristic.readValue();
	console.log('');
	console.log(`🔋 Battery level: ${value.getUint8(0)}%`);

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
