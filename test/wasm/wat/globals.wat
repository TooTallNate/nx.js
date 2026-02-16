(module
  (global (export "counter") (mut i32) (i32.const 0))
  (func (export "increment") (result i32)
    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0))
