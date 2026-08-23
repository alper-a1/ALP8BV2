# ALP8BV2 Assembler

A 6-pass assembler for the ALP8B v1.1 ISA. Five passes are implemented; the final
`write_intel_hex` file-writing pass is pending — `codegen` output currently stops
in memory.

## Pipeline

```
source_path + __built-ins.asm (loader level, each lexed with its own line numbering)
   │
   ▼
lexer (LoadSourceWithBuiltins)     loads & tokenizes files -> vector<TokenizedLine>
   │
   ▼
metadata_pass                      strips '.' metadata lines, populates ProgramMetadata
   │
   ▼
macro_pass (MacroEngine)           two internal passes:
   │                               1. define: '%DEFINE' token substitution (single-pass, chaining allowed)
   │                               2. macro:  '%MACRO' expansion & body label mangling (__<id>_<name>)
   ▼
label_pass                         resolves '$PCSET' & builds SymbolTable;
   │                               attaches resolved PC addresses directly to each line;
   │                               rewrites label references to immediates
   ▼
codegen                            consumes pre-addressed lines (no symbol table — labels are
   │                               already resolved); opcode lookup, encodes bytes, and executes
   │                               '@' data writes (incl. program-range guard)
   ▼
write_intel_hex   (NOT YET IMPLEMENTED)
                               formats final memory buffer + ProgramMetadata to disk
```

**Design invariant:** each pass's *output* is a strict subset of syntax — by the time
`label_pass` runs, no `%` syntax remains; by the time `codegen` runs, no `$`-only
bookkeeping needs further interpretation beyond what `label_pass` already resolved.

## Prefix = pass ownership

Every non-instruction line is classified by its leading sigil, which tells you which
pass owns it — no need to memorize individual directive names to know their timing:

| Prefix | Family | Resolved at | Affects binary? |
|---|---|---|---|
| `.` | metadata | `metadata_pass` | no |
| `%` | macro system | `macro_pass` (two internal passes); `%INCLUDE` deferred to v2 | no |
| `$` | assembler bookkeeping | `label_pass` | no |
| `@` | data-emitting directives | `label_pass` (label resolution in args) + `codegen` (bounds guard + byte write) | **yes** |
| `;` | comment | `lexer` (stripped) | no |
| (none) | true instruction / label def | `label_pass` + `codegen` | yes (instructions) |

---

## Lexer rules

- **Metadata directives** (`.NAME`, `.DESC`, `.DATE`, `.CLOCK`): start with `.`, may appear
  **anywhere** in the file, **no duplicates** allowed. `.NAME` and `.CLOCK` are
  **mandatory**; `.DESC` and `.DATE` are optional. All tokens after the directive are the
  argument — space-joined, cannot span line breaks. `.CLOCK` takes **exactly one** argument:
  a positive decimal integer (uint32, `> 0`); the other three take the rest of the line.
- **Comments**: `;` to end of line. Inline allowed, embedding (i.e. resuming code after a
  comment on the same line) is not — once `;` appears, the rest of the line is comment.
- **Labels**: a single contiguous word ending in `:`, and **nothing else on the line** —
  trailing tokens after a label definition are an error. Labels cannot start with a digit
  or `-` (guarantees a label can never clash with an immediate). Labels should not start
  with `__` (reserved for macro-generated labels) — not enforced by the lexer since
  `label_pass` runs after `macro_pass` has already claimed that namespace.

  ```
  VALID
  my_label:
  lcomment: ; here is a comment!

  INVALID
  loop count:              ; label cannot contain a space
  halt: JMP halt           ; trailing tokens after a label definition
  __label1: ; not strictly invalid, but risks colliding with macro-generated labels
  ```

