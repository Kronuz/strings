# Architecture

The internals of `strings`: how the surface is split between an inline/template
header and a compiled translation unit, what each dependency is there for, and
how the humanizers turn a raw number into a `2KiB` / `5s` string. For usage see
`README.md`; for the repo map and invariants see `AGENTS.md`.

## Shape

A header of inline/template utilities and a small compiled file for the
humanizers:

```
  strings.hh   the inline/template utilities: format, join, split, indent,
               left/center/right, startswith/endswith/hasupper, upper/lower,
               replace, ltrim/rtrim/trim, and the from_* declarations
  strings.cc   the compiled humanizers: from_bytes / from_time /
               from_small_time / from_delta, built on a Humanize helper
```

Everything that is generic over its argument type lives in the header so it can
inline at the call site. The byte/time formatters do real work (logarithms, a
unit table, color lookup) and don't depend on the caller's types, so they sit in
`strings.cc` behind plain declarations.

## A `std::to_string` extension

Before the `strings` namespace, the header adds a handful of `std::to_string`
overloads (for `std::string`, `std::string_view`, string-literal arrays,
`static_string`, and any type with a `.to_string()` member) and an `operator<<`
for the same `.to_string()`-having types. This is what lets `join()` call
`std::to_string(*it)` on a `std::vector` of almost anything and get a string
back. It is the reason the header pulls in `static_string.hh`: so the
`static_string` overload is available.

## format() and the silent fallback

`format()` is a thin wrapper over `std::vformat`:

```cpp
try {
    str = std::vformat(format, std::make_format_args(args...));
} catch(...) {
    // A standalone string library must not log: on a format error fall
    // back silently to the raw, unformatted format string.
    str = format;
}
```

On a bad format string you get the raw format text back. Nothing is logged and
nothing is thrown. Xapiand's version also logged the error; see `AGENTS.md`.

## join()

`join()` appends the text of each element, separated by the delimiter, with a
distinct `last_delimiter` before the final element when one is given (so a list
reads "a, b and c"; a one-element list has no separator). It indexes the vector, so
the last separator is chosen by position. The result is built with as few
allocations as possible: for `std::string` / `std::string_view` elements the total
size is summed first and reserved; a single string, or up to four strings
with a first element shorter than 16 bytes, skip that pass, because the result is small
and sizing it costs more than the growth it could save; integer elements (not `bool` or the wide
character types) are written with `std::to_chars` rather than through a temporary
`std::to_string` string; any other element goes through `std::to_string`.
The predicate overload drops the elements for which the predicate is true by
collecting pointers to the kept ones, without copying them. `vector<bool>`, whose
elements have no address, is filtered into a copy.

## Case helpers and replace

`lower` and `upper` of a `string_view` write into an exactly sized result, reading
the input once; up to seven bytes are mapped in a stack buffer and copied into the
short-string buffer once.
`inplace_lower` and `inplace_upper` edit the string. All four map only the ASCII letters
(`A-Z` and `a-z`); bytes outside them, including all bytes at or above 0x80, are
unchanged, the same mapping as `chars::tolower` / `chars::toupper`. Below 8 bytes each
byte goes through a 256-byte table, as the original did, because a vector loop has set-up
that a few bytes do not repay; the in-place loop is the original's, `std::transform` over
the string's own iterators (written on a pointer the same loop gets a vector prologue and
runs 2 to 25% slower at 4 to 6 bytes with Apple clang on x86-64). From 8 to 15 bytes the mapping works on two overlapping
64-bit words (the overlap is idempotent); from 16 bytes on it is plain arithmetic
(`c + 32` when `c - 'A' < 26`) in a loop the compiler vectorizes.

`inplace_replace` picks its path from the match count and the size relation:

- Same size: overwrite each match.  Pieces of up to 16 bytes are copied with two
  overlapping word moves instead of a library call.
- One match, or a string of at most 32 bytes: `std::string::replace` per match.
- Shorter replacement: close the gaps in one forward pass; the write cursor never
  passes the read cursor and the next match is found before its gap moves.
- Longer replacement: record the match offsets first (an array of 16 on the stack,
  then a vector). With two or three matches, call `replace` for each at its offset plus the growth so far.
  With up to 16 matches, or when the string already has room, resize once and move
  each tail up by the growth so far, last match first, so every byte moves at most
  once. With more matches and no room, build the result in one buffer of the exact size.

A search or replacement that views into the string is copied first when an edit in
place would change it or a resize would move it. An empty
search is handled first: the replacement goes before every byte and at the end (Python's
rule). `hasupper` tests ASCII `A`-`Z` with arithmetic, like the case helpers, so
it does not depend on the locale. `indent` walks `'\n'`-separated lines itself,
like Python's `textwrap.indent`: blank and whitespace-only lines are kept without
the prefix, and the trailing newline is preserved.

## The humanizers

`from_bytes`, `from_time`, `from_small_time`, and `from_delta` all funnel through
one `Humanize` functor defined in `strings.cc`. A `Humanize` is built from a base
(1024 for bytes, 1000 for sub-second time, 60 for time), a vector of scaling
exponents, a parallel vector of unit suffixes (`KiB`, `MiB`, ...; `ms`, `us`,
...), and a vector of colors one longer than the units (the extra slot is the
reset color). At construction it raises the base to each scaling exponent so the
runtime path is a divide-and-round rather than a `pow`.

At call time it takes the order of magnitude of the value (`-floor(log(delta) /
log(base))`, offset by where `0` sits in the scaling table), clamps it into the
table, divides by the matching scale, rounds, and formats `prefix + number +
unit` with `strings::format`. When `colored` is true it wraps the result in the
unit's ANSI color and the reset. `from_delta` takes nanoseconds, converts to
seconds, and dispatches to the small-time table under one second or the time
table at or above it.

The colors come from `colors.h` in the
[term-color](https://github.com/Kronuz/term-color) sibling: `rgb(...)` expands to
a compile-time `ansi_color` that converts implicitly to `std::string_view`,
which is how the color literals drop straight into the `std::vector<std::string_view>`
the `Humanize` constructor takes.

## Dependencies

Five sibling libraries, each resolving one local include:

- `chars.hh` ([char-classify](https://github.com/Kronuz/char-classify)) —
  included by `strings.hh`; the case helpers use the same ASCII mapping as its
  `chars::toupper` / `chars::tolower`.
- `split.h` ([split](https://github.com/Kronuz/split)) — `Split<>`, used by
  `split()`.
- `static_string.hh` ([static-string](https://github.com/Kronuz/static-string)) —
  the `std::to_string` overload for `static_string`, and (transitively, via
  term-color) the type the color literals are built from.
- `repr.hh` ([repr](https://github.com/Kronuz/repr)) — included by `strings.hh`,
  as in Xapiand's header.
- `colors.h` ([term-color](https://github.com/Kronuz/term-color)) — the color
  palette the humanizers use.

All five are linked `PUBLIC` because `strings.hh` is part of the public surface
and itself includes the first four, so a consumer of `strings.hh` needs them
resolvable too. `colors.h` is only used inside `strings.cc`, but is linked
`PUBLIC` for a consistent include path.

## Why this shape

The split between header and `.cc` is the usual one: generic, type-dependent
helpers inline at the call site, while the humanizers (which carry a unit table,
logs, and a color palette) compile once. Delegating coloring to `term-color`
keeps this library from reimplementing an ANSI palette, and keeps the family
consistent. `format()` never logs: a string toolkit should not decide to write
to a log, so its error path is a silent fallback.
