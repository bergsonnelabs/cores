;; Studio A4c proof-of-life module. Matches what the DSL compiler
;; emits for:
;;
;;   import Core.LED.heartbeat(period_ms: int)
;;   loop { Core.LED.heartbeat(500) }
;;
;; studio_start runs once at boot (empty here); studio_loop is
;; called by the host in a tight outer loop, and each iteration
;; issues one host call into core_led_heartbeat. The real SDK
;; handles the blink timing on the MCU's side — the module just
;; re-asserts the requested period.

(module
  (import "env" "core_led_heartbeat" (func $heartbeat (param i32)))
  (func (export "studio_start"))
  (func (export "studio_loop")
    (call $heartbeat (i32.const 500))))
