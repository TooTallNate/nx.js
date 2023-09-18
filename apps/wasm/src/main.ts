const importObject = {
	imports: {
		imported_func(arg: number) {
			console.log(arg);
		},
	},
};

const wasm = Switch.readFileSync(new URL('simple.wasm', Switch.entrypoint));
WebAssembly.instantiate(wasm, importObject).then((results) => {
	results.instance.exports.exported_func();
});
