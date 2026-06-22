# h1-wasm-blink - A4c proof-of-life DSL source.
#
# Regenerate module_wasm.h with:
#   cd ~/Documents/local/source/studio
#   npm run dsl-to-wasm -- \
#     ~/Documents/local/source/tiles/examples/h1-wasm-blink/module.dsl \
#     --header=~/Documents/local/source/tiles/examples/h1-wasm-blink/module_wasm.h \
#     --core=Core.ST.H5
#
# Then rebuild the firmware with: make.

import Core.LED.heartbeat(period_ms: int)

loop {
  Core.LED.heartbeat(500)
}
