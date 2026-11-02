#!/usr/bin/env python3
"""Mutation check: breaks strings.hh one line at a time and requires a test to fail on each.

Usage: tools/mutants.py <out dir> [compiler include flags for the dependencies...]

Each mutant is a copy of strings.hh with one edit; test/paths_test.cc (or test/diff_test.cc, where named)
is built against it with $CXX (default clang++) and must exit non-zero. A mutant that survives means a
path of replace, join or case mapping that no test pins. The dependency flags are the -I of char-classify,
split, static-string and repr, e.g. the _deps/*-src directories of a configured build.
"""
import os
import subprocess
import sys

repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1]
inc = sys.argv[2:]
cxx = os.environ.get("CXX", "clang++")
src = open(os.path.join(repo, "strings.hh")).read()

MUTANTS = [
    ("M1 grow in place: move tails by k*growth, not (k+1)*growth", "paths",
     "detail::shift(data + from + (k + 1) * growth, data + from, end - from);",
     "detail::shift(data + from + k * growth, data + from, end - from);"),
    ("M2 shrink in place: the next search starts one byte late", "paths",
     "found[k + 1] : str.find(search, pos + search_size);\n\t\t\twrite = detail::shift",
     "found[k + 1] : str.find(search, pos + search_size + 1);\n\t\t\twrite = detail::shift"),
    ("M3 many matches: buffer one byte too long", "paths",
     "std::string out(size + count * growth, '\\0');",
     "std::string out(size + count * growth + 1, '\\0');"),
    ("M4 growing in place: alias guard removed", "paths",
     "if (aliased(false)) {",
     "if (false) {"),
    ("M5 many matches: the search for the rest skips a byte", "paths",
     "for (; next != std::string::npos; next = str.find(search, next + search_size)) {\n\t\t\trest.push_back(next);",
     "for (; next != std::string::npos; next = str.find(search, next + search_size + 1)) {\n\t\t\trest.push_back(next);"),
    ("M6 two or three matches: the growth so far is not added", "paths",
     "str.replace(match(k) + k * growth, search_size, replacement);",
     "str.replace(match(k), search_size, replacement);"),
    ("M7 same size: resume the search one byte late", "paths",
     "pos = str.find(search, pos + replacement_size);\n\t\t} while",
     "pos = str.find(search, pos + replacement_size + 1);\n\t\t} while"),
    ("M8 shrinking: the last tail is one byte short", "paths",
     "write = detail::shift(data + write, data + last, size - last) - data;",
     "write = detail::shift(data + write, data + last, size - last - 1) - data;"),
    ("M9 shrinking: alias guard removed", "paths",
     "// read cursor, and the next match is found before its gap is moved.\n\t\tif (aliased(true)) {",
     "// read cursor, and the next match is found before its gap is moved.\n\t\tif (false) {"),
    ("M10 room already: the in-place condition is always true", "paths",
     "if (count <= replace_known_matches || str.capacity() >= size + count * growth) {",
     "if (true) {"),
    ("M11 join: last delimiter between every pair", "paths",
     "const auto& d = (i + 1 == count) ? last_delimiter : delimiter;\n\t\tresult.append",
     "const auto& d = last_delimiter;\n\t\tresult.append"),
    ("M20 short strings: the replace loop resumes one byte late", "paths",
     "pos = str.find(search, pos + replacement_size);\n\t\t} while (pos != std::string::npos);\n\t\treturn;\n\t}\n\tconst auto second",
     "pos = str.find(search, pos + replacement_size + 1);\n\t\t} while (pos != std::string::npos);\n\t\treturn;\n\t}\n\tconst auto second"),
    ("M26 out-of-line replace of a short string: the search after the handed-on matches starts one byte late", "paths",
     "pos = str.find(search, found[n - 1] + (n - 1) * growth + replacement_size);",
     "pos = str.find(search, found[n - 1] + (n - 1) * growth + replacement_size + 1);"),
    ("M27 two matches: the second is replaced at its old offset", "paths",
     "str.replace(pos, search_size, replacement);\n\t\tstr.replace(second + growth, search_size, replacement);\n\t\treturn;\n\t}\n\tconst auto fourth",
     "str.replace(pos, search_size, replacement);\n\t\tstr.replace(second, search_size, replacement);\n\t\treturn;\n\t}\n\tconst auto fourth"),
    ("M28 three matches: the third moves by one growth, not two", "paths",
     "str.replace(third + 2 * growth, search_size, replacement);",
     "str.replace(third + growth, search_size, replacement);"),
    ("M29 three matches: a fourth right behind the third is missed", "paths",
     "const auto fourth = str.find(search, third + search_size);",
     "const auto fourth = str.find(search, third + search_size + 1);"),
    ("M30 few matches: the alias guard is gone", "paths",
     " || detail::replace_aliased(str, search, replacement)) {",
     ") {"),
    ("M31 more than three matches: the fourth is handed on one byte late", "paths",
     "const std::size_t found[4] = {pos, second, third, fourth};",
     "const std::size_t found[4] = {pos, second, third, fourth + 1};"),
    ("M21 case table: an upper-case letter keeps its case in lower()", "paths",
     "lower[c] = static_cast<char>(c + 32);",
     "lower[c] = static_cast<char>(c + (c == 'Q' ? 0 : 32));"),
    ("M12 short join: last delimiter between every pair", "paths",
     "const auto& between = (i + 1 == count) ? last_delimiter : delimiter;",
     "const auto& between = last_delimiter;"),
    ("M13 lower(string_view): drops the last byte", "diff",
     "detail::ascii_lower(result.data(), str.data(), str.size());",
     "detail::ascii_lower(result.data(), str.data(), str.size() ? str.size() - 1 : 0);"),
    ("M14 case word: bytes >= 0x80 are mapped too", "paths",
     " & static_cast<W>(~x) & ",
     " & "),
    ("M15 case word: the second load is one byte off", "paths",
     "std::memcpy(&tail, s + n - 8, 8);",
     "std::memcpy(&tail, s + n - 7, 8);"),
    ("M22 case word in place: the second load is one byte off", "paths",
     "std::memcpy(&tail, p + n - 8, 8);",
     "std::memcpy(&tail, p + n - 7, 8);"),
    ("M16 case, below 8 bytes: the last byte is not mapped", "paths",
     "std::transform(s, s + n, d, [](char c) { return case_table_byte<Lower>(c); });",
     "std::transform(s, s + n - 1, d, [](char c) { return case_table_byte<Lower>(c); });"),
    ("M18 small copy: the second word is one byte off", "paths",
     "std::memcpy(&tail, from + n - 8, 8);",
     "std::memcpy(&tail, from + n - 7, 8);"),
    ("M19 short join of string_views: the first view is the last one", "paths",
     "\t\tjoin_append<T>(result, values[0]);\n\t\tfor (std::size_t i = 1; i < count; ++i) {\n\t\t\tconst auto& between",
     "\t\tjoin_append<T>(result, values[count - 1]);\n\t\tfor (std::size_t i = 1; i < count; ++i) {\n\t\t\tconst auto& between"),
    ("M17 case, below 8 bytes of a string_view: pushed unmapped", "paths",
     "result.push_back(case_table_byte<Lower>(c));",
     "result.push_back(c);"),
    ("M23 short join of strings: the last delimiter is the plain one", "paths",
     "\t\tresult.append(last_delimiter.data(), last_delimiter.size());\n\t\tresult.append(std::to_string(*it));",
     "\t\tresult.append(delimiter.data(), delimiter.size());\n\t\tresult.append(std::to_string(*it));"),
    ("M33 short join of strings: the middle delimiter is the last one", "paths",
     "\tfor (; it != it_l; ++it) {\n\t\tresult.append(delimiter.data(), delimiter.size());",
     "\tfor (; it != it_l; ++it) {\n\t\tresult.append(last_delimiter.data(), last_delimiter.size());"),
    ("M34 short join of strings: the first string loses its first byte", "paths",
     "result.append(std::to_string(*it++));",
     "result.append(std::to_string(*it++), 1);"),
    ("M24 inplace_lower: maps to upper", "paths",
     "detail::ascii_case_in_place<true>(str);",
     "detail::ascii_case_in_place<false>(str);"),
    ("M25 inplace_upper: maps to lower", "paths",
     "detail::ascii_case_in_place<false>(str);\n}\n\n\ninline std::string\nupper(std::string&& str)",
     "detail::ascii_case_in_place<true>(str);\n}\n\n\ninline std::string\nupper(std::string&& str)"),
]

os.makedirs(out, exist_ok=True)
failed = 0
for name, which, old, new in MUTANTS:
    d = os.path.join(out, name.split()[0])
    os.makedirs(d, exist_ok=True)
    if src.count(old) < 1:
        print(f"SKIP {name}: pattern not found")
        failed += 1
        continue
    text = src.replace(old, new, 1)
    open(os.path.join(d, "strings.hh"), "w").write(text)
    test = "paths_test" if which == "paths" else "diff_test"
    exe = os.path.join(d, test)
    cc = [cxx, "-std=c++20", "-O1", "-I" + d, "-I" + os.path.join(repo, "test")] + inc + \
         [os.path.join(repo, "test", test + ".cc"), "-o", exe]
    b = subprocess.run(cc, capture_output=True, text=True, errors="replace")
    if b.returncode != 0:
        print(f"BUILD FAILED {name}\n{b.stderr[:400]}")
        failed += 1
        continue
    r = subprocess.run([exe], capture_output=True, text=True, errors="replace")
    killed = r.returncode != 0
    print(f"{'KILLED ' if killed else 'SURVIVED'} {name} (test {which}, rc={r.returncode})")
    if not killed:
        failed += 1
sys.exit(1 if failed else 0)
