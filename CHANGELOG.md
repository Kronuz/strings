# Changelog

Notable changes to `strings`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 2026-11-01, small inputs again

### Fixed

The audit fix left rows between 0.79 and 0.98 of the original: `inplace_lower` and
`inplace_upper` of 1 to 4 B, `join` of two to four strings, `replace` of 16 B and of 256 B
with a few matches. This pass gives them the original's shape and keeps the large-input
gains.

- **`inplace_lower` / `inplace_upper`** below 8 bytes run the original's loop, `std::transform`
  over the string's own iterators. The same loop written on a pointer (what the previous build
  did) gets a vector prologue that a few bytes do not repay: up to 19% slower at 4 to 6 bytes with
  Apple clang on x86-64, 0.79 to 0.98 at 1 to 4 bytes on the M1 and M4. From 8 bytes on nothing
  changes.
- **`join(vector<string>)`** of one to four short strings appends a `std::string` as a
  `std::string`. Through a `std::string_view` the same append compiled 3 to 11% slower with
  Apple clang on x86-64.
- **`lower` / `upper`** of a `string_view` below 8 bytes default-construct the result and push
  the mapped bytes, as the original did, instead of constructing it with a size first.
- Two structures were measured and not kept. Keeping `join`'s short path inline and moving the
  sized path into a separate function made 10 to 1000 element joins 0.63 to 0.88 of the original
  on every machine, although the sized loop compiled to the same instructions. Keeping no match,
  one match and short strings of `inplace_replace` in the caller and the rest in a separate
  function moved the `g++ 16` rows with one match up (4 KB: 0.88 to 0.96) and the two- and
  three-match rows down on the M4 and the i9 (256 B with 3 matches on the M4: 0.99 to 0.90).

### Changed

- `bench/bench.cc` measures the original twice per row, around the new code, as two copies of
  the same lambda: `aa` is old / old2, the noise of the row (layout and inlining included), and
  `x` compares the new code with the better of the two.

### Added

- `tools/mutants.py` breaks `strings.hh` in 25 single-line ways and requires a test to fail on
  each; all 25 die.

Speed against the original (old / new = speedup), `strings_bench` built by CI with -O3, three
runs on each machine with the machine to itself, best of three against the better of the two
original columns. Times are ns per call (microseconds where marked):

