BUILDDIR=build
SRCDIR=engine
INCLUDES=-I./engine



# Wasm build variables
WCC=clang
WLD=wasm-ld



WCFLAGS=--target=wasm32 -nostdlib
WLFLAGS=--no-entry --export-dynamic

mkdir_guard=@mkdir -p $(@D)



${BUILDDIR}/web/module: ${BUILDDIR}/web/engine.o ${BUILDDIR}/web/module.wasm

${BUILDDIR}/web/module.wasm: ${BUILDDIR}/web/engine.o
	$(WLD) $(WLFLAGS) -o $@ $<

${BUILDDIR}/web/engine.o: engine/engine.c engine/wasmstd.h
	$(mkdir_guard)
	$(WCC) $(WCFLAGS) $(INCLUDES) -c -o $@ engine/engine.c
