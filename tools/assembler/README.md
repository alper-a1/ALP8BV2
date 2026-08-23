# ALP8BV2 Assembler

A 4-pass assembler for the ALP8B v1.1 ISA, targeting `tools/assembler/`.

## Pipeline

```
source_path + __built-ins.asm (loader level, each lexed with own line numbering)
   │
   ▼
lexer (LoadSourceWithBuiltins)  loads & tokenizes files -> vector<TokenizedLine>
   │
   ▼
metadata_pass                   strips '.' metadata lines, populates ProgramMetadata
   │
   ▼
define_pass                     '%DEFINE' token substitution (single-pass, chaining allowed)
   │
   ▼
macro_pass                      '%MACRO' expansion & body label mangling (__<id>_<name>)
   │
   ▼
label_pass                      resolves '$PCSET' & builds SymbolTable;
   │                            attaches resolved PC addresses directly to each line;
   │                            performs '@INISAFE' bounds validation
   │
   ▼
codegen                         consumes pre-addressed lines + SymbolTable;
                                performs opcode lookup, encodes bytes, and executes '@' data writes
   │
   ▼
write_intel_hex                 formats final memory buffer + ProgramMetadata to disk
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
| `%` | macro system | pre-`macro_pass` (lexer/loader level for `%INCLUDE`; `macro_pass` for `%DEFINE`/`%MACRO`) | no |
| `$` | assembler bookkeeping | `label_pass` | no |
| `@` | data-emitting directives | `label_pass` (address awareness) + `codegen` (byte write) | **yes** |
| `;` | comment | `lexer` (stripped) | no |
| (none) | true instruction / label def | `label_pass` + `codegen` | yes (instructions) |

---

## Lexer rules

- **Metadata directives** (`.NAME`, `.DESC`, `.DATE`, `.CLOCK`): start with `.`, require a
  value, may appear **anywhere** in the file, **no duplicates** allowed. Argument cannot
  span line breaks, all tokens after the directive is treated as part of the argument for that directive.
- **Comments**: `;` to end of line. Inline allowed, embedding (i.e. resuming code after a
  comment on the same line) is not — once `;` appears, the rest of the line is comment.
- **Labels**: single contiguous non-spaced word ending in `:`. Labels should not start with `__`
(reserved for macro-generated labels) — not enforced by the lexer since `label_pass` runs after
  `macro_pass` has already claimed that namespace.

  ```
  VALID
  my_label:
  lcomment: ; here is a comment!

  INVALID
  loop count:              ; label cannot contain a space
  __label1: ; not strictly invalid, but risks colliding with macro-generated labels
  ```

- **Instructions**: plain text, may have preceding whitespace and inline comments.

---

## `%` — macro system (pre-`macro_pass`, textual, no binary effect)

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
- Implemented as a pre-step inside the macro stage (`define_pass`), pure single-pass
  search-and-replace over **tokens**, not raw text.
- `value` must be a **single token**. If that token is an IDENTIFIER matching a previously
  defined `%DEFINE`, it is resolved immediately (chaining allowed). A define's value is never
  re-scanned recursively — only one level of chain is followed.
- `name` cannot shadow a built-in mnemonic or register name (checked against `BUILTINS`).
- Works inside any instruction / directive / macro definition body.
- `name`/`value` must not contain `:` (risk of colliding with label generation).
- Cannot share a name with a `%MACRO` OR its parameters (shared namespace, single dedup check).

### `%MACRO` / `%ENDMACRO`
```
%MACRO [name] [args]
    ...body...
%ENDMACRO
```
- `args`: space-separated; no args means empty parens or no parens at all.
- `name`/`args` cannot contain `;` or space.
- Labels created inside a macro body are auto-prefixed `__` and numbered with a
  **global** incrementing counter, e.g. `__1halt:`, `__2usrmacro:`.
- Macro bodies **cannot contain `@`/`$` directives as the first token of a line** — checked
  at *definition* time (reject immediately), not at expansion time.
- Macros **cannot recurse**, and (v1) cannot invoke other macros at all — enforced at
  *expansion* time by checking each body token against known macro names.
- A macro's name and arguments cannot shadow a built-in mnemonic or register name.
- Labels defined inside a macro body cannot shadow a built-in mnemonic or register name.
- No duplicate names; no name shared with a `%DEFINE`.
- Macros with no args are valid (simple defines-via-expansion, e.g. `HALT`).

**Built-ins** (shadow `%INCLUDE "__built-ins.asm"`):
```
%MACRO HALT
halt:
JMP halt
%ENDMACRO

%MACRO CLR RA
XOR RA RA
%ENDMACRO

%MACRO CBGT RA RB
CBLT RB RA
%ENDMACRO

%MACRO SHL RA
ADD RA RA
%ENDMACRO
```

---

## `$` — assembler bookkeeping (`label_pass` only, no binary effect)

```
$PCSET addr
```
Overrides the assembler's program-counter bookkeeping for subsequent address
calculations. Purely a `label_pass` concern — `codegen` never needs to know this
happened.

---

## `@` — data-emitting directives (`label_pass` + `codegen`, writes binary)

```
@INIT       addr value     ; write value at addr; error if addr falls within
                            ; program-instruction range
@UINIT      addr value     ; write value at addr; no protection check
```
`label_pass` must have already walked the full instruction stream (to know where
program range ends) before the `@INISAFE` bounds check can be performed — this
validation belongs wherever that extent is known, not duplicated across passes.

---

## File layout

```
tools/assembler/
├── CMakeLists.txt
├── README.md
├── main.cpp            orchestration only — reads like a 1:1 summary of the pipeline
├── token.hpp            Token, TokenType, Line — shared IR, no ISA knowledge
├── asm_error.hpp         shared AsmError type, thrown by every pass
├── isa.hpp               INSTRS, REGISTERS, BUILTINS, OperandShape — ISA data tables + builtin name set
├── lexer.hpp/.cpp         source text -> vector<Line>
├── metadata.hpp           ProgramMetadata struct (+ extract_metadata decl, or split out)
├── metadata_pass.cpp       strips '.' lines, populates ProgramMetadata
├── macro_pass.hpp/.cpp     '%DEFINE' + '%MACRO' expansion, built-in injection
├── label_pass.hpp/.cpp     symbol table construction, '$'/'@' address bookkeeping
└── codegen.hpp/.cpp        table lookup -> byte encoding, '@' directive writes
```

### `main.cpp` shape (target)

```cpp
auto tokenized      = LoadSourceWithBuiltins(source_path);    // lexer + __built-ins.asm injection
auto [code, meta]   = ExtractMetadata(tokenized);             // strips '.' lines
auto expanded       = ResolveMacroAndDefine(code);            // '%DEFINE' + '%MACRO' (two internal passes)
// ... label_pass, codegen, write_intel_hex — not yet implemented
```

---

## Open items (decide during implementation, not blocking)

- `.NAME` and `.CLOCK` are **mandatory** (enforced by `metadata_pass`). `.DESC` and `.DATE` are optional.
- Single shared namespace across `%DEFINE` + `%MACRO` (+ built-ins) is confirmed — populated in file order, forward references disallowed. `%DEFINE` and `%MACRO` names cannot shadow built-in mnemonics or registers.