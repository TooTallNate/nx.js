// `simple.wasm` exports a function named `exported_func()`,
// which invokes the imported function `imported_func()` with
// a single parameter containing the value 42.
// https://github.com/mdn/webassembly-examples/blob/main/js-api-examples/simple.wat
import wasmPath from './simple.wasm';

const wasm = Switch.readFileSync(new URL(wasmPath, Switch.entrypoint));

const mod = new WebAssembly.Module(wasm!);
console.log({
	imports: WebAssembly.Module.imports(mod),
	exports: WebAssembly.Module.exports(mod),
});

const instance = await WebAssembly.instantiate(mod, {
	imports: {
		imported_func(arg: number) {
			console.log({ arg });
		},
	},
});

const exported_func = instance.exports.exported_func as () => void;
exported_func();
