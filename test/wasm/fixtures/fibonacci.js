var wasm = loadWasm('fibonacci.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

__output({
  'fib(0)': instance.exports.fib(0),
  'fib(1)': instance.exports.fib(1),
  'fib(2)': instance.exports.fib(2),
  'fib(5)': instance.exports.fib(5),
  'fib(10)': instance.exports.fib(10),
  'fib(20)': instance.exports.fib(20),
});
