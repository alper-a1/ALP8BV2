#!/usr/bin/env python3
"""fontgen.py — regenerate the SystemVerilog font package from the CP437 ROM master.

    master:  shared/font/cp437_ibm_ega.rom   (2048 bytes: 256 chars x 8 row-bytes,
             bit 7 = leftmost pixel, row 0 = top)      <- THE MASTER, never edited here
    output:  shared/font/cp437_ibm_ega.sv    (package cp437_ibm_ega, generated)

RTL contract of the generated package (SVO-compatible one-shot lookup, as in
Clifford Wolf's svo_term):

    pixel(char c, row y, col x)  ==  FONT_CP437_IBM_EGA_8x8[{c, y, x}]
        y = 0 is the TOP row, x = 0 is the LEFTMOST pixel.

i.e. bit index c*64 + y*8 + x, counting from the LSB end of the vector, where
char 0 sits at the LSB end (so the literal is written char 255 -> char 0).
Equivalent byte transform on the master: reverse byte order, bit-reverse each byte.
Self-checks on every run (aborts on failure):
  - master is exactly 2048 bytes
  - round-trip: re-parse the emitted file, reconstruct the master byte-for-byte
  - ASCII-art previews drawn FROM THE EMITTED FILE (not from the input)
"""

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def bit_reverse(byte: int) -> int:
    return int(f"{byte:08b}"[::-1], 2)


def build_sv(rom: bytes) -> str:
    # written order = chars 255..0, rows bottom..top, each row byte bit-reversed
    out_bytes = [bit_reverse(b) for b in reversed(rom)]
    assert len(out_bytes) * 8 == 16384

    lines = []
    for i in range(0, len(out_bytes), 16):
        chunk = out_bytes[i : i + 16]
        lines.append("    " + ", ".join(f"8'h{b:02x}" for b in chunk) + ",")
    lines[-1] = lines[-1][:-1] + "};"  # last line closes the concatenation

    return f"""/*
  GENERATED FILE - do not hand-edit!
  Regenerate with:  python3 tools/fontgen.py
  Master:           shared/font/cp437_ibm_ega.rom (raw bytemap of IBM EGA CP437 8x8)
  Obtained from:    https://int10h.org/oldschool-pc-fonts/fontlist/font?ibm_bios

  Lookup contract (SVO-compatible one-shot index):
      pixel(char c, row y, col x) == FONT_CP437_IBM_EGA_8x8[{{c, y, x}}]
      y = 0 top row, x = 0 leftmost pixel, char 0 at the LSB end of the vector.
*/

`ifndef CP437_IBM_EGA_SV
`define CP437_IBM_EGA_SV

package cp437_ibm_ega;

localparam logic [16383:0] FONT_CP437_IBM_EGA_8x8 = {{
{chr(10).join(lines)}

endpackage

`endif // CP437_IBM_EGA_SV
"""


def parse_sv(text: str) -> int:
    """Parse the emitted file back into a packed bit-vector int (bit 0 = LSB)."""
    body = text.split("FONT_CP437_IBM_EGA_8x8 = {", 1)[1].split("};", 1)[0]
    vals = [int(v, 16) for v in re.findall(r"8'h([0-9a-fA-F]{2})", body)]
    assert len(vals) == 2048, f"expected 2048 byte literals, got {len(vals)}"
    vec = 0
    for b in vals:  # first written = MSB end
        vec = (vec << 8) | b
    return vec


def pixel(vec: int, c: int, y: int, x: int) -> bool:
    return (vec >> (c * 64 + y * 8 + x)) & 1


def render(vec: int, c: int, name: str) -> str:
    rows = [
        "".join("#" if pixel(vec, c, y, x) else "." for x in range(8)) for y in range(8)
    ]
    return f"  char 0x{c:02X} ({name}):\n" + "\n".join(f"    |{r}|" for r in rows)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("-i", "--rom", type=pathlib.Path,
                    default=ROOT / "shared/font/cp437_ibm_ega.rom")
    ap.add_argument("-o", "--out", type=pathlib.Path,
                    default=ROOT / "shared/font/cp437_ibm_ega.sv")
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    if len(rom) != 2048:
        sys.exit(f"ERROR: master {args.rom} is {len(rom)} bytes, expected 2048")

    text = build_sv(rom)
    vec = parse_sv(text)

    # round-trip: reconstructed master must be byte-identical to the input
    rebuilt = bytes(
        sum(pixel(vec, c, y, x) << (7 - x) for x in range(8))
        for c in range(256)
        for y in range(8)
    )
    if rebuilt != rom:
        sys.exit("ERROR: round-trip check FAILED - emitted data != master rom")

    args.out.write_text(text)
    print(f"wrote {args.out} ({len(text)} bytes) - round-trip OK\n")
    print(render(vec, 0x41, "'A'"))
    print(render(vec, 0x2D, "'-'"))
    print(render(vec, 0xB3, "CP437 box-drawing vertical"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
