var wasm = loadWasm('add.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

__output({
  'add(1,2)': instance.exports.add(1, 2),
  'add(0,0)': instance.exports.add(0, 0),
  'add(-1,1)': instance.exports.add(-1, 1),
  'add(100,200)': instance.exports.add(100, 200),
  'add(-50,-50)': instance.exports.add(-50, -50),
  'add(2147483647,1)': instance.exports.add(2147483647, 1),
});
