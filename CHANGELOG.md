# Changelog

Notable changes to `strings`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 2026-10-31, after the audit

### Fixed

The 2026-10-30 changes were measured on 4 KB inputs, 1000 elements and many matches. An
audit on an Intel i9 laptop (Apple clang 17, -O3) found `replace`, `join` and `lower` slower
than the originals on small inputs, rows the benchmark did not have and the changelog did
not list: `replace` of one byte by three in 256 B with 64 matches ran at 0.84 to 0.86 of the
original, a single match in 256 B at 0.92 to 0.93, `join` of 1 to 3 strings at 0.78 to 0.84,
and `lower(string_view)` of 1 B at 0.71 to 0.72. The same code also lost on rows nobody had
measured (`inplace_lower` and `inplace_upper` of 1 to 4 B, `join` of four strings,
`replace` of a 16-byte string). Every one is fixed, and `bench/bench.cc` now has all of
them (68 rows).

- **`replace`** finds the matches before it edits and picks its path from their number: one
  `std::string::replace` for one match or a string of at most 32 B; one forward pass that
  closes the gaps when the replacement is shorter; for a longer replacement, `replace` calls
  at the shifted offsets for two or three matches, one resize and one move per tail for up
  to 16 matches or when the string has room, and one buffer of the exact size for more. The
  previous build reserved room for two matches and reallocated as it grew, searched once
  more than needed, and then repeated that search. Pieces of up to 16 bytes move with two
  overlapping word moves instead of a library call.
- **`join(vector<string>)`** appends a single string, or up to four strings when the first
  is shorter than 16 bytes, without the sizing pass and `reserve`, which cost more than
  they saved there. Longer joins are unchanged.
- **`lower` / `upper`**: up to seven bytes of a `string_view` are pushed one by one, below
  eight bytes a table lookup per byte as the original did, from 8 to 15 bytes two
  overlapping 64-bit words, from 16 bytes the vectorized loop.
- A search or replacement that views into the string being edited could change while the
  string was edited in place, so the result depended on the order of the edits. It is copied
  first when an in-place edit or a resize would affect it.

Speed against the previous implementations (old / new = speedup), `strings_bench` built by CI
with -O3 and run on each machine in an exclusive hold, best of three runs. Times are ns per call
(microseconds where marked). The old column is `test/strings_old.hh`, the original code:

