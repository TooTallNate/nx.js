// Memory.buffer caching conformance tests
// Verifies .buffer returns the same ArrayBuffer instance until grow()

var mem = new WebAssembly.Memory({ initial: 1 });

// Buffer identity should be stable across repeated accesses
var buf1 = mem.buffer;
var buf2 = mem.buffer;
var stableBeforeGrow = buf1 === buf2;

// Record initial size
var initialByteLength = buf1.byteLength;

// Grow the memory by 1 page
var previousPages = mem.grow(1);

// After grow, buffer should be a different instance
var buf3 = mem.buffer;
var differentAfterGrow = buf1 !== buf3;

// New buffer should have the grown size
var grownByteLength = buf3.byteLength;

// New buffer identity should be stable after grow
var buf4 = mem.buffer;
var stableAfterGrow = buf3 === buf4;

// Grow again
mem.grow(2);
var buf5 = mem.buffer;
var differentAfterSecondGrow = buf4 !== buf5;

// Stable again after second grow
var buf6 = mem.buffer;
var stableAfterSecondGrow = buf5 === buf6;

var finalByteLength = buf5.byteLength;

__output({
  'buffer identity stable before grow': stableBeforeGrow,
  'initial byteLength': initialByteLength,
  'grow(1) returns previous page count': previousPages,
  'buffer different after grow': differentAfterGrow,
  'grown byteLength': grownByteLength,
  'buffer identity stable after grow': stableAfterGrow,
  'buffer different after second grow': differentAfterSecondGrow,
  'buffer identity stable after second grow': stableAfterSecondGrow,
  'final byteLength': finalByteLength,
});
