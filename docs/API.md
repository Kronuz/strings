# strings API reference

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

## What is in it

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
