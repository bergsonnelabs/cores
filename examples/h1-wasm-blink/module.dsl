# h1-wasm-blink - A4c proof-of-life DSL source.
#
# Regenerate module_wasm.h with:
#   cd ~/Documents/local/source/tessera
#   npm run dsl-to-wasm -- \
#     ~/Documents/local/source/cores/examples/h1-wasm-blink/program.dsl \
#     --header=~/Documents/local/source/cores/examples/h1-wasm-blink/module_wasm.h \
#     --core=Core.H
#
# Then rebuild the firmware with: make.

import Core.LED.heartbeat(period_ms: int)

loop {
  Core.LED.heartbeat(500)
}
