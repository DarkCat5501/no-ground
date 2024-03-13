BUILDDIR=build
SRCDIR=engine
INCLUDES=-I./engine -I./include/ -I./server/

CC=clang
CFLAGS=-Wall
LFLAGS=-lssl

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


no_ground_server: ${BUILDDIR}/server/server.o ${BUILDDIR}/server/thpool.o \
	${BUILDDIR}/server/base64.o ${BUILDDIR}/server/sha1.o ${BUILDDIR}/server/ws.o\
	${BUILDDIR}/server/hashmap.o ${BUILDDIR}/server/conn.o
	${CC} ${LFLAGS} -o $@ $+

${BUILDDIR}/server/server.o: server/server.c include/core.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/server.c -o $@ -ggdb

${BUILDDIR}/server/thpool.o: server/thpool.c include/core.h server/thpool.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/thpool.c -o $@ -ggdb

${BUILDDIR}/server/base64.o: server/base64.c server/base64.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/base64.c -o $@ -ggdb

${BUILDDIR}/server/sha1.o: server/sha1.c server/sha1.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/sha1.c -o $@ -ggdb

${BUILDDIR}/server/ws.o: server/ws.c server/ws.h include/core.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/ws.c -o $@ -ggdb

${BUILDDIR}/server/hashmap.o: server/hashmap.c server/hashmap.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/hashmap.c -o $@ -ggdb

${BUILDDIR}/server/conn.o: server/conn.c server/conn.h include/core.h
	${mkdir_guard}
	${CC} ${CFLAGS} ${INCLUDES} -I./server/ -c server/conn.c -o $@ -ggdb

certificate:
	openssl req \
    -new \
    -newkey rsa:2048 \
    -days 365 \
    -nodes \
    -x509 \
    -subj "/C=BR/ST=Paraiba/L=João-Pessoa/O=DIS/CN=www.noground.com" \
    -keyout certificate.key \
    -out certificate.cert
