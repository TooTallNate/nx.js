// WebAssembly.validate() conformance tests
var validWasm = loadWasm('add.wasm');

// Test with valid module bytes
var validResult = WebAssembly.validate(validWasm);

// Test with invalid bytes (not a wasm module)
var invalidResult = WebAssembly.validate(new ArrayBuffer(4));

// Test with empty buffer
var emptyResult = WebAssembly.validate(new ArrayBuffer(0));

// Test with garbage bytes
var garbage = new Uint8Array([0x01, 0x02, 0x03, 0x04, 0x05]);
var garbageResult = WebAssembly.validate(garbage.buffer);

// Test with just the wasm magic number but no content after
var magicOnly = new Uint8Array([0x00, 0x61, 0x73, 0x6d]);
var magicOnlyResult = WebAssembly.validate(magicOnly.buffer);

// Test with wasm magic + version but empty module
var magicAndVersion = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
var magicAndVersionResult = WebAssembly.validate(magicAndVersion.buffer);

// Test with Uint8Array view of valid wasm (BufferSource input)
var validView = new Uint8Array(validWasm);
var viewResult = WebAssembly.validate(validView);

// Test return type
var returnType = typeof validResult;

__output({
  'valid module': validResult,
  'invalid 4 zero bytes': invalidResult,
  'empty buffer': emptyResult,
  'garbage bytes': garbageResult,
  'magic number only': magicOnlyResult,
  'magic + version (minimal valid)': magicAndVersionResult,
  'Uint8Array view of valid': viewResult,
  'return type': returnType,
});
