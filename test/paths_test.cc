// Case mapping, replace and join paths against independent references, plus the buffer behavior
// that keeps replace from reallocating.
//
// Oracles (not the implementation):
//  * lower / upper: the per-byte definition, 'A'..'Z' <-> 'a'..'z' and nothing else.
//  * replace: Python's str.replace for a non-empty search: matches are found left to right and
//    do not overlap. ref_replace is a naive scan written from that definition and is checked
//    exhaustively on small strings over every size relation (shrink, same size, grow, zero
//    matches, one match, many matches, overlapping patterns), then on long random strings.
//  * join: pieces and delimiters laid end to end, the last delimiter before the last element.
//  * buffers: a growing replace with more than 16 matches builds its result in one buffer sized
//    for all of them (capacity within one allocation granule of the size), and a shrinking or
//    same-size replace edits the string where it is (same data pointer and capacity).
//
// CHECK aborts instead of asserting, so the test still checks under NDEBUG.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "strings.hh"
#include "strings_old.hh"

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			std::abort(); \
		} \
	} while (0)

#define CHECK_EQ(got, want, ...) \
	do { \
		const std::string g_ = (got), w_ = (want); \
		if (g_ != w_) { \
			std::fprintf(stderr, "%s:%d: got \"%s\" want \"%s\" for ", __FILE__, __LINE__, g_.c_str(), w_.c_str()); \
			std::fprintf(stderr, __VA_ARGS__); \
			std::fputc('\n', stderr); \
			std::abort(); \
		} \
	} while (0)


static std::string ref_replace(const std::string& s, const std::string& a, const std::string& b) {
	std::string out;
	std::size_t i = 0;
	while (i < s.size()) {
		if (s.compare(i, a.size(), a) == 0 && i + a.size() <= s.size()) {
			out += b;
			i += a.size();
		} else {
			out += s[i++];
		}
	}
	return out;
}

static std::string all_strings_of(const char* alphabet, std::size_t len, std::size_t index) {
	const std::size_t base = std::strlen(alphabet);
	std::string s(len, '\0');
	for (std::size_t i = 0; i < len; ++i) {
		s[i] = alphabet[index % base];
		index /= base;
	}
	return s;
}

static std::size_t power(std::size_t base, std::size_t exp) {
	std::size_t r = 1;
	while (exp--) { r *= base; }
	return r;
}

static void check_replace(const std::string& text, const std::string& search, const std::string& repl) {
	const std::string want = ref_replace(text, search, repl);
	std::string a = text;
	strings::inplace_replace(a, search, repl);
	CHECK_EQ(a, want, "inplace_replace(\"%s\", \"%s\", \"%s\")", text.c_str(), search.c_str(), repl.c_str());
	CHECK_EQ(strings::replace(std::string_view(text), search, repl), want, "replace(view \"%s\", \"%s\", \"%s\")", text.c_str(), search.c_str(), repl.c_str());
	CHECK_EQ(strings::replace(std::string(text), search, repl), want, "replace(&& \"%s\", \"%s\", \"%s\")", text.c_str(), search.c_str(), repl.c_str());
	std::string b = text;
	old_strings::inplace_replace(b, search, repl);
	CHECK_EQ(a, b, "old != new for (\"%s\", \"%s\", \"%s\")", text.c_str(), search.c_str(), repl.c_str());
}

// Every text over {a, b} up to 10 bytes, every search over {a, b} of 1 to 3 bytes, every
// replacement over {a, b, c} of 0 to 3 bytes.
static void test_replace_exhaustive() {
	long cases = 0;
	// Without and with a 30-byte prefix that no search matches: past 32 bytes replace edits in
	// place or builds a buffer, up to 32 it makes one replace call per match.
	for (std::size_t pad : {0, 30}) {
		for (std::size_t tl = 0; tl <= 10; ++tl) {
			for (std::size_t ti = 0; ti < power(2, tl); ++ti) {
				const std::string text = std::string(pad, 'z') + all_strings_of("ab", tl, ti);
				for (std::size_t sl = 1; sl <= 3; ++sl) {
					for (std::size_t si = 0; si < power(2, sl); ++si) {
						const std::string search = all_strings_of("ab", sl, si);
						for (std::size_t rl = 0; rl <= 3; ++rl) {
							for (std::size_t ri = 0; ri < power(3, rl); ++ri) {
								check_replace(text, search, all_strings_of("abc", rl, ri));
								++cases;
							}
						}
					}
				}
			}
		}
	}
	std::printf("strings paths OK: replace matches the reference on %ld exhaustive cases\n", cases);
}

