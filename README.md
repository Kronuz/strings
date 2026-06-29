# strings

A small C++20 toolkit of general string utilities: `std::format`-style
formatting, list joining, splitting, padding, trimming, case conversion,
prefix/suffix tests, and human-readable byte/time formatters. Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One namespace, `strings`, with the everyday helpers you reach for when working
with text. The header holds the inline and template utilities:

- `format(fmt, args...)` — a thin wrapper over `std::vformat`. On a bad format
  string it falls back silently to the raw, unformatted string instead of
  throwing.
- `join(values, delim[, last_delim][, pred])` — join a `std::vector` into a
  string, optionally with a distinct final delimiter ("a, b and c") and an
  optional predicate that drops elements before joining.
- `split(value, sep)` — split a string into a `std::vector` of pieces.
- `indent(str, fill, level[, indent_first])` — prefix each line with `level`
  copies of a fill character.
- `left` / `center` / `right` — width-based padding.
- `startswith` / `endswith` (string or `char`), `hasupper` — predicates.
- `upper` / `lower` / `inplace_upper` / `inplace_lower` — case conversion.
- `replace` / `inplace_replace` — substitute every occurrence.
- `ltrim` / `rtrim` / `trim` — strip leading/trailing whitespace (returns a
  `std::string_view` into the input).

The compiled side (`strings.cc`) adds the humanizers: `from_bytes`,
`from_time`, `from_small_time`, and `from_delta`, which render a number with a
human-readable unit (`2KiB`, `5s`, `2ms`, ...) and an optional ANSI color.

## Install

This is not header-only. It ships a compiled translation unit, `strings.cc`,
plus the header `strings.hh`. You build and link the `.cc`; the header holds the
inline/template utilities. It requires C++20 (it uses `std::format`).

It has five sibling dependencies, all part of the same family and all pulled in
for you by CMake:

- [char-classify](https://github.com/Kronuz/char-classify) — `chars::toupper` /
  `chars::tolower` for the case helpers.
- [split](https://github.com/Kronuz/split) — `Split<>`, used by `split()` and
  `indent()`.
- [static-string](https://github.com/Kronuz/static-string) — the `to_string()`
  overload for `static_string`.
- [repr](https://github.com/Kronuz/repr) — kept from the original header.
- [term-color](https://github.com/Kronuz/term-color) — the `rgb()` / `CLEAR_COLOR`
  palette the humanizers color with.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  strings
  GIT_REPOSITORY https://github.com/Kronuz/strings.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(strings)

target_link_libraries(your_target PRIVATE strings::strings)
```

`CMakeLists.txt` requests `cxx_std_20` `PUBLIC` and links the five deps `PUBLIC`,
so every header `strings.hh` includes resolves on the include path with no extra
wiring on your side. Then:

```cpp
#include "strings.hh"
```

The header keeps its original filename, so a codebase that already
`#include "strings.hh"` just needs this repo on its include path.

## Usage

```cpp
#include "strings.hh"

// std::format-style formatting; bad format strings fall back to the raw text.
strings::format("{} + {} = {}", 2, 3, 5);   // "2 + 3 = 5"

// Join with a delimiter, or a distinct last delimiter.
std::vector<std::string> w{"a", "b", "c"};
strings::join(w, ", ");                       // "a, b, c"
strings::join(w, ", ", " and ");              // "a, b and c"

// Split, then case / trim / predicates.
auto parts = strings::split(std::string("a,b,c"), ',');  // {"a","b","c"}
strings::upper(std::string_view("MixEd"));    // "MIXED"
strings::trim("  hi  ");                       // "hi"
strings::startswith("hello", "he");            // true
strings::endswith("hello", 'o');               // true

// Padding and indentation.
strings::right("hi", 5);                       // "   hi"
strings::indent("a\nb", ' ', 2);               // "  a\n  b"

// Humanizers (optionally colored).
strings::from_bytes(2048);                     // "2KiB"
strings::from_time(5);                          // "5s"
strings::from_delta(std::chrono::nanoseconds(1500000000));
```

## Build & test

```sh
cmake -B build -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
cmake --build build
ctest --test-dir build
```

The first configure fetches the five sibling libraries over the network. The
test exercises `format` (substitution, padding, and the silent fallback),
`join` (delimiter / last-delimiter / numeric / predicate forms), `split`,
`indent`, the padders, the prefix/suffix/case predicates, the case helpers
(value, rvalue, in-place), `replace`, the trim trio, and the `from_bytes` /
`from_time` / `from_small_time` / `from_delta` humanizers. It prints
`all strings tests passed` and exits 0.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `strings.hh` /
`strings.cc` were copied with one decoupling change: the original header
depended on Xapiand's logging (`#include "log.h"` and an `L_EXC(...)` call in the
`format` error path). A standalone string library must not log, so both were
removed; the `format` catch now keeps only the silent fallback (return the raw,
unformatted string). The five local includes were wired to their standalone
sibling libraries through CMake. See [ARCHITECTURE.md](ARCHITECTURE.md) for the
design and [AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE).
