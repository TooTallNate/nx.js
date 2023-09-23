// `simple.wasm` exports a function named `exported_func()`,
// which invokes the imported function `imported_func()` with
// a single parameter containing the value 42.
// https://github.com/mdn/webassembly-examples/blob/main/js-api-examples/simple.wat
const wasm = Switch.readFileSync(new URL('add.wasm', Switch.entrypoint));
//const wasm = require('fs').readFileSync(__dirname + '/../romfs/add.wasm');

const mod = new WebAssembly.Module(wasm);
console.log(WebAssembly.Module.exports(mod));
console.log(WebAssembly.Module.imports(mod));

WebAssembly.instantiate(mod).then((instance) => {
	console.log(Object.keys(instance.exports));
	if (typeof instance.exports.add === 'function') {
		console.log(instance.exports.add(6, 9));
	}
});

//const importObject = {
//	imports: {
//		imported_func(arg /* @type any */) {
//			console.log({ arg });
//		},
//	},
//};
//
//WebAssembly.instantiate(wasm, importObject)
//	.then((results) => {
//		console.log(WebAssembly.Module.exports(results.module));
//		console.log(WebAssembly.Module.imports(results.module));
//		//console.log(results.instance.exports.add);
//		//console.log(results.instance.exports.add(1, 5));
//		if (typeof results.instance.exports.exported_func === 'function') {
//			console.log(results.instance.exports.exported_func());
//		} else {
//			throw new Error(`"exported_func" was not exported from WASM file`);
//		}
//	});
