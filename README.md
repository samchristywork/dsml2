![Banner](https://s-christy.com/status-banner-service/dsml2/banner-slim.svg)

## Overview

DSML2 is a subset of the JSON programming language that is used for typesetting
documents. This repository contains the source for the interpreter for this
language, and associated sample files.

## Usage

```
Usage: ./build/dsml2 [-c content] [-s stylesheet] [-o output file]
 -c     The file that contains the document content. Default "content.json".
 -s     The file that contains the document style. Default "stylesheet.json".
 -s     The output file. Defaults to stdout.
```

## Dependencies

These are the dependencies for DSML2:

```
gcc
libcairo2-dev
libcjson-dev
make
pkg-config
```

`gcc`, `make`, and `pkg-config` are used to build the C program, which is
linked against `libcairo2` for the graphical output, and `libcjson-dev` for
parsing input files.

They can be installed with:

```
apt-get install -y gcc libcairo2-dev libcjson-dev make pkg-config
```

## License

This work is licensed under the GNU General Public License version 3 (GPLv3).

[<img src="https://s-christy.com/status-banner-service/GPLv3_Logo.svg" width="150" />](https://www.gnu.org/licenses/gpl-3.0.en.html)
