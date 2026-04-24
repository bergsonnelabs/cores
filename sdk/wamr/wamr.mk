# WAMR integration for cores — interpreter-only configuration.
#
# Included by the top-level Makefile when `WAMR_ENABLED=1`. Adds the
# WAMR source set, include paths, and feature-toggle defines to the
# main build. Object files land in `$(BUILD_DIR)/sdk/wamr/` and link
# into the final firmware alongside the rest of the SDK.
#
# Scope (A4c):
#   - Interpreter mode only (classic + fast interp). No AOT loader,
#     no JIT, no WAMR compiler. Modules ship as plain `.wasm`
#     bytecode; the on-device loader reads WebAssembly v1 directly.
#   - Single-threaded. No pthread / thread-mgr / WASI / libc-builtin.
#   - Bare-metal platform layer (sdk/wamr/platform_*) — no OS calls;
#     malloc/free forward to newlib's heap (managed by cores' _sbrk).
#
# Intentional exclusions that keep the footprint tight:
#   - Bulk memory, shared memory, shared heap, ref types, GC, tail
#     calls, SIMD, exception handling.
#   - Debug / dump-call-stack / memory+perf profiling.
#   - Mini-loader (broader compat rejects some valid modules; not
#     worth 1-2 KB when flash is in the hundreds of KB).
#
# Targets: Core.H (STM32H523, Cortex-M33 +FPU) and Core.W (STM32WBA55,
# Cortex-M33 +FPU). Core.U (Cortex-M4) is not supported here — its
# footprint was measured over-budget during the A4b spike; its path
# is a separate Wasm-subset interpreter (future work).

WAMR_DIR       := $(SDK_DIR)third_party/wasm-micro-runtime
WAMR_CORE      := $(WAMR_DIR)/core
WAMR_PAL_DIR   := $(SDK_DIR)sdk/wamr

# ---- Feature toggles passed to every WAMR translation unit ----
# These MUST match between the library build and any consumer (they
# leak through the public headers), which is why they live here and
# get tacked onto CFLAGS wholesale.
WAMR_DEFS := \
  -DBH_PLATFORM_BARE_METAL \
  -DBUILD_TARGET_THUMB_VFP \
  -DBUILD_TARGET=\"THUMBV7EM_VFP\" \
  -DWASM_ENABLE_INTERP=1 \
  -DWASM_ENABLE_FAST_INTERP=1 \
  -DWASM_ENABLE_AOT=0 \
  -DWASM_ENABLE_JIT=0 \
  -DWASM_ENABLE_WAMR_COMPILER=0 \
  -DWASM_ENABLE_LIBC_BUILTIN=0 \
  -DWASM_ENABLE_LIBC_WASI=0 \
  -DWASM_ENABLE_MULTI_MODULE=0 \
  -DWASM_ENABLE_SHARED_MEMORY=0 \
  -DWASM_ENABLE_SHARED_HEAP=0 \
  -DWASM_ENABLE_BULK_MEMORY=0 \
  -DWASM_ENABLE_REF_TYPES=0 \
  -DWASM_ENABLE_GC=0 \
  -DWASM_ENABLE_TAIL_CALL=0 \
  -DWASM_ENABLE_SIMD=0 \
  -DWASM_ENABLE_EXCE_HANDLING=0 \
  -DWASM_ENABLE_THREAD_MGR=0 \
  -DWASM_ENABLE_LIB_PTHREAD=0 \
  -DWASM_ENABLE_LIB_PTHREAD_SEMAPHORE=0 \
  -DWASM_ENABLE_LIB_WASI_THREADS=0 \
  -DWASM_ENABLE_DEBUG_AOT=0 \
  -DWASM_ENABLE_DEBUG_INTERP=0 \
  -DWASM_ENABLE_DUMP_CALL_STACK=0 \
  -DWASM_ENABLE_MEMORY_PROFILING=0 \
  -DWASM_ENABLE_PERF_PROFILING=0 \
  -DWASM_ENABLE_MINI_LOADER=0 \
  -DWASM_ENABLE_CUSTOM_NAME_SECTION=0 \
  -DWASM_ENABLE_SPEC_TEST=0 \
  -DWASM_DISABLE_HW_BOUND_CHECK=1 \
  -DWASM_DISABLE_STACK_HW_BOUND_CHECK=1 \
  -DWASM_DISABLE_WAKEUP_BLOCKING_OP=1 \
  -DBH_MALLOC=wasm_runtime_malloc \
  -DBH_FREE=wasm_runtime_free

# ---- Include paths ----
# Internal paths needed to build WAMR itself.
WAMR_INCS := \
  -I$(WAMR_PAL_DIR) \
  -I$(WAMR_CORE) \
  -I$(WAMR_CORE)/iwasm/include \
  -I$(WAMR_CORE)/iwasm/common \
  -I$(WAMR_CORE)/iwasm/interpreter \
  -I$(WAMR_CORE)/shared/platform/include \
  -I$(WAMR_CORE)/shared/utils \
  -I$(WAMR_CORE)/shared/utils/uncommon \
  -I$(WAMR_CORE)/shared/mem-alloc

# User code (a project's main.c) needs the embedder-facing header
# set — just `wasm_export.h` and friends. Tacked onto CFLAGS so
# projects with WAMR_ENABLED=1 can `#include "wasm_export.h"`.
CFLAGS += -I$(WAMR_CORE)/iwasm/include