| Row (ns per call) | M4 Pro | i9 clang | i9 g++ 16 |
| --- | --- | --- | --- |
| `lower(string_view) 1 B` | 2.2 / 2.3 = 0.96x | 6.5 / 6.6 = 0.98x | 9.8 / 5.5 = 1.78x |
| `lower(string_view) 4 B` | 4.6 / 4.7 = 0.98x | 10.0 / 10.3 = 0.97x | 27.9 / 10.3 = 2.71x |
| `lower(string_view) 8 B` | 8.9 / 3.3 = 2.70x | 16.1 / 9.9 = 1.63x | 52.7 / 19.6 = 2.69x |
| `lower(string_view) 16 B` | 20.5 / 4.4 = 4.66x | 31.9 / 11.4 = 2.80x | 144.0 / 53.6 = 2.69x |
| `lower(string_view) 4096 B` | 7.74 / 0.12 us = 63.46x | 7.26 / 0.24 us = 29.90x | 26.79 / 0.28 us = 96.38x |
| `inplace_lower 1 B` | 2.6 / 2.0 = 1.30x | 6.5 / 6.0 = 1.08x | 8.6 / 4.6 = 1.87x |
| `inplace_lower 4 B` | 4.4 / 3.5 = 1.26x | 7.6 / 6.6 = 1.15x | 25.2 / 5.3 = 4.75x |
| `inplace_lower 16 B` | 8.0 / 2.7 = 2.96x | 14.0 / 10.2 = 1.37x | 96.6 / 6.2 = 15.58x |
| `join(vector<string>) 1` | 5.0 / 4.4 = 1.14x | 12.5 / 12.2 = 1.02x | 7.5 / 6.8 = 1.10x |
| `join(vector<string>) 2` | 9.0 / 9.6 = 0.94x | 20.2 / 21.2 = 0.95x | 57.0 / 57.6 = 0.99x |
| `join(vector<string>) 3` | 27.3 / 27.4 = 1.00x | 67.2 / 69.4 = 0.97x | 60.9 / 62.2 = 0.98x |
| `join(vector<string>) 4` | 31.0 / 32.9 = 0.94x | 73.8 / 77.1 = 0.96x | 109.7 / 112.8 = 0.97x |
| `join(vector<string>) 10` | 75.6 / 57.1 = 1.32x | 160.8 / 127.3 = 1.26x | 194.3 / 121.5 = 1.60x |
| `join(vector<string>) 1000` | 4.94 / 4.76 us = 1.04x | 9.96 / 8.87 us = 1.12x | 9.12 / 8.69 us = 1.05x |
| `join(vector<string>) 2 x 60 B` | 38.9 / 24.2 = 1.61x | 97.6 / 59.3 = 1.65x | 137.0 / 55.1 = 2.49x |
| `join(vector<int>) 1000` | 13.00 / 8.53 us = 1.52x | 26.33 / 18.28 us = 1.44x | 19.57 / 18.04 us = 1.08x |
| `replace 1->3, 16 B, 2 matches` | 27.5 / 28.6 = 0.96x | 57.4 / 60.1 = 0.96x | 128.4 / 141.0 = 0.91x |
| `replace 1->3, 256 B, 1 match` | 37.8 / 39.1 = 0.97x | 132.8 / 134.0 = 0.99x | 251.1 / 267.4 = 0.94x |
| `replace 1->3, 256 B, 3 matches` | 62.7 / 63.4 = 0.99x | 180.4 / 189.3 = 0.95x | 299.2 / 319.2 = 0.94x |
| `replace 1->3, 256 B, 64 matches` | 916.4 / 597.2 = 1.53x | 1.68 / 1.25 us = 1.35x | 1.51 / 1.29 us = 1.17x |
| `replace 1->3, 4 KB, 1024 matches` | 34.38 / 7.95 us = 4.33x | 38.10 / 13.94 us = 2.73x | 35.62 / 13.91 us = 2.56x |
| `replace 1->1, 256 B, 64 matches` | 482.3 / 413.5 = 1.17x | 1.05 / 0.79 us = 1.34x | 876.8 / 798.9 = 1.10x |
| `replace 3->1, 256 B, 64 matches` | 739.6 / 491.9 = 1.50x | 1.37 / 0.84 us = 1.63x | 1.12 / 0.93 us = 1.21x |
| `replace 2->5, 256 B, 32 matches` | 464.8 / 304.3 = 1.53x | 1.03 / 0.76 us = 1.36x | 925.1 / 823.0 = 1.12x |
| `inplace_replace 1->3, 256 B, 64 matches` | 799.2 / 556.2 = 1.44x | 1.27 / 1.01 us = 1.26x | 1.07 / 1.00 us = 1.07x |

All 68 rows are in `bench/bench.cc`. Rows below 0.97 (best of three; the A/A of the same code
compiled twice, where it is given, is in parentheses):

- M4 Pro, 7 rows: `lower(string_view)` 1 B 0.96 (A/A 1.05), `upper(string_view)` 2 B 0.93 (0.96),
  `join` of 2 and 4 strings 0.94 (0.99, 1.00), `replace` of 16 B with 2 matches 0.96 (1.00),
  256 B with one match 0.97 (0.96) and with no match 0.97 (1.01).
- i9, Apple clang, 10 rows: `upper(string_view)` 2 B 0.95 and 4 B 0.92, `join` of 2, 3, 4
  strings 0.95, 0.97, 0.96, `replace` of 16 B with 2 matches 0.96, of 32 B with 4 matches 0.97,
  of 256 B with 3 matches 0.95 and 2 matches 0.96, of 4 KB with one match 0.96 (A/A 0.97 to 1.04).
- i9, g++ 16, 8 rows, all `replace`: 16 B with 2 matches 0.91, 32 B with 4 matches 0.88,
  256 B with 1, 2, 3 matches 0.94, 0.88, 0.94, with no match 0.95, 4 KB with one match 0.88, and
  3->1 with 2 matches 0.95.

The `lower` / `upper` rows run the original's own loop (a default-constructed string and a
`push_back` per byte) and the `join` rows its appends behind a test of the count and of the
first element's size; they differ from it by 0.1 to 3 ns, and the same code compiled twice
differs by up to 5% on them (`aa`). The `replace` rows differ in a way a literal search shows
and a search known only at run time may not: the original's few lines are inlined into the call
site, where the compiler can specialize on a literal one-byte search, and the new function is one
out-of-line copy of eight paths. The inline-front experiment above moved the `g++ 16` rows with
one match from 0.88 to 0.96, which points the same way. The rows left under 0.97 are those where
the difference is that, or 0.5 to 2 ns. The M1 was unreachable and is not in this table; its
column in the entry below is from before this pass.

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
with -O3 and run on each machine while nothing else ran, best of three runs. Times are ns per call
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
