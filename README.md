![Banner](https://s-christy.com/status-banner-service/dsml2/banner-slim.svg)

## Overview

Traditional document preparation systems like Microsoft Word, Libreoffice, and
LaTeX conceptualize text elements as rectangles, and use collision and reflow
algorithms to format text around these rectangles. This is great for long form
textual content like books, essays, and scientific papers, but is a poor
solution for more structured documents like infographics, résumés, and websites.
For the latter category, tree-based document preparation systems like HTML/CSS,
and XML are superior.

DSML Version 2 is a tree based documentation system designed to fill the gap
where Word and LaTeX are too inflexible, and HTML/CSS doesn't apply. DSML2 it is
a subset of the JSON data interchange format that is used for typesetting
documents. This repository contains the source for the interpreter for this
language, and associated sample files.

## Features

- Tree-based document preparation system
- Rich-text formatting with Pango Markdown
- Support for all system typefaces including bold and italicised versions
- Separate content and style JSON files to encourage theming
- Resource transclusion through the `INCLUDE` macro
- Output to PDF
- Layout options like text width, line height, spacing, and alignment
- Support for embedding PNGs and SVGs
- Support for downloading image data from the internet with cURL
- Embedded LUA and JSON interpreters
- Global constants
- Runtime evaluation of LUA expressions for conditional formatting
- Input example files
- Text reflow
- Support for all RGBA colors
- `CURRENT_DATE` macro for printing out date of compilation
- `REV` macro for input file versioning
- Clickable hyperlinks
- Verbose mode for debugging

## Usage

The DSML2 interpreter can be invoked as described in the usage statement:

```
Usage: dsml2 [-c content] [-s stylesheet] [-o output file]
 -c     The file that contains the document content. Default "content.json".
 -s     The file that contains the document style. Default "stylesheet.json".
 -s     The output file. Defaults to stdout.
```

Instructions for writing input files in the DSML language can be found in ![the
DSML2 primer](./PRIMER.md).

## Dependencies

These are the dependencies for DSML2:

```
gcc
libcairo2-dev
libcjson-dev
librsvg2-dev
make
pkg-config
```

`gcc`, `make`, and `pkg-config` are used to build the C program, which is
linked against `libcairo2` and `librsvg2-dev` for the graphical output, and
`libcjson-dev` for parsing input files.

They can be installed with:

```
apt-get install -y gcc libcairo2-dev libcjson-dev librsvg2-dev make pkg-config
```

## License

This work is licensed under the GNU General Public License version 3 (GPLv3).

[<img src="https://s-christy.com/status-banner-service/GPLv3_Logo.svg" width="150" />](https://www.gnu.org/licenses/gpl-3.0.en.html)