// Strings past the short-string buffer, so a grown result reallocates.
static void test_replace_long_random() {
	std::mt19937_64 rng(7);
	const char* alphabets[] = {"ab", "abc", "a", "aab"};
	for (int iter = 0; iter < 30000; ++iter) {
		const char* al = alphabets[rng() % 4];
		const std::size_t alen = std::strlen(al);
		auto make = [&](std::size_t n) { std::string s(n, '\0'); for (auto& c : s) { c = al[rng() % alen]; } return s; };
		const std::string text = make(20 + rng() % 300);
		const std::string search = make(1 + rng() % 4);
		const std::string repl = make(rng() % 9);
		check_replace(text, search, repl);
	}
	std::printf("strings paths OK: replace matches the reference on long random strings\n");
}

// Every match count around the point where replace stops recording offsets in an array (16) and
// moves to a vector (and past it), with gaps of every small size between matches, for every size
// relation.
static void test_replace_match_counts() {
	std::vector<std::size_t> counts;
	for (std::size_t m = 0; m <= 40; ++m) { counts.push_back(m); }
	for (std::size_t m : {63, 64, 65, 127, 128, 129, 300}) { counts.push_back(m); }
	for (std::size_t matches : counts) {
		for (std::size_t gap : {0, 1, 2, 5, 40}) {
			for (std::size_t sl = 1; sl <= 3; ++sl) {
				const std::string search = std::string(sl, 'x');
				std::string text = std::string(7, 'a');
				for (std::size_t i = 0; i < matches; ++i) { text += search + std::string(gap, 'a'); }
				for (std::size_t rl = 0; rl <= 5; ++rl) {
					const std::string repl = std::string(rl, 'y');
					check_replace(text, search, repl);
				}
			}
		}
	}
	std::printf("strings paths OK: replace matches the reference for 0 to 300 matches\n");
}

// Replacement and gap sizes of every length up to 20, around the sizes where the copies switch
// from word moves to the library (16), for every size relation.
static void test_replace_piece_sizes() {
	for (std::size_t matches : {2, 3, 4, 5, 20}) {
		for (std::size_t gap = 0; gap <= 20; ++gap) {
			for (std::size_t sl = 1; sl <= 3; ++sl) {
				const std::string search = std::string(sl, 'x');
				std::string text = "head";
				for (std::size_t i = 0; i < matches; ++i) {
					text += search;
					for (std::size_t j = 0; j < gap; ++j) { text += static_cast<char>('a' + (i + j) % 7); }
				}
				for (std::size_t rl = 0; rl <= 20; ++rl) {
					std::string repl;
					for (std::size_t j = 0; j < rl; ++j) { repl += static_cast<char>('A' + j % 13); }
					check_replace(text, search, repl);
				}
			}
		}
	}
	std::printf("strings paths OK: replace matches the reference for every piece size up to 20\n");
}

// A search or replacement that views into the string being edited keeps its value.
static void test_replace_aliasing() {
	std::mt19937_64 rng(11);
	for (int iter = 0; iter < 20000; ++iter) {
		const std::size_t n = 10 + rng() % 60;
		std::string text(n, '\0');
		for (auto& c : text) { c = "abc"[rng() % 3]; }
		const std::size_t sp = rng() % (n - 3), sl = 1 + rng() % 3;
		const std::size_t rp = rng() % (n - 6), rl = rng() % 7;
		const std::string search = text.substr(sp, sl), repl = text.substr(rp, rl);
		std::string a = text;
		std::string_view sv(a.data() + sp, sl), rv(a.data() + rp, rl);
		strings::inplace_replace(a, sv, rv);
		CHECK_EQ(a, ref_replace(text, search, repl), "aliased replace(\"%s\", \"%s\", \"%s\")", text.c_str(), search.c_str(), repl.c_str());
	}
	std::printf("strings paths OK: a search or replacement viewing into the string keeps its value\n");
}

