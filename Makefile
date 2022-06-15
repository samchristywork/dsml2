CC := gcc

CFLAGS := -Wall -Werror -Wpedantic $(shell pkg-config --cflags cairo librsvg-2.0 lua)
LIBS := $(shell pkg-config --libs cairo librsvg-2.0 lua) -lcjson -lcurl

all: build/dsml2

build/dsml2: src/dsml2.c
	mkdir -p build/
	${CC} src/dsml2.c ${CFLAGS} -o $@ ${LIBS}

clean:
	rm -rf build/

