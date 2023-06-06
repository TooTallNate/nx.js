console.log('Hello World!');
console.log(' - <3 nx.js');
console.log('');
console.log('Press the + button to exit...');

Switch.resolveDns('n8.io').then((r) => {
	console.log(r);
});
Switch.resolveDns('google.com').then((r) => {
	console.log(r);
});
Switch.resolveDns('github.com').then((r) => {
	console.log(r);
});
Switch.resolveDns('example.com').then((r) => {
	console.log(r);
});

Switch.resolveDns('dsfadfasdfasdfn8.io').then((r) => {
	console.log(r);
}, err => {
    console.log(err)
    console.log(err.stack)
});


Switch.readFile(Switch.argv[0]).then(
	(r) => {
		console.log('cb invoked');
		console.log(r instanceof ArrayBuffer);
		console.log(r.byteLength);
	},
	(err) => {
		console.log('err');
		console.log(err);
	}
);
