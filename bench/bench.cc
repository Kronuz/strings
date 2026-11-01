// strings benchmark: lower/upper, join and replace against the frozen Xapiand
// implementations (test/strings_old.hh) and, when abseil is installed
// (HAVE_ABSL), absl::AsciiStrToLower / AsciiStrToUpper / StrJoin / StrReplaceAll.
// Build with -DSTRINGS_BUILD_BENCH=ON. Best of 7 samples, ns per call. With abseil, every row first
// checks that strings and abseil return the same string for that input; a difference exits 1.
// The rows cover every path of the optimized functions: tiny and large inputs,
// one, two and many matches, and a replacement smaller, equal and larger than the search.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <initializer_list>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "strings.hh"
#include "strings_old.hh"
#ifdef HAVE_ABSL
#include "absl/strings/ascii.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#endif

template <typename F>
static double best_ns(F&& f) {
	using clk = std::chrono::steady_clock;
	double best = 1e18;
	for (int s = 0; s < 7; ++s) {
		long iters = 0;
		auto t0 = clk::now();
		auto t1 = t0;
		do {
			for (int i = 0; i < 8; ++i) { f(); }
			iters += 8;
			t1 = clk::now();
		} while (t1 - t0 < std::chrono::milliseconds(8));
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
		if (ns < best) { best = ns; }
	}
	return best;
}

static volatile size_t sink;
static int mismatches = 0;
#ifdef HAVE_ABSL
#define SAME(what, ours, theirs) \
	do { \
		if (std::string(ours) != std::string(theirs)) { \
			std::printf("DIFFERENT from abseil: %s\n", what); \
			++mismatches; \
		} \
	} while (0)
#else
#define SAME(what, ours, theirs) do { } while (0)
#endif

// One A/B row. The original runs twice, before and after the new code, as two copies of the same lambda
// (AB pastes the old code twice): each copy is compiled and laid out on its own, so `aa` = old / old2 is the
// noise of the row, layout and inlining included, and a difference of the new code below that is not one.
template <typename O, typename N, typename O2>
static void ab(const char* name, O&& old_fn, N&& new_fn, O2&& old2_fn, double a) {
	const double o = best_ns(old_fn);
	const double n = best_ns(new_fn);
	const double o2 = best_ns(old2_fn);
	std::printf("%-38s %10.1f %10.1f %10.1f %7.2f %7.2f %10.1f\n", name, o, o2, n, std::min(o, o2) / n, o / o2, a);
}
#define AB(name, old_fn, new_fn, a) ab(name, old_fn, new_fn, old_fn, a)

// n random printable bytes.
static std::string printable(std::mt19937& rng, size_t n) {
	std::string s(n, 'a');
	for (auto& c : s) { c = static_cast<char>(0x20 + rng() % 95); }
	return s;
}

// n bytes of 'a' with '_' every `every` bytes.
static std::string dense(size_t n, size_t every) {
	std::string s(n, 'a');
	for (size_t i = 0; i < n; i += every) { s[i] = '_'; }
	return s;
}

// n bytes of 'a' with '_' at the given offsets.
static std::string with_matches(size_t n, std::initializer_list<size_t> at) {
	std::string s(n, 'a');
	for (size_t i : at) { s[i] = '_'; }
	return s;
}

// `text` repeated `times` times.
static std::string repeated(std::string_view text, size_t times) {
	std::string s;
	for (size_t i = 0; i < times; ++i) { s += text; }
	return s;
}

// One replace row: copy the text and replace, as replace(string_view) does.
static void replace_row(const char* name, const std::string& text, std::string_view search, std::string_view repl) {
	std::string_view sv = text;
	double a = 0;
	SAME(name, strings::replace(sv, search, repl), absl::StrReplaceAll(sv, {{search, repl}}));
#ifdef HAVE_ABSL
	a = best_ns([&] { sink = absl::StrReplaceAll(sv, {{search, repl}}).size(); });
#endif
	AB(name, [&] { sink = old_strings::replace(sv, search, repl).size(); }, [&] { sink = strings::replace(sv, search, repl).size(); }, a);
}

int main() {
	std::mt19937 rng(3);
	std::printf("%-38s %10s %10s %10s %7s %7s %10s\n", "workload (ns/call)", "old", "old2", "new", "x", "aa", "absl");

	for (size_t n : {1, 2, 4, 8, 16, 32, 256, 4096}) {
		std::string s = printable(rng, n);
		std::string_view sv = s;
		double a = 0;
		char name[64];
		std::snprintf(name, sizeof name, "lower(string_view) %zu B", n);
		SAME(name, strings::lower(sv), absl::AsciiStrToLower(sv));
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::AsciiStrToLower(sv).size(); });
#endif
		AB(name, [&] { sink = old_strings::lower(sv).size(); }, [&] { sink = strings::lower(sv).size(); }, a);
		std::snprintf(name, sizeof name, "upper(string_view) %zu B", n);
		SAME(name, strings::upper(sv), absl::AsciiStrToUpper(sv));
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::AsciiStrToUpper(sv).size(); });
#endif
		AB(name, [&] { sink = old_strings::upper(sv).size(); }, [&] { sink = strings::upper(sv).size(); }, a);
		std::snprintf(name, sizeof name, "inplace_lower %zu B", n);
		std::string t = s;
#ifdef HAVE_ABSL
		a = best_ns([&] { absl::AsciiStrToLower(&t); sink = t.size(); });
