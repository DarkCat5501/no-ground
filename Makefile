BUILDDIR=build
SRCDIR=engine
INCLUDES=-I./engine

CC=clang
CFLAGS=-Wall
LFLAGS=

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

# run all web related test suits
test_web: tests/test_web.js 
	@bun tests/test_web.js


no_ground_server: ${BUILDDIR}/server/server.o ${BUILDDIR}/server/thpool.o ${BUILDDIR}/server/base64.o ${BUILDDIR}/server/sha1.o ${BUILDDIR}/server/ws.o
	${CC} ${LFLAGS} -o $@ $+

${BUILDDIR}/server/server.o: server/server.c
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/server.c -o $@ -ggdb

${BUILDDIR}/server/thpool.o: server/thpool.c
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/thpool.c -o $@ -ggdb

${BUILDDIR}/server/base64.o: server/base64.c
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/base64.c -o $@ -ggdb

${BUILDDIR}/server/sha1.o: server/sha1.c
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/sha1.c -o $@ -ggdb

${BUILDDIR}/server/ws.o: server/ws.c
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/ws.c -o $@ -ggdb
