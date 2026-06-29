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

In Xapiand the `catch` logged through `L_EXC("Cannot format {}", repr(format))`
before falling back. That logging call is the one piece of coupling that was
removed on extraction (see `AGENTS.md`). The behavior on a bad format string is
otherwise unchanged: you get the raw format text back, not an exception.

## join()

`join()` walks the vector once, appending `std::to_string(value)` of each
element separated by the delimiter, and uses a distinct `last_delimiter` before
the final element when one is given (so a list reads "a, b and c"). It computes
the position of the last element up front with a reverse iterator so the final
separator is chosen correctly even for a one-element list (where no separator is
emitted at all). The predicate overload first `remove_copy_if`s the filtered
elements into a fresh vector, then joins that.

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
  `chars::toupper` / `chars::tolower`, the transforms behind the case helpers.
- `split.h` ([split](https://github.com/Kronuz/split)) — `Split<>`, used by
  `split()` and by `indent()` to walk lines.
- `static_string.hh` ([static-string](https://github.com/Kronuz/static-string)) —
  the `std::to_string` overload for `static_string`, and (transitively, via
  term-color) the type the color literals are built from.
- `repr.hh` ([repr](https://github.com/Kronuz/repr)) — included in the original
  header; kept on extraction.
- `colors.h` ([term-color](https://github.com/Kronuz/term-color)) — the color
  palette the humanizers use.

All five are linked `PUBLIC` because `strings.hh` is part of the public surface
and itself includes the first four, so a consumer of `strings.hh` needs them
resolvable too. `colors.h` is only used inside `strings.cc`, but is linked
`PUBLIC` for a consistent include path.

## Why this shape

The split between header and `.cc` is the usual one: generic, type-dependent
helpers inline at the call site, while the humanizers (which carry a unit table,
logs, and a color palette) compile once. Delegating case conversion to
`char-classify` and coloring to `term-color` keeps this library from
reimplementing a `tolower` table or an ANSI palette, and keeps the family
consistent. The one deliberate departure from the original is the removed
logging in `format()`: a string toolkit should never decide to write to a log,
so the error path was reduced to its silent fallback.
