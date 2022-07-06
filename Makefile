CC := gcc

CFLAGS := -g -Wall -Werror -Wpedantic $(shell pkg-config --cflags cairo librsvg-2.0 lua pango pangocairo)
LIBS := $(shell pkg-config --libs cairo librsvg-2.0 lua pango pangocairo) -lcjson -lcurl -lz

all: build/dsml2

build/io.o: src/io.*
	mkdir -p build/
	${CC} src/io.c -c ${CFLAGS} -o $@ ${LIBS}

build/style.o: src/style.*
	mkdir -p build/
	${CC} src/style.c -c ${CFLAGS} -o $@ ${LIBS}

build/traverse.o: src/traverse.*
	mkdir -p build/
	${CC} src/traverse.c -c ${CFLAGS} -o $@ ${LIBS}

build/render.o: src/render.* src/style.h src/version.h
	mkdir -p build/
	${CC} src/render.c -c ${CFLAGS} -o $@ ${LIBS}

build/dsml2: src/dsml2.* src/render.* build/render.o build/traverse.o build/style.o build/io.o
	mkdir -p build/
	${CC} src/dsml2.c build/*.o ${CFLAGS} -o $@ ${LIBS}

sample:
	make
	./build/dsml2 -c example/simple/content.json -s example/simple/stylesheet.json > build/out.pdf

valgrind:
	make
	valgrind --leak-check=full ./build/dsml2 -c ./example/simple/content.json -s ./example/simple/stylesheet.json -o build/out.pdf

clean:
	rm -rf build/
