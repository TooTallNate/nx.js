var logged = [];

var wasm = loadWasm('imports.wasm');
var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module, {
  env: {
    log: function (val) {
      logged.push(val);
    },
  },
});

instance.exports.callLog(42);
instance.exports.callLog(100);
instance.exports.callLog(-1);

__output({
  'logged values': logged,
  'logged length': logged.length,
});
