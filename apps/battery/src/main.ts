navigator.getBattery().then((bm) => {
	console.log({ bm, instance: bm instanceof BatteryManager });
	setInterval(() => {
		console.log({ level: bm.level, charging: bm.charging });
	}, 1000);
});
