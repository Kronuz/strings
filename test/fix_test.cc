// Correctness fixes: indent, hasupper, inplace_replace with an empty search.
//
// Oracles (not the implementation):
//  * indent: Python's textwrap.indent(text, prefix). The golden strings below were
//    produced by running it; ref_indent is an independent reference written from
//    its definition (split keeping line ends, prefix a line unless it is all
//    whitespace) and is checked exhaustively over small strings.
//  * hasupper: ASCII 'A'..'Z', the same classes as strings::upper / lower.
//  * inplace_replace("", r): Python's str.replace("", r) ("abc","-" -> "-a-b-c-").
// The frozen old indent is kept to assert exactly which inputs changed.
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>

#include "strings.hh"
#include "split.h"

static long failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::fprintf(stderr, "FAIL line %d: ", __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } } while (0)

namespace frozen {
// indent() before the fix: Split skipped empty lines, so blank lines and the
// trailing newline were lost.
inline std::string indent(std::string_view str, char sep, int level, bool indent_first=true) {
	std::string result;
	result.reserve(((indent_first ? 1 : 0) + std::count(str.begin(), str.end(), '\n')) * level);
	if (indent_first) { result.append(level, sep); }
	Split<std::string_view> lines(str, '\n');
	auto it = lines.begin();
	if (it != lines.end()) {
		for (; !it.last(); ++it) {
			const auto& line = *it;
			result.append(line);
			result.push_back('\n');
			result.append(level, sep);
		}
		const auto& line = *it;
		result.append(line);
	}
	return result;
}
inline bool hasupper(std::string_view str) {
	for (const auto& c : str) { if (isupper(c) != 0) { return true; } }
	return false;
}
}  // namespace frozen

