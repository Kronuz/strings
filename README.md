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

`lower`/`upper` are a vectorizable arithmetic loop over a copy (no per-byte
`push_back`, no table lookup), `join` reserves its result and writes integers with
`std::to_chars`, and `replace` builds its result in one pass when it has several
matches of a different size. On Apple silicon (Apple clang 17, -O3), previous to now:
`lower(string_view)` 16 B 19.6 ns to 6.7 ns, 256 B 532 ns to 26 ns, 4 KB 7.4 us to 119 ns
(`absl::AsciiStrToLower` 1.2 us); `join(vector<int>)` of 1000 12.9 us to 8.5 us
(`absl::StrJoin` 9.2 us); `join(vector<string>)` 1.0-1.4x; `replace` with several matches
on 4 KB 34.7 us to 11.2 us (`absl::StrReplaceAll` 13.3 us).
Run it with `cmake -B build -DSTRINGS_BUILD_BENCH=ON && cmake --build build && ./build/strings_bench`;
`ctest` also runs a differential test against the previous implementations.

## 📚 Documentation

- [docs/API.md](docs/API.md): every helper, with examples.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the humanizers pick units.
- [AGENTS.md](AGENTS.md): repository map and traps.

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
