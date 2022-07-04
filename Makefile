CC := gcc

CFLAGS := -g -Wall -Werror -Wpedantic $(shell pkg-config --cflags cairo librsvg-2.0 lua pango pangocairo)
LIBS := $(shell pkg-config --libs cairo librsvg-2.0 lua pango pangocairo) -lcjson -lcurl -lz

all: build/dsml2

build/render.o: src/render.* src/style.h src/version.h
	mkdir -p build/
	${CC} src/render.c -c ${CFLAGS} -o $@ ${LIBS}

build/dsml2: src/dsml2.* src/render.* build/render.o
	mkdir -p build/
	${CC} src/dsml2.c build/render.o ${CFLAGS} -o $@ ${LIBS}

sample:
	make
	./build/dsml2 -c example/simple/content.json -s example/simple/stylesheet.json > build/out.pdf

valgrind:
	make
	valgrind --leak-check=full ./build/dsml2 -c ./example/simple/content.json -s ./example/simple/stylesheet.json -o build/out.pdf

clean:
	rm -rf build/
