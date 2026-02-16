var wasm = loadWasm('memory.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

// Store and load values
instance.exports.store(0, 42);
instance.exports.store(4, 100);
instance.exports.store(8, -1);

var results = {
  'load(0) after store(0,42)': instance.exports.load(0),
  'load(4) after store(4,100)': instance.exports.load(4),
  'load(8) after store(8,-1)': instance.exports.load(8),
};

// Check memory buffer via Memory export
var mem = instance.exports.memory;
var view = new Uint8Array(mem.buffer);
results['memory.buffer byteLength'] = mem.buffer.byteLength;
results['byte at offset 0'] = view[0];
results['byte at offset 1'] = view[1];

__output(results);
