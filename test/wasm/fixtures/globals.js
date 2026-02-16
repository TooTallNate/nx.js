var wasm = loadWasm('globals.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

var results = {
  'increment() 1st': instance.exports.increment(),
  'increment() 2nd': instance.exports.increment(),
  'increment() 3rd': instance.exports.increment(),
};

__output(results);
