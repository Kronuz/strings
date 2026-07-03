/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * usage: a runnable, self-checking example that drives `strings` the way
 * Xapiand does: formatted log/error text, joined option sets and node lists,
 * query ranges split on "..", case-folded terms and content types, prefix /
 * suffix checks for paths and wildcard tokens, URI-path replacement, and the
 * byte / duration humanizers used in HTTP headers and replication logs.
 *
 * It doubles as a REGRESSION TEST: every step asserts the exact output Xapiand
 * relies on, and main() returns non-zero on any failure, so CMake registers it
 * with ctest.
 */

#include "strings.hh"

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>


static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)


int main() {
	// Xapiand builds log lines, HTTP headers, pid-file text, and schema errors
	// through strings::format. The silent fallback keeps bad diagnostic format
	// strings from throwing out of the string library.
	CHECK(strings::format("Signal received: {}\n", "SIGTERM") == "Signal received: SIGTERM\n");
	CHECK(strings::format("Non-stored object has a size limit of {}", strings::from_bytes(4096)) == "Non-stored object has a size limit of 4KiB");
	CHECK(strings::format("{} {} {}", 1) == "{} {} {}");

	// Xapiand joins version banners, cluster-node lists, query fragments, and
	// schema accuracy sets, including the distinct final delimiter form.
	std::vector<std::string> components{"Xapian v1.4.22", "ICU v74.2", "Lua v5.4"};
	CHECK(strings::join(components, ", ", " and ") == "Xapian v1.4.22, ICU v74.2 and Lua v5.4");
	std::vector<std::string> nodes{"node-a", "node-b"};
	CHECK(strings::join(nodes, ", ", " and ") == "node-a and node-b");
	std::vector<std::string> query_parts{"title:hello", "body:world"};
	CHECK(strings::join(query_parts, "|") == "title:hello|body:world");
	std::vector<int> revisions{41, 42, 43};
	CHECK(strings::join(revisions, ",") == "41,42,43");

	// Query DSL range parsing splits field values on the literal ".." delimiter.
	auto dates = strings::split(std::string("2024-01-01..2024-12-31"), std::string(".."));
	CHECK(dates.size() == 2);
	CHECK(dates[0] == "2024-01-01");
	CHECK(dates[1] == "2024-12-31");

	// Xapiand case-folds node names, MIME types, boolean strings, and query terms.
	CHECK(strings::lower(std::string_view("Text/HTML; Charset=UTF-8")) == "text/html; charset=utf-8");
	CHECK(strings::lower(std::string("Node-A")) == "node-a");
	CHECK(strings::hasupper("userID"));
	CHECK(!strings::hasupper("userid"));

	// Path and query handling relies on string and char prefix/suffix checks.
	std::string_view absolute = "/indexes/books";
	std::string_view internal = ".xapiand/indices/books";
	std::string_view wildcard = "title:prefix**";
	std::string directory = "/srv/db/.xapiand";
	CHECK(strings::startswith(absolute, '/'));
	CHECK(strings::startswith(internal, ".xapiand/"));
	CHECK(strings::endswith(directory, "/.xapiand"));
	CHECK(strings::endswith(wildcard, "**"));
	CHECK(strings::endswith(wildcard, '*'));

	// Foreign-schema defaults percent-encode path separators with replace().
	CHECK(strings::replace(std::string_view("books/authors/en"), "/", "%2F") == "books%2Fauthors%2Fen");
	std::string endpoint_path = "books/authors";
	CHECK(strings::replace(std::move(endpoint_path), "/", "%2F") == "books%2Fauthors");

	// Xapiand renders sizes and durations in human-facing logs and HTTP headers.
	CHECK(strings::from_bytes(0) == "0B");
	CHECK(strings::from_bytes(1536) == "1.5KiB");
	CHECK(strings::from_delta(2'000'000LL) == "2ms");
	CHECK(strings::from_delta(1'500'000'000LL) == "1.5s");
	CHECK(strings::from_delta(std::chrono::nanoseconds(65'000'000'000LL)) == "1.08min");

	if (failures == 0) {
		std::puts("usage: all checks passed (format + join/split + case + predicates + replace + humanizers)");
	}
	return failures == 0 ? 0 : 1;
}
