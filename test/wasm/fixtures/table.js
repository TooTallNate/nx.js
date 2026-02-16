var wasm = loadWasm('table.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

// callIndirect(a, b, tableIndex): calls table[tableIndex](a, b)
// index 0 = add, index 1 = sub
__output({
  'callIndirect(3,4,0) [add]': instance.exports.callIndirect(3, 4, 0),
  'callIndirect(3,4,1) [sub]': instance.exports.callIndirect(3, 4, 1),
  'callIndirect(10,3,0) [add]': instance.exports.callIndirect(10, 3, 0),
  'callIndirect(10,3,1) [sub]': instance.exports.callIndirect(10, 3, 1),
});