static bool all_space(const std::string& l) {
	for (char c : l) { if (!(c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f' || c == '\n')) { return false; } }
	return true;
}

// textwrap.indent from its definition, '\n' as the only line break.
static std::string ref_indent(const std::string& text, char sep, int level, bool first) {
	std::string out, line;
	bool is_first = true;
	for (std::size_t i = 0; i <= text.size(); ++i) {
		bool end = i == text.size();
		if (!end) { line += text[i]; }
		if (end || text[i] == '\n') {
			if (!line.empty()) {
				if (!all_space(line) && (first || !is_first)) { out.append(level, sep); }
				out += line;
			}
			line.clear();
			is_first = false;
		}
	}
	return out;
}

static void golden() {
	struct { const char* in; const char* want; } v[] = {
		{"a\n\nb", "..a\n\n..b"}, {"a\n", "..a\n"}, {"\n", "\n"}, {"", ""}, {"a\n\n", "..a\n\n"},
		{"\na", "\n..a"}, {"a\nb\n", "..a\n..b\n"}, {"a\n  \nb\n", "..a\n  \n..b\n"},
		{"  x\n\t\n y", "..  x\n\t\n.. y"}, {"a\r\nb\r\n\r\n", "..a\r\n..b\r\n\r\n"},
	};
	for (auto& c : v) {
		auto got = strings::indent(c.in, '.', 2);
		CHECK(got == c.want, "indent golden \"%s\": got \"%s\" want \"%s\"", c.in, got.c_str(), c.want);
	}
}

static void indent_exhaustive() {
	const char al[] = {'a', ' ', '\n', '\t', '\r'};
	std::string s;
	std::size_t total = 1;
	for (int len = 0; len <= 7; ++len) {
		for (std::size_t k = 0; k < total; ++k) {
			std::size_t m = k;
			s.assign(len, 'a');
			for (int i = 0; i < len; ++i) { s[i] = al[m % 5]; m /= 5; }
			for (int level : {0, 1, 3}) {
				for (bool first : {true, false}) {
					auto got = strings::indent(s, '.', level, first);
					auto want = ref_indent(s, '.', level, first);
					CHECK(got == want, "indent len=%d level=%d first=%d \"%s\"", len, level, first, got.c_str());
					// Old vs new: identical unless the text has an empty or blank line, a
					// trailing newline, or is empty (the classes that changed by design).
					bool changed = s.empty() || s.back() == '\n' || s.find("\n\n") != std::string::npos
						|| s.front() == '\n';
					bool blank = false;
					{ std::string l; for (std::size_t i = 0; i <= s.size(); ++i) { if (i == s.size() || s[i] == '\n') { if (!l.empty() && all_space(l)) { blank = true; } l.clear(); } else { l += s[i]; } } }
					auto old = frozen::indent(s, '.', level, first);
					if (level > 0 && changed && !(s == "\n" && !first)) {
						CHECK(old != got || (s.empty() && !first), "indent unchanged where it should differ: \"%s\" level=%d first=%d", s.c_str(), level, first);
					}
					if (level > 0 && !changed && !blank) {
						CHECK(old == got, "indent changed outside the fixed classes: \"%s\" level=%d first=%d old=\"%s\" new=\"%s\"", s.c_str(), level, first, old.c_str(), got.c_str());
					}
				}
			}
		}
		total *= 5;
	}
}

static bool ref_hasupper(std::string_view s) {
	for (unsigned char c : s) { if (c >= 'A' && c <= 'Z') { return true; } }
	return false;
}

static void hasupper_test() {
	for (int c = 0; c < 256; ++c) {
		std::string s(1, static_cast<char>(c));
		CHECK(strings::hasupper(s) == ref_hasupper(s), "hasupper byte %d", c);
		s = "abc" + s + "def";
		CHECK(strings::hasupper(s) == ref_hasupper(s), "hasupper embedded byte %d", c);
	}
	CHECK(!strings::hasupper("") && !strings::hasupper("abc 123") && strings::hasupper("abC"), "hasupper basics");
	// In a Latin-1 locale libc isupper() treats 0xC0..0xDE as upper case; the old function followed it.
	if (std::setlocale(LC_CTYPE, "en_US.ISO-8859-1") || std::setlocale(LC_CTYPE, "en_US.ISO8859-1")) {
		std::string s(1, static_cast<char>(0xC9));
		// (isupper(char) with a negative value is itself undefined; libcs differ on it.)
		std::printf("old hasupper(0xC9) in Latin-1: %d\n", frozen::hasupper(s) ? 1 : 0);
		CHECK(!strings::hasupper(s), "hasupper must be ASCII only");
		std::setlocale(LC_CTYPE, "C");
	} else {
		std::printf("(no Latin-1 locale here: locale-dependence of the old hasupper not demonstrated)\n");
	}
}

static std::string ref_replace_empty(const std::string& s, const std::string& r) {
	std::string out = r;
	for (char c : s) { out += c; out += r; }
	return out;
}

static void replace_empty() {
	std::string a = "abc";
	strings::inplace_replace(a, "", "-");
	CHECK(a == "-a-b-c-", "python golden");
	a = "";
	strings::inplace_replace(a, "", "x");
	CHECK(a == "x", "empty text");
	a = "abc";
	strings::inplace_replace(a, "", "");
	CHECK(a == "abc", "empty both");
	CHECK(strings::replace(std::string_view("ab"), "", "--") == "--a--b--", "replace view");
	CHECK(strings::replace(std::string("ab"), "", "") == "ab", "replace rvalue");
	const char al[] = {'a', 'b'};
	for (int tl = 0; tl <= 5; ++tl)
		for (int tm = 0; tm < (1 << tl); ++tm) {
			std::string t;
			for (int i = 0; i < tl; ++i) { t += al[(tm >> i) & 1]; }
			for (int rl = 0; rl <= 3; ++rl)
				for (int rm = 0; rm < (1 << rl); ++rm) {
					std::string r;
					for (int i = 0; i < rl; ++i) { r += al[(rm >> i) & 1]; }
					std::string x = t;
					strings::inplace_replace(x, "", r);
					CHECK(x == ref_replace_empty(t, r), "empty search \"%s\" \"%s\"", t.c_str(), r.c_str());
				}
		}
}

int main() {
	golden();
	indent_exhaustive();
	hasupper_test();
	replace_empty();
	if (failures != 0) { std::fprintf(stderr, "%ld failures\n", failures); return 1; }
	std::printf("strings fixes OK: indent (textwrap.indent), hasupper (ASCII), inplace_replace empty search\n");
	return 0;
}