// lower and upper against the per-byte definition, every byte value at every position of
// every length up to 40 (covers each word-wise branch and the overlap of its two words), as
// string_view, in place and as a moved string.
static void test_case_mapping() {
	auto ref_lower = [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c); };
	auto ref_upper = [](unsigned char c) { return static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c); };
	for (std::size_t n = 0; n <= 40; ++n) {
		for (int byte = 0; byte < 256; ++byte) {
			for (std::size_t at = 0; at < n; ++at) {
				std::string s(n, '\0');
				for (std::size_t i = 0; i < n; ++i) { s[i] = static_cast<char>("aZ@[`{ 09"[i % 9]); }
				s[at] = static_cast<char>(byte);
				std::string want_lower = s, want_upper = s;
				for (auto& c : want_lower) { c = ref_lower(static_cast<unsigned char>(c)); }
				for (auto& c : want_upper) { c = ref_upper(static_cast<unsigned char>(c)); }
				CHECK(strings::lower(std::string_view(s)) == want_lower);
				CHECK(strings::upper(std::string_view(s)) == want_upper);
				std::string a = s;
				strings::inplace_lower(a);
				CHECK(a == want_lower);
				a = s;
				strings::inplace_upper(a);
				CHECK(a == want_upper);
				CHECK(strings::lower(std::string(s)) == want_lower);
				CHECK(strings::upper(std::string(s)) == want_upper);
				// The helper behind lower / upper of a view, for every length (the public
				// functions take the short strings through a different path).
				std::string out(n, '\0');
				strings::detail::ascii_lower(out.data(), s.data(), n);
				CHECK(out == want_lower);
				strings::detail::ascii_upper(out.data(), s.data(), n);
				CHECK(out == want_upper);
			}
		}
	}
	std::printf("strings paths OK: lower and upper match the per-byte definition for every byte, position and length to 40\n");
}

// Applies the replace to a copy that owns a heap buffer and reports what happened to the buffer.
struct Edit {
	std::string result;
	bool same_buffer;       // the data pointer and capacity did not change
	bool exact_capacity;    // the capacity is within one allocation granule of the size
};

static Edit edit(const std::string& base, std::string_view search, std::string_view repl) {
	Edit e;
	e.result = base;
	const char* data = e.result.data();
	const std::size_t capacity = e.result.capacity();
	strings::inplace_replace(e.result, search, repl);
	e.same_buffer = e.result.data() == data && e.result.capacity() == capacity;
	e.exact_capacity = e.result.capacity() <= e.result.size() + 16;
	return e;
}

