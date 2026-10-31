// Differential test: strings::join / lower / upper / inplace_* / replace must
// match the frozen pre-optimization versions (test/strings_old.hh) exactly, on
// random and edge inputs.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "strings.hh"
#include "strings_old.hh"

static long failures = 0;

static void expect(const std::string& a, const std::string& b, const char* what, const std::string& in) {
	if (a != b && ++failures < 20) {
		std::fprintf(stderr, "MISMATCH %s on \"%s\":\n  new=\"%s\"\n  old=\"%s\"\n", what, in.c_str(), a.c_str(), b.c_str());
	}
}

static std::string random_text(std::mt19937_64& rng, size_t n, const char* alphabet, size_t alen) {
	std::string s(n, '\0');
	for (auto& c : s) { c = alphabet[rng() % alen]; }
	return s;
}

int main() {
	std::mt19937_64 rng(99);

	// Case mapping: every byte value, in every position of lengths around the vector widths.
	for (int c = 0; c < 256; ++c) {
		for (size_t n : {1, 2, 7, 8, 15, 16, 17, 31, 33, 64, 100}) {
			std::string s(n, 'q');
			for (size_t pos = 0; pos < n; pos += (n > 40 ? 13 : 1)) {
				s[pos] = static_cast<char>(c);
				expect(strings::lower(std::string_view(s)), old_strings::lower(std::string_view(s)), "lower", s);
				expect(strings::upper(std::string_view(s)), old_strings::upper(std::string_view(s)), "upper", s);
				std::string a = s, b = s;
				strings::inplace_lower(a); old_strings::inplace_lower(b);
				expect(a, b, "inplace_lower", s);
				a = s; b = s;
				strings::inplace_upper(a); old_strings::inplace_upper(b);
				expect(a, b, "inplace_upper", s);
				expect(strings::lower(std::string(s)), old_strings::lower(std::string(s)), "lower&&", s);
				expect(strings::upper(std::string(s)), old_strings::upper(std::string(s)), "upper&&", s);
				s[pos] = 'q';
			}
		}
	}
	for (int iter = 0; iter < 20000; ++iter) {
		std::string s(rng() % 300, '\0');
		for (auto& c : s) { c = static_cast<char>(rng()); }
		expect(strings::lower(std::string_view(s)), old_strings::lower(std::string_view(s)), "lower", s);
		expect(strings::upper(std::string_view(s)), old_strings::upper(std::string_view(s)), "upper", s);
	}

	// replace: small alphabets so matches are frequent and overlap; every size relation.
	const char* alphabets[] = {"ab", "abc", "aab", "xy.", "a"};
	for (int iter = 0; iter < 60000; ++iter) {
		const char* al = alphabets[rng() % 5];
		size_t alen = std::strlen(al);
		std::string text = random_text(rng, rng() % 40, al, alen);
		std::string search = random_text(rng, rng() % 4, al, alen);
		std::string repl = random_text(rng, rng() % 6, al, alen);
		if (search.empty()) { continue; }  // the original (and still the new) code loops forever on an empty search
		std::string in = text + " | " + search + " | " + repl;
		expect(strings::replace(std::string_view(text), search, repl), old_strings::replace(std::string_view(text), search, repl), "replace", in);
		std::string a = text, b = text;
		strings::inplace_replace(a, search, repl);
		old_strings::inplace_replace(b, search, repl);
		expect(a, b, "inplace_replace", in);
	}

	// join: strings, string_views, ints of several widths, doubles, bools, chars.
	for (int iter = 0; iter < 5000; ++iter) {
		size_t n = rng() % 12;
		std::string d = random_text(rng, rng() % 4, ", ;-", 4);
		std::string ld = random_text(rng, rng() % 5, " andor&", 7);
		std::vector<std::string> vs;
		std::vector<std::string_view> vv;
		std::vector<int> vi;
		std::vector<unsigned long long> vu;
		std::vector<long long> vl;
		std::vector<short> vsh;
		std::vector<unsigned char> vuc;
		std::vector<char> vc;
		std::vector<double> vd;
		std::vector<bool> vb;
		for (size_t i = 0; i < n; ++i) {
			vs.push_back(random_text(rng, rng() % 10, "abcXYZ ,", 8));
			vi.push_back(static_cast<int>(rng()));
			vu.push_back(rng());
			vl.push_back(static_cast<long long>(rng()) >> (rng() % 64));
			vsh.push_back(static_cast<short>(rng()));
			vuc.push_back(static_cast<unsigned char>(rng()));
			vc.push_back(static_cast<char>(rng()));
			vd.push_back(static_cast<double>(rng() % 100000) / 8.0);
			vb.push_back((rng() & 1) != 0);
		}
		for (auto& s : vs) { vv.push_back(s); }
		vi.push_back(0); vi.push_back(INT32_MIN); vi.push_back(INT32_MAX);
		vl.push_back(INT64_MIN); vl.push_back(INT64_MAX);
		vu.push_back(UINT64_MAX);
		std::string tag = "n=" + std::to_string(n) + " d=" + d + " ld=" + ld;
#define J(V) expect(strings::join(V, d, ld), old_strings::join(V, d, ld), "join " #V, tag); \
	expect(strings::join(V, d), old_strings::join(V, d), "join2 " #V, tag)
		J(vs); J(vv); J(vi); J(vu); J(vl); J(vsh); J(vuc); J(vc); J(vd); J(vb);
#undef J
		auto pred_s = [](const std::string& s) { return s.size() % 2 == 0; };
		expect(strings::join(vs, d, ld, pred_s), old_strings::join(vs, d, ld, pred_s), "join pred s", tag);
		expect(strings::join(vs, d, pred_s), old_strings::join(vs, d, pred_s), "join pred s2", tag);
		auto pred_i = [](int x) { return x % 3 == 0; };
		expect(strings::join(vi, d, ld, pred_i), old_strings::join(vi, d, ld, pred_i), "join pred i", tag);
		auto pred_b = [](bool x) { return x; };
		expect(strings::join(vb, d, ld, pred_b), old_strings::join(vb, d, ld, pred_b), "join pred b", tag);
	}
	if (failures != 0) { std::fprintf(stderr, "%ld mismatches\n", failures); return 1; }
	std::printf("strings differential OK\n");
	return 0;
}
