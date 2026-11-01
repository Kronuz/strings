# strings

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/strings/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/strings/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

String helpers for C++20: forgiving format, natural joins, ASCII case mapping, and human-readable sizes and durations.
A header of inline templates covers formatting, joining, splitting, padding,
indenting, trimming, case mapping and replacing; one compiled file turns byte
counts and durations into `2KiB` and `1.5s`. It fits the text chores of servers
and tools: log lines, reports, and CLI output.

## ✨ Highlights

- **Formatting that falls back.** `strings::format` wraps `std::vformat` and
  returns the format text unchanged when the format string and its arguments do
  not match.
- **Natural joins.** `join(words, ", ", " and ")` gives `"a, b and c"`; an
  overload drops the elements a predicate selects, and integers are written with
  `std::to_chars`.
- **Python's text rules.** `indent` follows `textwrap.indent`, and `replace` with
  an empty search inserts the replacement at every position, as `str.replace`
  does.
- **Trims return views.** `trim`, `ltrim` and `rtrim` return a `std::string_view`
  into the input.
- **Vectorized ASCII case mapping.** `lower` and `upper` map `A-Z` and `a-z` with
  arithmetic the compiler vectorizes, and leave every other byte as it is.
- **Humanizers.** `from_bytes(2048)` is `"2KiB"`; `from_time`, `from_small_time`
  and `from_delta` render durations, optionally in ANSI color.

## 🚀 Quick start

```cpp
#include "strings.hh"

#include <cstdio>
#include <string>
#include <vector>

int main() {
	std::vector<std::string> w{"a", "b", "c"};
	std::puts(strings::join(w, ", ", " and ").c_str());           // a, b and c
	std::puts(strings::format("{} + {} = {}", 2, 3, 5).c_str());   // 2 + 3 = 5
	std::puts(strings::from_bytes(2048).c_str());                 // 2KiB
	std::puts(strings::from_delta(1'500'000'000LL).c_str());      // 1.5s
	std::puts(std::string(strings::trim("  hi  ")).c_str());      // hi
}
```

[`examples/usage.cc`](examples/usage.cc) runs every helper.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it fetches five sibling
libraries (char-classify, split, static-string, repr, term-color) and compiles
one source file. CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64),
and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(strings
  GIT_REPOSITORY https://github.com/Kronuz/strings.git
  GIT_TAG        78b3c93d8d5306c7d1e94a51ee58c7e2bae3f342)  # or main for the tip
FetchContent_MakeAvailable(strings)

target_link_libraries(my_app PRIVATE strings::strings)
```

```cpp
#include "strings.hh"
```

## 🎯 When to use it

Use it for the small text chores of servers and tools, where a missing helper
would otherwise be a hand-written loop.

Not for:

- **Unicode text.** Case mapping, padding and trimming work byte by byte; only
  ASCII letters change case, and widths count bytes.
- **Format strings checked at compile time.** `format` takes the format at run
  time, and a mismatch returns the format text without an error.
- **Headers that must stay out of `std`.** `strings.hh` adds `std::to_string`
  overloads, and a global `operator<<`, for strings, `static_string`, and any
  type with a `to_string()` member.

## ⚡ Performance

Time per call, in ns (µs where marked), from `bench/bench.cc`: the best of seven samples of at least
8 ms. *strings* is this library; *abseil* is the call a program would make without it, from [abseil
20260817.0](https://github.com/abseil/abseil-cpp/tree/20260817.0): `absl::AsciiStrToLower` for
`lower`, `absl::StrJoin(v, ", ")` for `join(v, ", ")`, and `absl::StrReplaceAll` with one search and
replacement pair for `replace`. Before it times a row, the bench checks that both return the same
string for that input, and it exits 1 when they differ. `lower` runs over random printable ASCII;
`join` over strings like `item417` or random integers below 100,000; `replace` over `a` bytes with
`_` at every 4th or 16th byte, replacing `_` by `-=-` (*64 matches* is every 4th byte of 256 B). The
ratio is strings' time over abseil's, so below 1 is faster. Apple clang 17, `-O3`, on an Apple M4
Pro (macOS 26).

| Operation | Apple M4 Pro: strings | abseil | ratio |
| --- | ---: | ---: | ---: |
| `lower`, 8 B | 3.3 ns | 4.9 ns | 0.67 |
| `lower`, 16 B | 4.6 ns | 7.8 ns | 0.59 |
| `lower`, 4 KB | 128 ns | 1.35 µs | 0.10 |
| `join` of 10 strings | 60.8 ns | 48.2 ns | 1.26 |
| `join` of 1000 strings | 4.93 µs | 3.75 µs | 1.31 |
| `join` of 1000 `int`s | 8.34 µs | 8.97 µs | 0.93 |
| `replace` 1 byte by 3, 256 B, no match | 28.4 ns | 51.3 ns | 0.55 |
| `replace` 1 byte by 3, 256 B, 64 matches | 600 ns | 788 ns | 0.76 |
| `replace` 1 byte by 3, 4 KB, 1024 matches | 7.93 µs | 11.6 µs | 0.69 |

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSTRINGS_BUILD_BENCH=ON
cmake --build build --target strings_bench
./build/strings_bench
```

The bench uses abseil when CMake finds it. [docs/perf/README.md](docs/perf/README.md) has every row.

## 📚 Documentation

- [docs/API.md](docs/API.md): every helper, with examples.
- [ARCHITECTURE.md](ARCHITECTURE.md): the header and compiled split, the
  dependencies, and how the humanizers pick units.
- [CHANGELOG.md](CHANGELOG.md): what changed, with the measurements.
- [AGENTS.md](AGENTS.md): repository map and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries): the catalog.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

The tests check every helper, the case mapping and `replace` against reference
implementations, and `join`, `lower`, `upper` and `replace` against the Xapiand
original. Add
`-DSTRINGS_BUILD_BENCH=ON` to build `strings_bench`.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the
catalog's [rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and Copyright (c) 2015-2020 Dubalu
LLC. See [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
