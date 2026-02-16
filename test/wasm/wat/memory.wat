(module
  (memory (export "memory") 1)
  (func (export "store") (param i32 i32)
    local.get 0
    local.get 1
    i32.store)
  (func (export "load") (param i32) (result i32)
    local.get 0
    i32.load))
