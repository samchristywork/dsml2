CC := gcc

CFLAGS := -g -Wall -Werror -Wpedantic $(shell pkg-config --cflags cairo librsvg-2.0 lua pango pangocairo)
LIBS := $(shell pkg-config --libs cairo librsvg-2.0 lua pango pangocairo) -lcjson -lcurl

all: build/dsml2

build/dsml2: src/dsml2.c
	mkdir -p build/
	${CC} src/dsml2.c ${CFLAGS} -o $@ ${LIBS}

sample:
	make
	./build/dsml2 -c example/simple/content.json -s example/simple/stylesheet.json > build/out.pdf

valgrind:
	make
	valgrind --leak-check=full ./build/dsml2 -c ./example/simple/content.json -s ./example/simple/stylesheet.json -o build/out.pdf

clean:
	rm -rf build/
