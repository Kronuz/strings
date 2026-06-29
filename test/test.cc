// Smoke test for the standalone strings library.
//
// Exercises the general string-utilities surface: format() (and its silent
// fallback on a bad format string), join() in its delimiter / last-delimiter /
// predicate forms, split(), indent(), the left/center/right padders,
// startswith / endswith / hasupper, the upper/lower case helpers (value,
// rvalue, and in-place), replace() (value and in-place), the ltrim/rtrim/trim
// trio, and the from_bytes / from_time / from_delta humanizers from strings.cc.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "strings.hh"


// ---------------------------------------------------------------------------
// format(): std::format-style, with a silent fallback to the raw format string
// when the arguments don't match (the decoupled catch path, no logging).
// ---------------------------------------------------------------------------

static void test_format() {
	assert(strings::format("{} + {} = {}", 2, 3, 5) == "2 + 3 = 5");
	assert(strings::format("no args here") == "no args here");
	assert(strings::format("{:>4}", 7) == "   7");

	// A format string that references an argument index that isn't there throws
	// inside std::vformat; the library swallows it and returns the raw string.
	assert(strings::format("{} {} {}", 1) == "{} {} {}");

	std::printf("format OK: substitution, padding, silent fallback\n");
}


// ---------------------------------------------------------------------------
// join(): plain delimiter, distinct last delimiter, and the predicate form
// that drops filtered elements before joining.
// ---------------------------------------------------------------------------

static void test_join() {
	std::vector<std::string> words{"a", "b", "c"};
	assert(strings::join(words, ", ") == "a, b, c");

	// A distinct last delimiter ("a, b and c").
	assert(strings::join(words, ", ", " and ") == "a, b and c");

	// Single element: no delimiter is ever inserted.
	std::vector<std::string> one{"solo"};
	assert(strings::join(one, ", ") == "solo");

	// Empty vector joins to the empty string.
	std::vector<std::string> none{};
	assert(strings::join(none, ", ").empty());

	// Numeric values go through std::to_string.
	std::vector<int> nums{1, 2, 3};
	assert(strings::join(nums, "-") == "1-2-3");

	// Predicate form: drop empty strings before joining.
	std::vector<std::string> sparse{"x", "", "y", ""};
	auto is_empty = [](const std::string& s) { return s.empty(); };
	assert(strings::join(sparse, ",", is_empty) == "x,y");

	std::printf("join OK: delimiter, last delimiter, numeric, predicate filter\n");
}


// ---------------------------------------------------------------------------
// split(): splits a string on a separator into a vector of pieces.
// ---------------------------------------------------------------------------

static void test_split() {
	auto parts = strings::split(std::string("a,b,c"), ',');
	assert(parts.size() == 3);
	assert(parts[0] == "a" && parts[1] == "b" && parts[2] == "c");

	// A round-trip: split then join with the same separator reproduces the input.
	assert(strings::join(parts, ",") == "a,b,c");

	std::printf("split OK: separator split, join round-trip\n");
}


// ---------------------------------------------------------------------------
// indent(): prefixes each line with `level` copies of a fill char.
// ---------------------------------------------------------------------------

static void test_indent() {
	// Two lines, indented two spaces; first line indented by default.
	assert(strings::indent("a\nb", ' ', 2) == "  a\n  b");

	// indent_first = false leaves the first line flush.
	assert(strings::indent("a\nb", ' ', 2, false) == "a\n  b");

	std::printf("indent OK: per-line fill, indent_first toggle\n");
}


// ---------------------------------------------------------------------------
// left / center / right: width-based padding (fill controls trailing space).
// ---------------------------------------------------------------------------

static void test_padding() {
	// left without fill is just the string; with fill it is padded on the right.
	assert(strings::left("hi", 5) == "hi");
	assert(strings::left("hi", 5, true) == "hi   ");

	// right pads on the left to the requested width.
	assert(strings::right("hi", 5) == "   hi");

	// center places the text in the middle; with fill the right side is padded.
	assert(strings::center("hi", 6, true) == "  hi  ");

	std::printf("padding OK: left, right, center\n");
}


