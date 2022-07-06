## Overview

DSML2 is a new document markup language that is designed to produce PDF output
from human-editable input files. DSML2 is a subset of the JSON data interchange
format. The best way to think about the relationship between JSON and DSML2 is
that it is like the relationship between XML and HTML: everything written in the
latter is valid in the former, but the latter has special control structures
that inform the interpreter how to handle the data. Like Markdown and LaTeX,
DSML2 is a language that describes the content and style of the document output,
but unlike those languages, it is designed to separate the style and content
into completely different files. This makes it easy to "mix and match" content
and style templates like one would switch themes on a website, or apply
different styling to presentation slides.

I wrote the original DSML to be a compilation target for other programs. As
such, DSML1 was very low level and hard to work with. DSML2 is suitable as a
compilation target, but it is also much easier for a human to understand and
edit with a traditional text editor. Furthermore, because the language is valid
JSON, syntax highlighting should work out of the box.

This primer serves as an introduction to DSML2 and its interpreter. Knowledge
of JSON is a requirement for understanding this document.

## Quick Start

The following is a trivial example of a valid DSML2 document. The input consists
of two files: `content.json` contains the content, and `stylesheet.json`
contains the style information. The output is a PDF document that is blank
except for the string "Hello, World!" printed in the default size, color, and
font face, at the location 72x72 from the upper left hand corner of the page.
Please note that by default, DSML2 uses 72 dots per inch, thus the text in this
example will be 1x1 inche from the upper left hand corner of the page.

### content.json

```json
{
  "hello": "Hello, World!"
}
```

### stylesheet.json

```json
{
  "hello": {
    "_style": {
      "x": 72,
      "y": 72,
    }
  }
}
```

```
./dsml2 -c content.json -s stylesheet.json > out.pdf
```

## Constants

You may place an optional `_constants` section in the root of either the
`content.json` or `stylesheet.json` files. Numerical constants defined in this
section can be referred to elsewhere in the file. Here's an example:

```json
{
  "_constants": {
    "oneinch": 72
  },
  "hello": {
    "_style": {
      "x": "oneinch * 2",
      "y": "oneinch"
    }
  }
}
```

## Style

You may place an optional `_style` section in any node in the stylesheet. The
elements within this section are evaluated for formatting and style purposes.
Valid identifiers include:

- x: The x position in DPI relative to the parent element (Default 0).
- y: The y position in DPI relative to the parent element (Default 0).
- fontsize: The font size in DPI of any text element (Default 12).
- imagescalex: The horizontal scaling modifier for images (Default 1.0).
- imagescaley: The vertical scaling modifier for images (Default 1.0).
- xoffset: A horizontal value that successive elements are shifted by (Default 0).
- yoffset: A vertical value that successive elements are shifted by (Default 0).

Most style information is propagated to child elements. Exceptions include
`xoffset`, and `yoffset`.

### Position

Position elements (E.G. "x" and "y" fields in the `_style` section) are either
numbers or strings. In the number form, arithmetic and constants are not
allowed. In the string form, you may use arbitrarily complicated arithmetic
expressions with the constants you defined in the `_constants` section.

The positions you set with "x" and "y" are relative to the parent element, so if
the parent has an x offset of 100, and the child an offset of 16, then the child
will be placed at 116 dots from the left of the page.

The "xoffset" and "yoffset" fields are different. These fields apply to
successive child elements, compounding as more elements are added. Thus the
children of a parent element that has a yoffset of 16 will be arranged
vertically in an array exactly 16 dots from one another.

### Font and Color

The font face can be selected with the "face" field. You can use any value that
your operating system will understand. For instance, on my computer I can
specify "Sans", "Serif", "Georgia", "PressStart2P" or "Unifont".

The font size is the size in points of the text you want to display. Default is
12.

Color can be specified using the fields "r", "g", "b", and "a". These should all
be values between zero and one, and represent the three color channels and the
alpha channel.

### Text Width

The "textwidth" field defines the width in points at which the text will wrap.
By default the text does not wrap at all. Text with any alignment other than
"left" must have this field set.
