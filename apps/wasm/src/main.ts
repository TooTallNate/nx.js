const importObject = {
	imports: {
		imported_func(arg: number) {
			console.log(arg);
		},
	},
};

const wasm = Switch.readFileSync('simple.wasm');
WebAssembly.instantiate(wasm, importObject).then((results) => {
	results.instance.exports.exported_func();
});