static void test_replace_buffers() {
	std::string dense(256, 'a');
	for (std::size_t i = 0; i < dense.size(); i += 4) { dense[i] = '_'; }

	// Grow with more than 16 matches: one buffer for all of them, not room for two matches
	// that reallocates as it fills.
	Edit e = edit(dense, "_", "-=-");
	CHECK(e.result == ref_replace(dense, "_", "-=-") && e.exact_capacity);
	std::string seventeen(256, 'a');
	for (int i = 0; i < 17; ++i) { seventeen[10 + 13 * i] = '_'; }
	e = edit(seventeen, "_", "-=-");
	CHECK(e.result == ref_replace(seventeen, "_", "-=-") && e.exact_capacity);
	std::string multi;
	for (int i = 0; i < 32; ++i) { multi += "xyzabxyz"; }
	e = edit(multi, "ab", "12345");
	CHECK(e.result == ref_replace(multi, "ab", "12345") && e.exact_capacity);

	// Room already: even with many matches the string is edited where it is.
	std::string roomy = dense;
	roomy.reserve(2048);
	const char* roomy_data = roomy.data();
	strings::inplace_replace(roomy, "_", "-=-");
	CHECK(roomy == ref_replace(dense, "_", "-=-") && roomy.data() == roomy_data);

	// Grow with up to 16 matches: the string grows once and each tail moves once.
	std::string few(256, 'a');
	few[10] = few[100] = few[200] = '_';
	e = edit(few, "_", "-=-");
	CHECK(e.result == ref_replace(few, "_", "-=-"));
	std::string sixteen(256, 'a');
	for (int i = 0; i < 16; ++i) { sixteen[10 + 13 * i] = '_'; }
	e = edit(sixteen, "_", "-=-");
	CHECK(e.result == ref_replace(sixteen, "_", "-=-"));

	// Shrink and same size edit in place.
	std::string shrink;
	for (int i = 0; i < 64; ++i) { shrink += "ab_a"; }
	e = edit(shrink, "b_a", "-");
	CHECK(e.result == ref_replace(shrink, "b_a", "-") && e.same_buffer);
	for (int matches : {2, 5, 16, 17, 18, 40}) {
		std::string text;
		for (int i = 0; i < matches; ++i) { text += "padding ab_a "; }
		e = edit(text, "b_a", "-");
		CHECK(e.result == ref_replace(text, "b_a", "-") && e.same_buffer);
	}
	e = edit(dense, "_", "-");
	CHECK(e.result == ref_replace(dense, "_", "-") && e.same_buffer);
	e = edit(dense, "aaa", "");
	CHECK(e.result == ref_replace(dense, "aaa", "") && e.same_buffer);

	// One match shifts the tail once; no match changes nothing.
	std::string one(256, 'a');
	one[100] = '_';
	e = edit(one, "_", "-=-");
	CHECK(e.result == ref_replace(one, "_", "-=-"));
	e = edit(one, "_", "-");
	CHECK(e.result == ref_replace(one, "_", "-") && e.same_buffer);
	e = edit(one, "zz", "-=-");
	CHECK(e.result == one && e.same_buffer);
	std::printf("strings paths OK: replace builds one buffer for a growing result and edits in place otherwise\n");
}


static std::string ref_join(const std::vector<std::string>& v, const std::string& d, const std::string& ld) {
	std::string out;
	for (std::size_t i = 0; i < v.size(); ++i) {
		if (i > 0) { out += (i + 1 == v.size()) ? ld : d; }
		out += v[i];
	}
	return out;
}

static void test_join_paths() {
	const std::size_t sizes[] = {0, 1, 5, 22, 23, 40, 100};
	const char* delimiters[] = {"", ",", ", ", " and "};
	std::mt19937_64 rng(5);
	for (std::size_t n = 0; n <= 6; ++n) {
		for (const char* d : delimiters) {
			for (const char* ld : delimiters) {
				for (int rep = 0; rep < 20; ++rep) {
					std::vector<std::string> v;
					std::vector<std::string_view> views;
					for (std::size_t i = 0; i < n; ++i) {
						std::string s(sizes[rng() % 7], '\0');
						for (auto& c : s) { c = static_cast<char>('a' + rng() % 26); }
						v.push_back(s);
					}
					for (const auto& s : v) { views.push_back(s); }
					const std::string want = ref_join(v, d, ld);
					CHECK_EQ(strings::join(v, d, ld), want, "join n=%zu", n);
					CHECK_EQ(strings::join(views, d, ld), want, "join views n=%zu", n);
					CHECK_EQ(old_strings::join(v, d, ld), want, "old join n=%zu", n);
					CHECK_EQ(strings::join(v, d), ref_join(v, d, d), "join2 n=%zu", n);
				}
			}
		}
	}
	std::printf("strings paths OK: join of strings matches the reference for 0 to 6 elements\n");
}

int main() {
	test_case_mapping();
	test_replace_exhaustive();
	test_replace_long_random();
	test_replace_match_counts();
	test_replace_piece_sizes();
	test_replace_aliasing();
	test_replace_buffers();
	test_join_paths();
	std::printf("all strings path tests passed\n");
	return 0;
}
