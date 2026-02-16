(module
  (table (export "table") 2 funcref)
  (func $add (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  (func $sub (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.sub)
  (elem (i32.const 0) $add $sub)
  (func (export "callIndirect") (param i32 i32 i32) (result i32)
    local.get 0
    local.get 1
    local.get 2
    call_indirect (param i32 i32) (result i32)))
