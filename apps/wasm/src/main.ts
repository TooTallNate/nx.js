// `simple.wasm` exports a function named `exported_func()`,
// which invokes the imported function `imported_func()` with
// a single parameter containing the value 42.
// https://github.com/mdn/webassembly-examples/blob/main/js-api-examples/simple.wat
const wasm = Switch.readFileSync(new URL('simple.wasm', Switch.entrypoint));
//const wasm = require('fs').readFileSync(__dirname + '/../romfs/simple.wasm');

const mod = new WebAssembly.Module(wasm);
console.log(WebAssembly.Module.exports(mod));
console.log(WebAssembly.Module.imports(mod));

const importObject = {
	imports: {
		imported_func(arg /* @type any */) {
			console.log({ arg });
		},
	},
};

WebAssembly.instantiate(mod, importObject)
	.then((instance) => {
		console.log(Object.keys(instance.exports));
		instance.exports.exported_func();
	})
	.catch((err) => {
		console.error(err);
	});
