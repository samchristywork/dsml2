CC := gcc

CFLAGS := $(shell pkg-config --cflags cairo)
LIBS := $(shell pkg-config --libs cairo) -lcjson

all: build/dsml2

build/dsml2: src/dsml2.c
	mkdir -p build/
	${CC} src/dsml2.c ${CFLAGS} -o $@ ${LIBS}

clean:
	rm -rf build/