// ---------------------------------------------------------------------------
// startswith / endswith / hasupper: prefix, suffix, and case predicates.
// ---------------------------------------------------------------------------

static void test_predicates() {
	assert(strings::startswith("hello world", "hello"));
	assert(strings::startswith("hello world", 'h'));
	assert(!strings::startswith("hello", "hello world"));

	assert(strings::endswith("hello world", "world"));
	assert(strings::endswith("hello world", 'd'));
	assert(!strings::endswith("hi", "hello"));

	assert(strings::hasupper("aBc"));
	assert(!strings::hasupper("abc"));

	std::printf("predicates OK: startswith, endswith, hasupper\n");
}


// ---------------------------------------------------------------------------
// case helpers: upper / lower as value and in-place.
// ---------------------------------------------------------------------------

static void test_case() {
	assert(strings::upper(std::string_view("MixEd")) == "MIXED");
	assert(strings::lower(std::string_view("MixEd")) == "mixed");

	// In-place variants mutate the argument.
	std::string up = "abc";
	strings::inplace_upper(up);
	assert(up == "ABC");

	std::string down = "ABC";
	strings::inplace_lower(down);
	assert(down == "abc");

	// Rvalue overload moves through.
	assert(strings::upper(std::string("xyz")) == "XYZ");

	std::printf("case OK: upper, lower, inplace variants\n");
}


// ---------------------------------------------------------------------------
// replace: substitutes every occurrence (value and in-place forms).
// ---------------------------------------------------------------------------

static void test_replace() {
	assert(strings::replace(std::string_view("a.b.c"), ".", "/") == "a/b/c");

	// In-place form replaces all matches.
	std::string s = "foofoo";
	strings::inplace_replace(s, "foo", "bar");
	assert(s == "barbar");

	// Replacement longer than the search term, multiple hits.
	assert(strings::replace(std::string_view("x-x"), "-", "---") == "x---x");

	std::printf("replace OK: value, in-place, length change\n");
}


// ---------------------------------------------------------------------------
// trim trio: ltrim / rtrim / trim strip whitespace at the edges.
// ---------------------------------------------------------------------------

static void test_trim() {
	assert(strings::ltrim("  hi  ") == "hi  ");
	assert(strings::rtrim("  hi  ") == "  hi");
	assert(strings::trim("  hi  ") == "hi");

	// Mixed whitespace kinds, and an all-whitespace string trims to empty.
	assert(strings::trim("\t\n hi \r\n") == "hi");
	assert(strings::trim("   ").empty());

	std::printf("trim OK: ltrim, rtrim, trim, mixed whitespace\n");
}


// ---------------------------------------------------------------------------
// Humanizers (the compiled side, strings.cc): from_bytes / from_time /
// from_delta render a number with a human-readable unit. We don't pin exact
// strings (rounding/locale), just the unit suffix and that the value shows.
// ---------------------------------------------------------------------------

static void test_humanizers() {
	// 2048 bytes -> a "2KiB"-ish string (binary scaling at base 1024).
	std::string bytes = strings::from_bytes(2048);
	assert(bytes.find("KiB") != std::string::npos);
	assert(bytes.find('2') != std::string::npos);

	// A few seconds renders in the seconds tier.
	std::string secs = strings::from_time(5);
	assert(secs.find('s') != std::string::npos);

	// Sub-second deltas fall into the small-time tier (ms/us/ns).
	std::string small = strings::from_small_time(0.002L);
	assert(small.find("ms") != std::string::npos);

	// from_delta takes nanoseconds; a std::chrono::nanoseconds overload exists.
	std::string delta = strings::from_delta(std::chrono::nanoseconds(1500000000));
	assert(!delta.empty());

	// The time_point overload measures a span; an empty span renders harmlessly.
	auto t = std::chrono::steady_clock::now();
	std::string span = strings::from_delta(t, t);
	assert(!span.empty());

	std::printf("humanizers OK: from_bytes, from_time, from_small_time, from_delta\n");
}


int main() {
	test_format();
	test_join();
	test_split();
	test_indent();
	test_padding();
	test_predicates();
	test_case();
	test_replace();
	test_trim();
	test_humanizers();
	std::printf("all strings tests passed\n");
	return 0;
}