| Row (ns per call) | M4 Pro | M1 Pro | i9 clang | i9 g++ 16 |
| --- | --- | --- | --- | --- |
| `lower(string_view) 1 B` | 2.3 / 2.3 = 1.00x | 2.9 / 2.9 = 1.00x | 7.2 / 7.1 = 1.01x | 10.5 / 6.5 = 1.62x |
| `lower(string_view) 8 B` | 8.8 / 3.3 = 2.67x | 21.3 / 4.9 = 4.35x | 21.8 / 11.0 = 1.98x | 55.2 / 12.1 = 4.56x |
| `lower(string_view) 16 B` | 20.4 / 4.5 = 4.53x | 42.5 / 7.0 = 6.07x | 37.8 / 12.8 = 2.95x | 159.9 / 52.8 = 3.03x |
| `lower(string_view) 4096 B` | 7.88 / 0.11 us = 69.08x | 9.23 / 0.15 us = 60.69x | 8.28 / 0.26 us = 31.64x | 26.73 / 0.29 us = 91.55x |
| `inplace_lower 1 B` | 2.8 / 3.0 = 0.93x | 4.3 / 4.2 = 1.02x | 7.0 / 6.4 = 1.09x | 9.4 / 4.9 = 1.92x |
| `inplace_lower 4 B` | 4.6 / 5.2 = 0.88x | 5.3 / 6.7 = 0.79x | 8.0 / 8.2 = 0.98x | 28.3 / 5.7 = 4.96x |
| `inplace_lower 16 B` | 7.9 / 7.0 = 1.13x | 9.7 / 8.3 = 1.17x | 15.4 / 12.1 = 1.27x | 110.9 / 7.3 = 15.19x |
| `join(vector<string>) 1` | 5.0 / 4.5 = 1.11x | 8.9 / 7.4 = 1.20x | 13.5 / 13.3 = 1.02x | 13.0 / 13.2 = 0.98x |
| `join(vector<string>) 2` | 9.3 / 9.9 = 0.94x | 14.5 / 15.1 = 0.96x | 21.8 / 23.1 = 0.94x | 75.0 / 72.5 = 1.03x |
| `join(vector<string>) 3` | 27.9 / 28.4 = 0.98x | 45.7 / 46.4 = 0.98x | 73.2 / 74.6 = 0.98x | 84.5 / 76.3 = 1.11x |
| `join(vector<string>) 4` | 31.6 / 36.0 = 0.88x | 51.9 / 53.9 = 0.96x | 79.1 / 84.0 = 0.94x | 135.7 / 134.1 = 1.01x |
| `join(vector<string>) 10` | 77.9 / 59.6 = 1.31x | 116.3 / 89.9 = 1.29x | 175.6 / 138.2 = 1.27x | 220.1 / 120.7 = 1.82x |
| `join(vector<string>) 1000` | 5.02 / 4.76 us = 1.06x | 7.15 / 7.18 us = 1.00x | 11.05 / 10.19 us = 1.08x | 9.53 / 8.34 us = 1.14x |
| `join(vector<string>) 2 x 60 B` | 39.1 / 25.1 = 1.56x | 63.7 / 41.1 = 1.55x | 109.0 / 61.8 = 1.76x | 147.3 / 58.6 = 2.51x |
| `join(vector<int>) 1000` | 13.30 / 8.60 us = 1.55x | 18.11 / 11.66 us = 1.55x | 28.50 / 19.34 us = 1.47x | 18.74 / 18.77 us = 1.00x |
| `replace 1->3, 16 B, 2 matches` | 28.2 / 28.9 = 0.98x | 46.5 / 47.4 = 0.98x | 60.7 / 64.4 = 0.94x | 146.4 / 153.8 = 0.95x |
| `replace 1->3, 256 B, 1 match` | 39.3 / 39.6 = 0.99x | 64.1 / 63.4 = 1.01x | 147.9 / 144.1 = 1.03x | 273.8 / 271.5 = 1.01x |
| `replace 1->3, 256 B, 3 matches` | 65.2 / 67.0 = 0.97x | 104.2 / 104.5 = 1.00x | 197.2 / 212.0 = 0.93x | 321.8 / 339.4 = 0.95x |
| `replace 1->3, 256 B, 64 matches` | 925.3 / 607.8 = 1.52x | 1.22 / 0.80 us = 1.53x | 1.81 / 1.29 us = 1.40x | 1.63 / 1.15 us = 1.42x |
| `replace 1->3, 4 KB, 1024 matches` | 34.90 / 7.86 us = 4.44x | 45.33 / 11.01 us = 4.12x | 40.58 / 14.57 us = 2.78x | 38.06 / 11.31 us = 3.37x |
| `replace 1->1, 256 B, 64 matches` | 481.5 / 409.5 = 1.18x | 602.9 / 581.9 = 1.04x | 1.12 / 0.86 us = 1.31x | 952.0 / 876.0 = 1.09x |
| `replace 3->1, 256 B, 64 matches` | 750.9 / 494.6 = 1.52x | 1.01 / 0.64 us = 1.57x | 1.49 / 0.92 us = 1.62x | 1.28 / 0.91 us = 1.41x |
| `replace 2->5, 256 B, 32 matches` | 460.1 / 333.2 = 1.38x | 674.1 / 427.3 = 1.58x | 1.06 / 0.80 us = 1.33x | 1.01 / 0.68 us = 1.48x |
| `inplace_replace 1->3, 256 B, 64 matches` | 753.5 / 562.8 = 1.34x | 1.02 / 0.76 us = 1.34x | 1.39 / 1.09 us = 1.27x | 1.07 / 0.99 us = 1.09x |

All 68 rows are in `bench/bench.cc`. The rows below 0.97 are within a few percent of the
original and are not fixed: `join` of two to four strings is 0.94 to 0.96 on every machine
(0.88 at four strings on the M4), `inplace_lower` / `inplace_upper` of 1 to 4 bytes are 0.79 to
0.98 (the M1 is the lowest), and `replace` of 16 bytes with two matches and of 256 bytes with
three matches is 0.93 to 0.98 on the i9. Run-to-run spread of the original on the same row
is 1 to 15%, up to 23% on the i9 for rows with no change in their path.


### Added

- `bench/bench.cc` rows: `lower`, `upper` and their in-place forms from 1 B to 4 KB, `join`
  of 1, 2, 3, 4, 10 and 1000 strings and integers (and with a predicate), and `replace` with
  0, 1, 2, 3, 16, 32, 64 and 1024 matches shrinking, keeping the size and growing.
- `test/paths_test.cc` (CTest `strings_paths`): `lower` and `upper` against the per-byte
  definition for every byte at every position of every length to 40; `replace` against a
  naive scan of Python's rule, exhaustively over small strings (with and without a prefix
  that pushes them past 32 B), for 0 to 300 matches, for piece sizes to 20 and for random
  long strings; a search or replacement viewing into the string; what `replace` does to the
  buffer (a longer result with more than 16 matches has capacity within one allocation
  granule of its size; shrinking, keeping the size, and a string with room keep the data
  pointer); and `join` against its definition for 0 to 6 strings. The path test fails on the
  header before this change.

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
