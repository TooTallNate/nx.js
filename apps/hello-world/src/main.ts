console.log('Hello World!');
console.log(' - <3 nx.js');
console.log('');
console.log('Press the + button to exit...');

Switch.readFile(Switch.argv[0]).then((r) => {
    console.log('cb invoked')
    console.log(r instanceof ArrayBuffer);
    console.log(r.byteLength);
}, (err) => {
    console.log('err');
    console.log(err);
});
