(module
  (import "env" "log" (func $log (param i32)))
  (func (export "callLog") (param i32)
    local.get 0
    call $log))
