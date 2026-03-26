# TTF

A lightweight implementation for parsing, rasterizing, and visualizing glyphs
from **TrueType font (`.ttf`) files**.

The project focuses on safely decoding the internal structures of a TrueType
font and exposing glyph data for rendering or inspection. It includes:

- a TrueType parser
- a glyph rasterizer
- a glyph viewer for debugging and exploration

This project is a rewrite of an earlier internal project, rebuilt with a more
structured architecture and expanded functionality.

## Feature Status

Below are a list of currently planned features.

### Font Table Parsing (partial or complete support)
- [x] Font Directory
- [x] head
- [x] maxp
- [x] cmap
    - [x] Format 0
    - [ ] Format 2
    - [ ] Format 4
    - [x] Format 6
    - [ ] Format 8
    - [ ] Format 10
    - [ ] Format 12
    - [ ] Format 13
    - [ ] Format 14
- [x] loca
- [x] glyf
    - [x] simple glyphs
    - [x] compound glyphs
    - [ ] hinting (using built-in instructions)
- [ ] hhea
- [ ] hmtx
- [ ] gvar
- [ ] kern
- [ ] gpos
- [ ] cff
    - [ ] simple glyphs
    - [ ] compound glyphs
- [ ] cff2
    - [ ] simple glyphs
    - [ ] compound glyphs

Additional tables may be added in the future.

### Rendering
- [ ] Outline rasterization
- [ ] Anti-aliasing
      
## Testing
- [ ] Unit tests
- [ ] Fuzz testing
- [ ] Corpus testing with real-world fonts
