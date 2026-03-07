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

### Font Tables
- [x] TrueType file parsing
- [x] cmap
- [ ] kerning (kern)
- [ ] metrics (hhea, hmtx)

### cmap Subtable Support
- [x] Format 0
- [ ] Format 2
- [ ] Format 4
- [x] Format 6
- [ ] Format 8
- [ ] Format 10
- [ ] Format 12
- [ ] Format 13
- [ ] Format 14

### Glyph Parsing
- [x] Simple glyphs (glyf)
- [ ] Compound glyphs (glyf)
- [ ] CFF glyphs

### Hinting
- [ ] glyf instruction interpreter

### Rendering
- [ ] Outline rasterization
- [ ] Anti-aliasing
      
## Testing
- [ ] Unit tests
- [ ] Fuzz testing
- [ ] Corpus testing with real-world fonts