- **Immediates** (operand position only — never the first token of a line): decimal
  `0–255`, hex `0x00–0xFF`, binary `0b0–0b11111111`, or negative decimal `-128…0`
  (stored two's-complement). A token that fails to parse as one of these is treated
  as an identifier.
- **Instructions**: plain text, may have preceding whitespace and inline comments.

---

## `%` — macro system (textual, resolved before `label_pass`, no binary effect)

### `%INCLUDE` !DEFERRED TO ASSEMBLER V2!
```
%INCLUDE "path/to/file.asm"
```
- Loader-level, not a `label_pass`/`codegen` concern despite living in the same tier as
  `%MACRO`/`%DEFINE`.
- **Convention (v1, unenforced): includes contain definitions only** (`%MACRO`/`%DEFINE`),
  never instructions — no ordering guarantees are made for arbitrary code across includes.
- Each included file is lexed **independently** (its own line-1-relative numbering), then
  its `Line`s are spliced into the stream ahead of the including file's own lines. This
  keeps error messages accurate to whichever physical file a token came from.
- Built-in macros are implemented as an implicit, unconditional
  `%INCLUDE "__built-ins.asm"` injected before any user-level includes.

### `%DEFINE`
```
%DEFINE [name] [value]
```
- Implemented as the first internal pass of `macro_pass` (pre-step before `%MACRO`
  expansion), pure single-pass search-and-replace over **tokens**, not raw text.
- `value` must be a **single token**. If that token is an IDENTIFIER matching a previously
  defined `%DEFINE`, it is resolved immediately (chaining allowed). A define's value is
  never re-scanned recursively — only one level of chain is followed.
- `name` cannot shadow a built-in mnemonic or register name (checked against `BUILTINS`).
- Works inside any instruction / directive / macro definition body.
- Cannot share a name with a `%MACRO` OR its parameters (shared namespace, single dedup check).

### `%MACRO` / `%ENDMACRO`
```
%MACRO [name] [args]
    ...body...
%ENDMACRO
```
- `args`: space-separated; there is **no paren syntax** — a macro with no args is
  invoked by bare name.
- `name`/`args` cannot contain `;` or space.
- `name`/`args` cannot shadow a built-in **mnemonic** (unlike `%DEFINE` names, register
  names are not reserved here).
- Labels created inside a macro body are auto-prefixed `__` and numbered with a
  **global** incrementing instance counter (starting at 0), formatted
  `__<instance>_<name>` — the first expansion of `HALT` yields `__0_halt:`.
- Macro bodies **cannot contain `@`/`$` directives as the first token of a line** — checked
  at *definition* time (reject immediately), not at expansion time.
- Macros **cannot recurse**, and (v1) cannot invoke other macros at all — enforced at
  *expansion* time by checking each body token against known macro names.
- Labels defined inside a macro body cannot shadow a built-in mnemonic or register name.
- No duplicate names; no name shared with a `%DEFINE` (single shared namespace, populated
  in file order — forward references disallowed).
- Macros with no args are valid (simple defines-via-expansion, e.g. `HALT`).
- Errors in expanded code report the macro **definition's** file + line, not the callsite.

**Built-ins** (from `__built-ins.asm`, injected implicitly before any user source):
```
%MACRO HALT
halt:
JMP halt
%ENDMACRO

%MACRO CLR RA
XOR RA RA
%ENDMACRO

%MACRO CBGT RA RB lbl
CBLT RB RA lbl
%ENDMACRO

%MACRO SHL RA
ADD RA RA
%ENDMACRO
```

---

## `$` — assembler bookkeeping (`label_pass` only, no binary effect)

```
$PCSET 0xADDR
```
- Overrides the assembler's program-counter bookkeeping for subsequent address
  calculations. Purely a `label_pass` concern — `codegen` never needs to know this
  happened.
- `addr` must be a **hex literal** (`0x00`–`0xFF`); decimal is rejected.
- **No range or overlap protection**: the PC may be set backwards or into the middle of
  a 2-byte instruction. Overlapping byte writes are the programmer's responsibility
  (by design — no overlap check).

---

## `@` — data-emitting directives (`label_pass` + `codegen`, writes binary)

```
@INI      addr value      ; write value at addr; error if addr falls within
                          ; the program-instruction range
@UINI     addr value      ; write value at addr; no protection check — may
                          ; overwrite program memory
@BLOCK    start end value ; fill [start..end] (inclusive) with value; guarded like
                          ; @INI; start == end is allowed (single byte)
```
- `addr`/`value`/`start`/`end` may be **label references** as well as literals —
  `label_pass` rewrites any label reference to an immediate before `codegen` sees the
  directive.
- The program-range guard runs in `codegen`: it first walks the *entire* line stream and
  builds a live-address bitset of all program instruction bytes, then checks every
  `@INI`/`@BLOCK` byte against it — file order doesn't matter.
- `@UINI` performs no guard and silently overwrites whatever is at the address.

---

## File layout

```
tools/assembler/
├── CMakeLists.txt
├── README.md
├── main.cpp               orchestration only — reads like a 1:1 summary of the pipeline
├── token.hpp              Token, TokenType, TokenizedLine — shared IR, no ISA knowledge
├── asm_error.hpp          shared AsmError type, thrown by every pass
├── isa.hpp                INSTRS, REGISTERS, BUILTINS, OperandShape/InstrDef — ISA data tables
├── lexer.hpp/.cpp         source text -> vector<TokenizedLine> (+ __built-ins.asm injection)
├── metadata_pass.hpp/.cpp ProgramMetadata struct; strips '.' lines
├── macro_pass.hpp/.cpp    '%DEFINE' + '%MACRO' expansion (MacroEngine, two internal passes)
├── label_pass.hpp/.cpp    symbol table construction, '$PCSET', PC attachment, label rewrite
└── codegen.hpp/.cpp       table lookup -> byte encoding, '@' directive writes
```

### `main.cpp` shape (current)

```cpp
auto tokenized      = LoadSourceWithBuiltins(input_path);           // lexer + __built-ins.asm injection
auto [meta_pass, metadata] = ExtractMetadata(std::move(tokenized)); // strips '.' lines
auto expanded       = ResolveDefineAndMacro(std::move(meta_pass));  // '%DEFINE' + '%MACRO' (two internal passes)
auto label_mapped   = ResolveAndMapLabels(std::move(expanded));
auto compiled       = ConvertToMachineCode(label_mapped);
// write_intel_hex — not yet implemented
```