#endif
		AB(name, [&] { old_strings::inplace_lower(t); sink = t.size(); }, [&] { strings::inplace_lower(t); sink = t.size(); }, a);
		std::snprintf(name, sizeof name, "inplace_upper %zu B", n);
#ifdef HAVE_ABSL
		a = best_ns([&] { absl::AsciiStrToUpper(&t); sink = t.size(); });
#endif
		AB(name, [&] { old_strings::inplace_upper(t); sink = t.size(); }, [&] { strings::inplace_upper(t); sink = t.size(); }, a);
	}

	for (size_t n : {1, 2, 3, 4, 10, 1000}) {
		std::vector<std::string> vs;
		std::vector<int> vi;
		for (size_t i = 0; i < n; ++i) { vs.push_back("item" + std::to_string(rng() % 1000)); vi.push_back(static_cast<int>(rng() % 100000)); }
		char name[64];
		double a = 0;
		std::snprintf(name, sizeof name, "join(vector<string>) %zu", n);
		SAME(name, strings::join(vs, ", "), absl::StrJoin(vs, ", "));
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrJoin(vs, ", ").size(); });
#endif
		AB(name, [&] { sink = old_strings::join(vs, ", ").size(); }, [&] { sink = strings::join(vs, ", ").size(); }, a);
		std::snprintf(name, sizeof name, "join(vector<int>) %zu", n);
		SAME(name, strings::join(vi, ", "), absl::StrJoin(vi, ", "));
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrJoin(vi, ", ").size(); });
#endif
		AB(name, [&] { sink = old_strings::join(vi, ", ").size(); }, [&] { sink = strings::join(vi, ", ").size(); }, a);
		if (n >= 10) {
			auto even = [](const std::string& s) { return s.size() % 2 == 0; };
			std::snprintf(name, sizeof name, "join(vector<string>, pred) %zu", n);
			AB(name, [&] { sink = old_strings::join(vs, ", ", ", ", even).size(); }, [&] { sink = strings::join(vs, ", ", ", ", even).size(); }, 0);
		}
	}
	for (size_t n : {1, 2, 3}) {
		std::vector<std::string> vs;
		for (size_t i = 0; i < n; ++i) { vs.push_back(std::string(60 + i, 'x')); }
		char name[64];
		double a = 0;
		std::snprintf(name, sizeof name, "join(vector<string>) %zu x 60 B", n);
		SAME(name, strings::join(vs, ", "), absl::StrJoin(vs, ", "));
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrJoin(vs, ", ").size(); });
#endif
		AB(name, [&] { sink = old_strings::join(vs, ", ").size(); }, [&] { sink = strings::join(vs, ", ").size(); }, a);
	}

	// replace(string_view): the result is a fresh string, so every row includes one copy of the text.
	replace_row("replace 1->3, 16 B, 2 matches", dense(16, 8), "_", "-=-");
	replace_row("replace 1->3, 32 B, 4 matches", dense(32, 8), "_", "-=-");
	replace_row("replace 1->3, 256 B, 64 matches", dense(256, 4), "_", "-=-");
	replace_row("replace 1->3, 256 B, 16 matches", dense(256, 16), "_", "-=-");
	replace_row("replace 1->3, 256 B, 3 matches", with_matches(256, {10, 100, 200}), "_", "-=-");
	replace_row("replace 1->3, 256 B, 2 matches", with_matches(256, {10, 200}), "_", "-=-");
	replace_row("replace 1->3, 256 B, 1 match", with_matches(256, {100}), "_", "-=-");
	replace_row("replace 1->3, 256 B, no match", std::string(256, 'a'), "_", "-=-");
	replace_row("replace 1->3, 4 KB, 1024 matches", dense(4096, 4), "_", "-=-");
	replace_row("replace 1->3, 4 KB, 1 match", with_matches(4096, {2000}), "_", "-=-");
	replace_row("replace 1->1, 256 B, 16 matches", dense(256, 16), "_", "-");
	replace_row("replace 1->1, 256 B, 64 matches", dense(256, 4), "_", "-");
	replace_row("replace 3->1, 256 B, 64 matches", repeated("ab_a", 64), "b_a", "-");
	replace_row("replace 3->1, 256 B, 2 matches", std::string(256, 'a').replace(10, 3, "b_a").replace(200, 3, "b_a"), "b_a", "-");
	replace_row("replace 3->1, 4 KB, 1024 matches", repeated("ab_a", 1024), "b_a", "-");
	replace_row("replace 3->0, 256 B, 64 matches", repeated("ab_a", 64), "b_a", "");
	replace_row("replace 2->5, 256 B, 32 matches", repeated("xyzabxyz", 32), "ab", "12345");

	// inplace_replace on a string the caller owns: the copy below only refills it.
	{
		const std::string grow = dense(256, 4), shrink = repeated("ab_a", 64);
		std::string t;
		AB("inplace_replace 1->3, 256 B, 64 matches", [&] { t = grow; old_strings::inplace_replace(t, "_", "-=-"); sink = t.size(); },
			[&] { t = grow; strings::inplace_replace(t, "_", "-=-"); sink = t.size(); }, 0);
		AB("inplace_replace 3->1, 256 B, 64 matches", [&] { t = shrink; old_strings::inplace_replace(t, "b_a", "-"); sink = t.size(); },
			[&] { t = shrink; strings::inplace_replace(t, "b_a", "-"); sink = t.size(); }, 0);
	}
	if (mismatches != 0) { std::printf("%d rows differ from abseil\n", mismatches); }
	return mismatches != 0 ? 1 : 0;
}
