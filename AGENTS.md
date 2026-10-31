# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
strings.hh                   Inline/template utilities: format, join, split, indent, left/center/right, startswith/endswith/hasupper, upper/lower, replace, ltrim/rtrim/trim, and the from_* declarations. Plus a std::to_string extension.
strings.cc                   The compiled humanizers: from_bytes / from_time / from_small_time / from_delta, on a Humanize helper. Includes "colors.h". Same as Xapiand's.
test/fix_test.cc             indent / hasupper / empty-search replace against Python textwrap.indent and ASCII oracles.
test/diff_test.cc            Differential test of join / lower / upper / replace against test/strings_old.hh.
test/strings_old.hh          Frozen Xapiand join / upper / lower / replace, the differential test's oracle. Do not edit.
bench/bench.cc               Benchmark (STRINGS_BUILD_BENCH=ON) against strings_old.hh and, when installed, abseil.
test/test.cc                 Runnable smoke test: format (incl. silent fallback), join, split, indent, padding, predicates, case, replace, trim, humanizers.
CMakeLists.txt               STATIC library `strings` (+ alias strings::strings); FetchContent of five sibling deps; CTest test `strings`.
LICENSE                      MIT, Copyright (c) 2015-2019 Dubalu LLC.
README.md                    What it is, install, quick start.
docs/API.md                  Usage and the full list of helpers.
ARCHITECTURE.md              Internal design: header/.cc split, format fallback, join, the humanizers.
CHANGELOG.md                 Behavior and performance changes, newest first.
```

This is not header-only. `strings.cc` must be compiled and linked; the header
holds the inline/template utilities. The CMake target is a `STATIC` library that
links the five sibling deps so their headers resolve.

## Build and run the test

```sh
cmake -B build -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
cmake --build build
ctest --test-dir build
```

The first configure fetches the five sibling libraries over the network
(FetchContent, at the commits `CMakeLists.txt` pins). Expected output ends with
`all strings tests passed`, exit 0. The test target is `strings_test`; the
registered CTest name is `strings`. The test is only added when this repo is the
top-level project (CMakeLists.txt), so consumers vendoring it via `FetchContent`
won't build it.

Building with the Homebrew LLVM clang++ is the family convention (Apple clang is
unreliable for some of the C++20 / sanitizer paths on recent macOS). A plain
`cmake -B build` works too if the default compiler is new enough for
`std::format`.

## Dependencies

Five sibling libraries, each resolving one local include, all linked `PUBLIC`:

| Include              | Library                                                      | CMake target to link              | What for                                  |
| -------------------- | ----------------------------------------------------------- | --------------------------------- | ----------------------------------------- |
| `"chars.hh"`         | [char-classify](https://github.com/Kronuz/char-classify)    | `char_classify::char_classify`    | included by `strings.hh`                  |
| `"split.h"`          | [split](https://github.com/Kronuz/split)                    | `split` (no namespaced alias)     | `Split<>` for split()                     |
| `"static_string.hh"` | [static-string](https://github.com/Kronuz/static-string)    | `static_string::static_string`    | `to_string` overload; color literal type  |
| `"repr.hh"`          | [repr](https://github.com/Kronuz/repr)                      | `repr::repr`                      | included by `strings.hh`, as in Xapiand   |
| `"colors.h"`         | [term-color](https://github.com/Kronuz/term-color)          | `term_color::term_color`          | the humanizer color palette               |

Note the repo dir names use hyphens (`char-classify`, `static-string`,
`term-color`) while the CMake target/alias names use underscores. `split` is the
odd one out: it exposes only the bare `split` target, with no `split::split`
alias, so link the bare name. `CMakeLists.txt` pins each dep to a commit.

## No logging

Xapiand's `strings.hh` includes `"log.h"` and logs
`L_EXC("Cannot format {}", repr(format))` in the `format()` error path. This
library does neither: a standalone string library must not log, so the
`format()` catch keeps only the silent fallback (`str = format;`). The result on
a bad format string is the same raw text. There are no `L_*` / `log.h` / `opts`
/ `config` references; keep it that way.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC` (the code uses
  `std::format`). Don't drop the target below it.
- **Filenames are stable.** The header and source keep their original Xapiand
  names (`strings.hh`, `strings.cc`) so a consumer that already `#include`s the
  header just needs this repo on the include path. Don't rename them.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on
  source files.

## Load-bearing invariants

- **No logging, ever.** The library has no logging dependency. Don't reintroduce `log.h`, `L_*` macros, or any other "report an
  error somewhere" behavior. The `format()` error path stays a silent fallback to
  the raw format string.
- **`format()` swallows format errors.** A format string that doesn't match its
  arguments must not throw out of `format()`; it returns the raw, unformatted
  string. The smoke test pins this. Don't "fix" it into a throw.
- **The `std::to_string` extension is what makes `join()` generic.** The
  overloads added to `namespace std` (for `std::string`, `string_view`, literal
  arrays, `static_string`, and any `.to_string()`-having type) are why `join()`
  works over a vector of arbitrary elements. Removing one narrows `join`.
- **Humanizer color literals rely on the implicit `string_view` conversion.** In
  `strings.cc` the `rgb(...)` / `CLEAR_COLOR` values from term-color are stored in
  a `std::vector<std::string_view>`; they work because `ansi_color` converts
  implicitly to `std::string_view`. Don't change those vectors to hold a concrete
  color type.

## How to extend

- **Add a string helper.** Most utilities are inline/template in `strings.hh`;
  add yours there and give it a case in `test/test.cc`. Keep it free of any I/O
  or logging.
- **Add a humanizer unit/tier.** Edit the `Humanize` tables in `strings.cc`
  (scaling exponents, units, and the colors vector, which must stay exactly one
  longer than units). The constructor asserts these sizes line up.
- **Always extend the smoke test.** `test/test.cc` asserts exact results for
  every helper; the other tests cover specific oracles. Add a case for any new helper or tier and assert the exact result.

## Traps

- **`split` has no namespaced alias.** Link the bare `split` target, not
  `split::split` (which doesn't exist). The other four use the `name::name` form.
- **Trim helpers return a `std::string_view` into the input.** `ltrim` / `rtrim`
  / `trim` don't allocate; they return a view over the caller's storage. Don't
  let that view outlive the string it points into.
- **The humanizer color vector is one longer than the units.** The extra slot is
  the reset color appended at the end. The constructor asserts
  `colors.size() == units.size() + 1`; off-by-one here trips the assert.
- **Upstream `split.h` warns under recent libc++.** It derives an iterator from
  the deprecated `std::iterator`. That warning comes from the dependency, not
  this code; don't try to silence it here.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `strings.cc` matches upstream.
`strings.hh` differs in three ways: no logging (above), faster `join`, case
helpers and `replace` with the same results, and Python-compatible `indent`,
`hasupper` and empty-search `replace` (see `CHANGELOG.md`). The five local
includes resolve against the standalone sibling libraries through CMake. Keep
changes reconcilable with upstream.
