/**
 * WebAssembly API Bridge for nx.js test harness
 *
 * Maps the standard WebAssembly API to the native nx.js C bindings
 * exposed via the '$' global (init_obj).
 *
 * Native bindings:
 *   $.wasmNewModule(arrayBuffer)         → module opaque
 *   $.wasmNewInstance(module, imports[])  → [instance, exports[]]
 *   $.wasmCallFunc(func, ...args)        → result
 *   $.wasmMemNew(descriptor)             → memory opaque
 *   $.wasmNewGlobal(descriptor)          → global opaque
 *   $.wasmModuleImports(module)          → import descriptors
 *   $.wasmModuleExports(module)          → export descriptors
 *   $.wasmGlobalGet(global)              → value
 *   $.wasmGlobalSet(global, value)       → undefined
 *   $.wasmTableGet(table, index)         → function opaque
 *   $.wasmInitMemory(MemoryClass)        → undefined
 *   $.wasmInitTable(TableClass)          → undefined
 */

// ---- loadWasm(filename) ----
// Reads a .wasm file from the modules directory and returns an ArrayBuffer.
var loadWasm = function (filename) {
	var path = __modules_dir + '/' + filename;
	return readFile(path);
};

// ---- WebAssembly namespace ----
var WebAssembly = {};

// ---- WebAssembly.Module ----
WebAssembly.Module = function Module(bufferSource) {
	// Store the native module opaque
	this._module = $.wasmNewModule(bufferSource);
};

WebAssembly.Module.imports = function (module) {
	return $.wasmModuleImports(module._module);
};

WebAssembly.Module.exports = function (module) {
	return $.wasmModuleExports(module._module);
};

// ---- WebAssembly.Memory ----
WebAssembly.Memory = function Memory(descriptor) {
	// The native side sets up buffer getter and grow() on the prototype
};
$.wasmInitMemory(WebAssembly.Memory);

// Constructor needs to call native
var _OrigMemory = WebAssembly.Memory;
WebAssembly.Memory = function Memory(descriptor) {
	var obj = $.wasmMemNew(descriptor);
	// obj is already an instance of the class with buffer/grow
	return obj;
};
WebAssembly.Memory.prototype = _OrigMemory.prototype;

// ---- WebAssembly.Table ----
WebAssembly.Table = function Table(descriptor) {
	// placeholder
};
$.wasmInitTable(WebAssembly.Table);

// ---- WebAssembly.Global ----
WebAssembly.Global = function Global(descriptor, value) {
	this._global = $.wasmNewGlobal(descriptor);
	this._descriptor = descriptor;
	this._initialValue = value;
};

Object.defineProperty(WebAssembly.Global.prototype, 'value', {
	get: function () {
		return $.wasmGlobalGet(this._global);
	},
	set: function (v) {
		return $.wasmGlobalSet(this._global, v);
	},
	configurable: true,
});

// ---- Helper: wrap an exported function ----
function wrapExportedFunc(funcOpaque) {
	return function () {
		var args = [funcOpaque];
		for (var i = 0; i < arguments.length; i++) {
			args.push(arguments[i]);
		}
		return $.wasmCallFunc.apply($, args);
	};
}

// ---- WebAssembly.Instance ----
WebAssembly.Instance = function Instance(module, importObject) {
	// Build the flat imports array that the native side expects
	var importsArray = [];

	if (importObject) {
		// Get module's import descriptors to know what's needed
		var importDescs = $.wasmModuleImports(module._module);
		for (var i = 0; i < importDescs.length; i++) {
			var desc = importDescs[i];
			var mod = importObject[desc.module];
			if (!mod) continue;

			var val = mod[desc.name];
			if (val === undefined) continue;

			var entry = {
				module: desc.module,
				name: desc.name,
				kind: desc.kind,
				val: val,
			};

			// If importing a Global, pass initial value
			if (desc.kind === 'global' && val instanceof WebAssembly.Global) {
				entry.val = val._global;
				entry.i = val._initialValue;
			} else if (desc.kind === 'memory' && val.buffer !== undefined) {
				// Memory import — pass the native memory object directly
				entry.val = val;
			}

			importsArray.push(entry);
		}
	}

	var result = $.wasmNewInstance(module._module, importsArray);
	this._instance = result[0];
	var exportsArray = result[1];

	// Build exports object
	this.exports = {};
	for (var i = 0; i < exportsArray.length; i++) {
		var exp = exportsArray[i];
		if (exp.kind === 'function') {
			this.exports[exp.name] = wrapExportedFunc(exp.val);
		} else if (exp.kind === 'memory') {
			this.exports[exp.name] = exp.val;
		} else if (exp.kind === 'global') {
			// Wrap in a Global-like object with value getter/setter
			var g = Object.create(WebAssembly.Global.prototype);
			g._global = exp.val;
			this.exports[exp.name] = g;
		} else if (exp.kind === 'table') {
			// Wrap table with get() and length
			var tbl = exp.val;
			var tableObj = {
				get: function (index) {
					var fn = $.wasmTableGet(tbl, index);
					if (fn === null) return null;
					return wrapExportedFunc(fn);
				},
			};
			Object.defineProperty(tableObj, 'length', {
				get: function () {
					return tbl.length;
				},
			});
			this.exports[exp.name] = tableObj;
		}
	}
};

// ---- WebAssembly.compile() ----
WebAssembly.compile = function (bufferSource) {
	return new WebAssembly.Module(bufferSource);
};

// ---- WebAssembly.instantiate() ----
WebAssembly.instantiate = function (bufferSourceOrModule, importObject) {
	var module;
	if (bufferSourceOrModule instanceof WebAssembly.Module) {
		module = bufferSourceOrModule;
	} else {
		module = new WebAssembly.Module(bufferSourceOrModule);
	}
	var instance = new WebAssembly.Instance(module, importObject);
	return { module: module, instance: instance };
};