# ---- Source set (interpreter-only) ----
# iwasm/interpreter — loader + fast interpreter. Fast interp is a
# preprocessing pass on load that converts the stack-based ops into
# a threaded form amenable to a hot dispatch loop; it uses a
# different WASMInterpFrame layout than classic, so the two are
# mutually exclusive. Fast is the default. (Classic is smaller but
# meaningfully slower; revisit if footprint becomes the blocker.)
WAMR_SRCS := \
  $(WAMR_CORE)/iwasm/interpreter/wasm_loader.c \
  $(WAMR_CORE)/iwasm/interpreter/wasm_interp_fast.c \
  $(WAMR_CORE)/iwasm/interpreter/wasm_runtime.c \
  $(WAMR_CORE)/iwasm/common/wasm_application.c \
  $(WAMR_CORE)/iwasm/common/wasm_runtime_common.c \
  $(WAMR_CORE)/iwasm/common/wasm_native.c \
  $(WAMR_CORE)/iwasm/common/wasm_exec_env.c \
  $(WAMR_CORE)/iwasm/common/wasm_memory.c \
  $(WAMR_CORE)/iwasm/common/wasm_loader_common.c \
  $(WAMR_CORE)/iwasm/common/wasm_blocking_op.c \
  $(WAMR_CORE)/iwasm/common/wasm_shared_memory.c \
  $(WAMR_CORE)/shared/utils/bh_assert.c \
  $(WAMR_CORE)/shared/utils/bh_bitmap.c \
  $(WAMR_CORE)/shared/utils/bh_common.c \
  $(WAMR_CORE)/shared/utils/bh_hashmap.c \
  $(WAMR_CORE)/shared/utils/bh_leb128.c \
  $(WAMR_CORE)/shared/utils/bh_list.c \
  $(WAMR_CORE)/shared/utils/bh_log.c \
  $(WAMR_CORE)/shared/utils/bh_queue.c \
  $(WAMR_CORE)/shared/utils/bh_vector.c \
  $(WAMR_CORE)/shared/utils/runtime_timer.c \
  $(WAMR_CORE)/shared/mem-alloc/mem_alloc.c \
  $(WAMR_CORE)/shared/mem-alloc/ems/ems_alloc.c \
  $(WAMR_CORE)/shared/mem-alloc/ems/ems_kfc.c \
  $(WAMR_CORE)/shared/mem-alloc/ems/ems_hmu.c \
  $(WAMR_PAL_DIR)/platform_stubs.c

# thumb-VFP native-call trampoline — used when Wasm calls into a
# registered C host function. The .s suffix matters: the top-level
# Makefile's assembler rule matches on it.
WAMR_ASM_SRCS := \
  $(WAMR_CORE)/iwasm/common/arch/invokeNative_thumb_vfp.s

# ---- Object files ----
# Flatten into build/sdk/wamr/ regardless of source tree depth. The
# top-level Makefile mirrors the per-subdir-per-object layout; we
# intentionally collapse here because WAMR's source tree has
# overlapping basenames (wasm_runtime.c exists in both common/ and
# interpreter/, same applies to runtime_common.c's sibling). Collapsing
# breaks; so we nest-by-parent-dir instead.
WAMR_OBJS := $(patsubst $(WAMR_DIR)/%.c,$(BUILD_DIR)/third_party/wasm-micro-runtime/%.o,$(filter $(WAMR_DIR)/%,$(WAMR_SRCS)))
WAMR_OBJS += $(patsubst $(WAMR_PAL_DIR)/%.c,$(BUILD_DIR)/sdk/wamr/%.o,$(filter $(WAMR_PAL_DIR)/%,$(WAMR_SRCS)))
WAMR_ASM_OBJS := $(patsubst $(WAMR_DIR)/%.s,$(BUILD_DIR)/third_party/wasm-micro-runtime/%.o,$(WAMR_ASM_SRCS))

WAMR_ALL_OBJS := $(WAMR_OBJS) $(WAMR_ASM_OBJS)

# ---- Rules ----
# WAMR sources don't need the GEN_HEADERS dep — they're external and
# don't reach into cores' generated config. This keeps clean builds
# from re-running coregen unnecessarily when only WAMR changes.

$(BUILD_DIR)/third_party/wasm-micro-runtime/%.o: $(WAMR_DIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) $(WAMR_DEFS) $(WAMR_INCS) $(WAMR_WARN_SILENCE) -c $< -o $@

$(BUILD_DIR)/third_party/wasm-micro-runtime/%.o: $(WAMR_DIR)/%.s
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  AS    $(notdir $<)"
	$(Q)$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/sdk/wamr/%.o: $(WAMR_PAL_DIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) $(WAMR_DEFS) $(WAMR_INCS) $(WAMR_WARN_SILENCE) -c $< -o $@

# WAMR's codebase triggers a lot of warnings under cores' -Wall
# -Wextra -Wshadow -Wdouble-promotion baseline. We silence the ones
# that fire on vendor code without hiding genuine errors. Kept in a
# dedicated variable so WAMR sources get it but cores sources don't.
WAMR_WARN_SILENCE := \
  -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
  -Wno-implicit-fallthrough -Wno-strict-aliasing \
  -Wno-shadow -Wno-double-promotion \
  -Wno-error=int-conversion -Wno-error=incompatible-pointer-types \
  -Wno-error=implicit-function-declaration
