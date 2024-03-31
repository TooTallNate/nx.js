// `simple.wasm` exports a function named `exported_func()`,
// which invokes the imported function `imported_func()` with
// a single parameter containing the value 42.
// https://github.com/mdn/webassembly-examples/blob/main/js-api-examples/simple.wat

const wasm = Switch.readFileSync(
	new URL('wasm/simple.wasm', Switch.entrypoint),
);

const mod = new WebAssembly.Module(wasm);
console.log({
	imports: WebAssembly.Module.imports(mod),
	exports: WebAssembly.Module.exports(mod),
});

const importObject = {
	imports: {
		imported_func(arg: number) {
			console.log({ arg });
		},
	},
};

WebAssembly.instantiate(mod, importObject)
	.then((instance) => {
		const exported_func = instance.exports.exported_func as () => void;
		exported_func();
	})
	.catch((err) => {
		console.error(err);
	});
