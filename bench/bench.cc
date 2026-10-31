// strings benchmark: lower/upper, join and replace against the frozen old
// implementations (test/strings_old.hh) and, when abseil is installed
// (HAVE_ABSL), absl::AsciiStrToLower / StrJoin / StrReplaceAll. Build with
// -DSTRINGS_BUILD_BENCH=ON. Best of 7 samples, ns per call.
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
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

static void row(const char* name, double o, double n, double a) {
	std::printf("%-30s %10.1f %10.1f %7.2f %10.1f\n", name, o, n, o / n, a);
}

int main() {
	std::mt19937 rng(3);
	std::printf("%-30s %10s %10s %7s %10s\n", "workload (ns/call)", "old", "new", "x", "absl");
	for (size_t n : {16, 256, 4096}) {
		std::string s(n, 'a');
		for (auto& c : s) { c = static_cast<char>(0x20 + rng() % 95); }
		std::string_view sv = s;
		double a = 0;
		char name[64];
		std::snprintf(name, sizeof name, "lower(string_view) %zu B", n);
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::AsciiStrToLower(sv).size(); });
#endif
		row(name, best_ns([&] { sink = old_strings::lower(sv).size(); }), best_ns([&] { sink = strings::lower(sv).size(); }), a);
		std::snprintf(name, sizeof name, "inplace_lower %zu B", n);
		std::string t = s;
#ifdef HAVE_ABSL
		a = best_ns([&] { absl::AsciiStrToLower(&t); sink = t.size(); });
#endif
		row(name, best_ns([&] { old_strings::inplace_lower(t); sink = t.size(); }), best_ns([&] { strings::inplace_lower(t); sink = t.size(); }), a);
	}
	for (size_t n : {10, 1000}) {
		std::vector<std::string> vs;
		std::vector<int> vi;
		for (size_t i = 0; i < n; ++i) { vs.push_back("item" + std::to_string(rng() % 1000)); vi.push_back(static_cast<int>(rng() % 100000)); }
		char name[64];
		double a = 0;
		std::snprintf(name, sizeof name, "join(vector<string>) %zu", n);
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrJoin(vs, ", ").size(); });
#endif
		row(name, best_ns([&] { sink = old_strings::join(vs, ", ").size(); }), best_ns([&] { sink = strings::join(vs, ", ").size(); }), a);
		std::snprintf(name, sizeof name, "join(vector<int>) %zu", n);
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrJoin(vi, ", ").size(); });
#endif
		row(name, best_ns([&] { sink = old_strings::join(vi, ", ").size(); }), best_ns([&] { sink = strings::join(vi, ", ").size(); }), a);
	}
	for (size_t n : {256, 4096}) {
		std::string s(n, 'a');
		for (size_t i = 0; i < n; i += 4) { s[i] = '_'; }
		std::string_view sv = s;
		char name[64];
		double a = 0;
		std::snprintf(name, sizeof name, "replace 1->3 chars, %zu B", n);
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::StrReplaceAll(sv, {{"_", "-=-"}}).size(); });
#endif
		row(name, best_ns([&] { sink = old_strings::replace(sv, "_", "-=-").size(); }), best_ns([&] { sink = strings::replace(sv, "_", "-=-").size(); }), a);
	}
	return 0;
}
