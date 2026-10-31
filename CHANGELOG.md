# Changelog

Notable changes to `strings`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 2026-10-31

### Fixed

- `indent` walked the lines with `Split`, which skips empty pieces, so it
  dropped blank lines and the trailing newline, and turned the empty string into
  just the prefix. It now follows Python's `textwrap.indent`: lines split on
  `'\n'`, blank and whitespace-only lines are kept without the prefix, the
  trailing newline is preserved, and `indent_first = false` leaves the first line
  flush. `indent("a\n\nb", ' ', 2)` is `"  a\n\n  b"`.
- `inplace_replace` / `replace` with an empty search never terminated (the
  string grew as fast as the cursor advanced). They now insert the replacement at
  every position, including both ends, as Python's `str.replace` does.
- `hasupper` used libc `isupper`, which depends on the locale (in Latin-1 it
  treats 0xC0 to 0xDE as upper case) and is undefined for a negative `char`. It
  now tests ASCII `A`-`Z`, like `upper()` and `lower()`.

### Added

- `test/fix_test.cc`: Python `textwrap.indent` golden values, a reference
  checked exhaustively on every string up to seven characters over
  `{a, space, \n, \t, \r}`, Python `str.replace` for the empty search, and ASCII
  `A`-`Z` for `hasupper` over all 256 byte values. It keeps the previous `indent`
  frozen to pin exactly which inputs changed.

## 2026-10-30

### Changed

- `lower` / `upper` copied through a `back_inserter` one byte at a time with a
  table lookup per byte. They now copy the input once and map ASCII letters in
  place with arithmetic, which the compiler vectorizes. `inplace_lower` /
  `inplace_upper` use the same loop. Bytes outside `A-Z` / `a-z` are unchanged,
  as before.
- `join` reserves the result for string elements and writes integers with
  `std::to_chars` instead of a temporary `std::to_string`. The predicate
  overload no longer copies the kept elements.
- `replace` with several matches and a size change builds the result in one
  pass instead of shifting the tail once per match.
- Results are identical to the previous implementations.
- Speed against the previous implementations, on Apple silicon (Apple clang 17,
  -O3), from `strings_bench`:

  | Operation | Before | After | abseil |
  | --- | --- | --- | --- |
  | `lower(string_view)`, 16 B | 19.6 ns | 6.7 ns | |
  | `lower(string_view)`, 256 B | 532 ns | 26 ns | |
  | `lower(string_view)`, 4 KB | 7.4 us | 119 ns | `AsciiStrToLower` 1.2 us |
  | `join(vector<int>)`, 1000 elements | 12.9 us | 8.5 us | `StrJoin` 9.2 us |
  | `join(vector<string>)` | | 1.0x to 1.4x faster | |
  | `replace`, several matches in 4 KB | 34.7 us | 11.2 us | `StrReplaceAll` 13.3 us |

- The repr dependency is pinned to repr commit `3191c08`.

### Added

- `test/diff_test.cc`, a differential test against frozen copies of the
  previous implementations (`test/strings_old.hh`).
- `bench/bench.cc`, built with `-DSTRINGS_BUILD_BENCH=ON`.

### Fixed

- `test/diff_test.cc` includes `<cstring>`; `std::strlen` failed to build on
  libstdc++ without it.
