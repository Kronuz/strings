# strings

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/strings/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/strings/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Everyday string helpers for C++20: forgiving `std::format`, join with a final
"and", split, pad, indent, trim, case conversion, and human-readable sizes and
durations such as `2KiB` and `1.5s`.

## ✨ Highlights

- **Formatting that never throws.** `strings::format` falls back to the raw text
  on a bad format string.
- **Natural joins.** `join(words, ", ", " and ")` gives `"a, b and c"`, with an
  optional filter.
- **Split, indent, pad, trim**, with trims returning views into the input.
- **Humanizers.** `from_bytes(2048)` is `"2KiB"`; `from_time`, `from_small_time`,
  and `from_delta` render durations, optionally colored.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it fetches five sibling
libraries (char-classify, split, static-string, repr, term-color) and compiles
one source file. CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64),
and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(strings
  GIT_REPOSITORY https://github.com/Kronuz/strings.git
  GIT_TAG        main)
FetchContent_MakeAvailable(strings)

target_link_libraries(my_app PRIVATE strings::strings)
```

## 🚀 Quick start

```cpp
#include "strings.hh"

#include <cstdio>
#include <vector>

int main() {
	std::vector<std::string> w{"a", "b", "c"};
	std::puts(strings::join(w, ", ", " and ").c_str());        // a, b and c
	std::puts(strings::format("{} + {} = {}", 2, 3, 5).c_str()); // 2 + 3 = 5
	std::puts(strings::from_bytes(2048).c_str());               // 2KiB
	std::puts(std::string(strings::trim("  hi  ")).c_str());     // hi
}
```

[`examples/usage.cc`](examples/usage.cc) runs every helper.

## 🎯 When to use it

- ✅ The small text chores of servers and tools: log lines, reports, CLI output.
- ❌ Unicode-aware case conversion or width: helpers work byte by byte.

## ⚡ Performance

`lower`/`upper` of a few bytes use a table lookup per byte, from 8 to 15 bytes two
overlapping 64-bit words, and from 16 bytes plain arithmetic that the compiler
vectorizes. `join` reserves its result and writes integers with `std::to_chars`; up to
four short strings are appended without the sizing pass. `replace` finds the matches
first and then edits in place, or builds the result in one buffer of the exact size
when it grows by many matches with no room.

Apple M4 Pro (Apple clang 17, -O3), `strings_bench`, ns per call:

| Operation | strings | original | abseil |
| --- | --- | --- | --- |
| `lower(string_view)`, 8 B / 16 B / 4 KB | 3.3 / 4.5 ns / 114 ns | 8.8 / 20 ns / 7.9 us | `AsciiStrToLower` 4 KB: 1.2 us |
| `join(vector<string>)`, 10 / 1000 elements | 60 ns / 4.8 us | 78 ns / 5.0 us | |
| `join(vector<int>)`, 1000 elements | 8.6 us | 13.3 us | `StrJoin`: 9.2 us |
| `replace`, 1 byte by 3, 256 B with 64 matches | 608 ns | 925 ns | |
| `replace`, several matches in 4 KB | 7.9 us | 34.9 us | `StrReplaceAll`: 13.3 us |

The abseil figures come from the benchmark built with abseil installed (`HAVE_ABSL`) on Apple
silicon. The same rows on an Intel i9-9980HK (Apple clang 17): `lower` 8 B 11 ns, 4 KB 262 ns;
`join(vector<int>)` 1000 19.3 us; `replace` 256 B with 64 matches 1.3 us, 4 KB 14.6 us.

Run the benchmark with:

```sh
cmake -B build -DSTRINGS_BUILD_BENCH=ON && cmake --build build && ./build/strings_bench
```

## 📚 Documentation

- [docs/API.md](docs/API.md): every helper, with examples.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the humanizers pick units.
- [AGENTS.md](AGENTS.md): repository map and traps.
- [CHANGELOG.md](CHANGELOG.md): behavior and performance changes.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
./build/strings_usage
```

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) first. CI must
pass on every platform.

## 📄 License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE). Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).
