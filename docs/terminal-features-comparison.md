# Terminal Features Comparison: SwiftTerm vs termihui2

> Last updated: 2026-02-15
>
> Reference: [SwiftTerm](https://github.com/migueldeicaza/SwiftTerm)

## Legend

- **Parity** — feature is implemented at comparable level
- **Behind** — partially implemented (parsed but ignored, or incomplete)
- **Missing** — not implemented at all

---

## 1. Terminal Emulation Level

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| VT100/VT220/VT320/VT420/VT500 | Full (DECSCL conformance levels) | Basic ANSI/xterm only | **Behind** |
| xterm-256color compatibility | Full | Partial | **Behind** |

## 2. Escape Sequence Parsing

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| CSI sequences | Full set | Basic set (cursor, erase, scroll, SGR) | **Behind** |
| OSC sequences | Full set (0–12, 52, 133, 777, 1337…) | OSC 0/1/2, OSC 7, OSC 133 | **Behind** |
| DCS sequences | Full (Sixel, DECRQSS, etc.) | Not implemented | **Missing** |
| APC sequences | Supported (Kitty graphics) | Not implemented | **Missing** |
| 7-bit and 8-bit control codes | Both supported | 7-bit only | **Behind** |

## 3. Color Support

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| 16 ANSI colors (30–37, 40–47, 90–97, 100–107) | Yes | Yes | **Parity** |
| 256-color palette (38;5;N / 48;5;N) | Yes | Yes | **Parity** |
| True Color 24-bit RGB (38;2;R;G;B) | Yes (both `:` and `;` separators) | Yes (`;` separator only) | **~Parity** |
| Underline color (SGR 58:2 / 58:5) | Yes | No | **Missing** |
| OSC 4 (query/change color index) | Yes | No | **Missing** |
| OSC 10/11/12 (fg/bg/cursor color query) | Yes | No | **Missing** |
| OSC 104 (reset color index) | Yes | No | **Missing** |

## 4. Mouse Support

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| X10 mouse protocol (mode 9) | Yes | No | **Missing** |
| VT200 normal tracking (mode 1000) | Yes | No | **Missing** |
| Button event tracking (mode 1002) | Yes | No | **Missing** |
| Any event tracking (mode 1003) | Yes | No | **Missing** |
| UTF-8 mouse encoding (mode 1005) | Yes | No | **Missing** |
| SGR mouse encoding (mode 1006) | Yes | No | **Missing** |
| URxvt mouse encoding (mode 1015) | Yes | No | **Missing** |
| SGR Pixel mouse (mode 1016) | Yes | No | **Missing** |

## 5. Unicode / UTF-8

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Basic UTF-8 encode/decode | Yes | Yes | **Parity** |
| Wide characters (CJK, emoji — double width) | Yes (East Asian Width tables) | No (everything is single-width) | **Missing** |
| Combining characters (diacritics) | Yes | No | **Missing** |
| Grapheme clusters (ZWJ emoji sequences) | Yes | No | **Missing** |
| Variation selectors (VS15/VS16) | Yes | No | **Missing** |
| Skin tone modifiers (Fitzpatrick) | Yes | No | **Missing** |
| Regional indicator sequences (flags) | Yes | No | **Missing** |

## 6. Clipboard

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Native copy/paste (Cmd+C/V) | Yes | Yes | **Parity** |
| OSC 52 (terminal clipboard protocol) | Yes | No | **Missing** |

## 7. Terminal Modes

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Alternate screen buffer (1049/47/1047) | Yes | Yes | **Parity** |
| Bracketed paste mode (2004) | Yes | No | **Missing** |
| Application cursor mode (DECCKM) | Yes | No | **Missing** |
| Application keypad mode | Yes | No | **Missing** |
| Origin mode (DECOM) | Yes | No | **Missing** |
| Autowrap mode (DECAWM) | Yes | Parsed but ignored | **Behind** |
| Cursor visibility (DECTCEM, mode 25) | Yes | Parsed but ignored | **Behind** |
| Reverse wraparound mode | Yes | No | **Missing** |
| Insert mode | Yes | No | **Missing** |
| Focus reporting mode | Yes | No | **Missing** |
| 80/132 column mode (DECCOLM) | Yes | No | **Missing** |
| Synchronized output (DCS 2026) | Yes | No | **Missing** |
| Margin mode (DECLRMM) | Yes | No | **Missing** |

## 8. Scrollback Buffer

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Scrollback history | Yes (configurable, default 500 lines) | No (visible screen only) | **Missing** |
| Reflow on resize | Yes | No | **Missing** |
| Circular buffer implementation | Yes | No | **Missing** |

## 9. Selection

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Mouse drag selection | Yes | Yes | **Parity** |
| Copy to clipboard | Yes | Yes | **Parity** |
| Cross-block selection | N/A | Yes | termihui2 specific |

## 10. Graphics / Images

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Sixel graphics (DCS q) | Yes (full parser) | No | **Missing** |
| iTerm2 inline images (OSC 1337) | Yes | No | **Missing** |
| Kitty graphics protocol (APC G) | Yes (full: transmit, store, place, delete) | No | **Missing** |

## 11. Hyperlinks

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| OSC 8 hyperlinks | Yes (with ID support) | No | **Missing** |

## 12. Window / Tab Title

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| OSC 0/1/2 (set title) | Yes | Yes | **Parity** |
| Title query (CSI 20t/21t) | Yes | No | **Missing** |

## 13. Cursor Styles

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| DECSCUSR (block/underline/bar × blink/steady) | Yes (6 styles) | No | **Missing** |
| Cursor visibility (DECTCEM) | Yes | Parsed but ignored | **Behind** |
| Save/Restore cursor (DECSC/DECRC) | Yes | Parsed but ignored | **Behind** |

## 14. Character Attributes (SGR)

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Bold (1) | Yes | Yes | **Parity** |
| Dim/Faint (2) | Yes | Yes | **Parity** |
| Italic (3) | Yes | Yes | **Parity** |
| Underline (4) | Yes | Yes | **Parity** |
| Blink (5/6) | Yes | Yes | **Parity** |
| Reverse/Inverse (7) | Yes | Yes | **Parity** |
| Hidden (8) | Yes | Yes | **Parity** |
| Strikethrough (9) | Yes | Yes | **Parity** |
| Underline color (58) | Yes | No | **Missing** |
| SGR 0 (reset all) | Yes | Yes | **Parity** |

## 15. Tab Stops

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Fixed tab stops (every 8 columns) | Yes | Yes | **Parity** |
| HTS (set tab, ESC H) | Yes | No | **Missing** |
| TBC (clear tabs, CSI g) | Yes | No | **Missing** |

## 16. Character Sets / Line Drawing

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| DEC line drawing (G0, ESC(0) | Yes | No | **Missing** |
| G0/G1/G2/G3 character set switching | Yes | No | **Missing** |
| National character sets (British, German, etc.) | Yes (12+ sets) | No | **Missing** |
| Single/Locking Shift (SS2, SS3, LS0–LS3) | Yes | No | **Missing** |

## 17. Terminal Reset

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| RIS — Hard reset (ESC c) | Yes | Yes | **Parity** |
| DECSTR — Soft reset (CSI ! p) | Yes | No | **Missing** |

## 18. Terminal Resize

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Dynamic resize (TIOCSWINSZ) | Yes | Yes | **Parity** |
| Content reflow on resize | Yes | No | **Missing** |
| Window size reports (CSI 18t/19t) | Yes | No | **Missing** |

## 19. DEC Rectangular Operations

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| DECCRA (Copy Rectangular Area) | Yes | No | **Missing** |
| DECFRA (Fill Rectangular Area) | Yes | No | **Missing** |
| DECERA (Erase Rectangular Area) | Yes | No | **Missing** |
| DECSERA (Selective Erase Rect Area) | Yes | No | **Missing** |
| DECIC / DECDC (Insert/Delete Column) | Yes | No | **Missing** |

## 20. Device Status Reports

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| DSR 5 (status report) | Yes | No | **Missing** |
| DSR 6 (cursor position report) | Yes | No | **Missing** |
| DA (Device Attributes) | Yes | No | **Missing** |
| DECRQSS (Request Status String) | Yes | No | **Missing** |

## 21. Other Features

| Feature | SwiftTerm | termihui2 | Status |
|---------|-----------|-----------|--------|
| Bell (BEL) | Yes | Yes | **Parity** |
| OSC 7 (current working directory) | Yes | Yes | **Parity** |
| OSC 133 (command tracking / shell integration) | Yes | Yes | **Parity** |
| Search in buffer (regex) | Yes | No | **Missing** |
| Progress reporting (OSC 9;4) | Yes | No | **Missing** |
| Notifications (OSC 777) | Yes | No | **Missing** |
| Thread safety | Yes | No | **Missing** |
| Left/Right margins (DECSLRM) | Yes | No | **Missing** |

---

## Summary

| Category | Count |
|----------|-------|
| **Parity** (comparable implementation) | ~18 |
| **Behind** (partially implemented / parsed but ignored) | ~5 |
| **Missing** (not implemented) | ~50 |

---

## Top Priority Gaps

These missing features have the **highest impact** on real-world terminal usage:

| Priority | Feature | Impact |
|----------|---------|--------|
| **P0** | Mouse support (modes 1000/1002/1003 + SGR encoding) | TUI apps don't work (htop, mc, vim, lazygit, etc.) |
| **P0** | Application cursor mode (DECCKM) | Arrow keys broken in vim, less, nano, etc. |
| **P0** | Device Attributes / DSR responses | Many apps hang waiting for DA/DSR reply |
| **P1** | Bracketed paste mode (2004) | Pasted text misinterpreted as commands |
| **P1** | Save/Restore cursor (DECSC/DECRC) | TUI layouts break without working save/restore |
| **P1** | Cursor visibility (DECTCEM) | Apps can't hide/show cursor |
| **P1** | Scrollback buffer | Users can't scroll back through output history |
| **P1** | DEC line drawing character set | Box-drawing borders render as garbage in legacy apps |
| **P2** | Wide characters (CJK / emoji) | CJK text and emoji misaligned |
| **P2** | Cursor styles (DECSCUSR) | vim/nvim can't change cursor shape per mode |
| **P2** | Soft reset (DECSTR) | Terminal state recovery issues |
| **P2** | Application keypad mode | Numpad keys misbehave |
| **P3** | OSC 52 clipboard | Remote clipboard integration unavailable |
| **P3** | Hyperlinks (OSC 8) | Clickable links not supported |
| **P3** | Image protocols (Sixel/Kitty/iTerm2) | No inline image display |
| **P3** | Synchronized output (DCS 2026) | Flicker during rapid screen updates |
