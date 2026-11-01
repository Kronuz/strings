/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#pragma once

#include <algorithm>          // for std::count
#include <cmath>              // for std::isinf, std::isnan
#include <charconv>           // for std::to_chars
#include <chrono>             // for std::chrono
#include <cstdint>            // for std::uint32_t, std::uint64_t
#include <cstring>            // for std::memcpy, std::memmove
#include <functional>         // for std::less
#include <ostream>            // for std::ostream
#include <string>             // for std::string
#include <string_view>        // for std::string_view
#include <tuple>              // for std::make_tuple, std::apply
#include <type_traits>        // for std::forward
#include <vector>             // for std::vector

#include "chars.hh"           // kept: includers of strings.hh see chars::
#include <format>             // for std::vformat
#include "repr.hh"            // for repr
#include "split.h"            // for Split
#include "static_string.hh"   // for static_string


namespace std {
	inline auto& to_string(std::string& str) {
		return str;
	}

	inline const auto& to_string(const std::string& str) {
		return str;
	}

	template <typename T, int N>
	inline auto to_string(const T (&&s)[N]) {
		return std::string(s, N - 1);
	}

	inline auto to_string(const std::string_view& str) {
		return std::string(str);
	}

	template <std::size_t SN, typename ST>
	inline auto to_string(const static_string::static_string<SN, ST>& str) {
		return std::string(str.data(), str.size());
	}

	template <typename T, typename = std::enable_if_t<
		std::is_convertible<decltype(std::declval<T>().to_string()), std::string>::value
	>>
	inline auto to_string(const T& obj) {
		return obj.to_string();
	}
}


// overload << so that it's easy to convert to a string
template <typename T, typename = std::enable_if_t<
	std::is_convertible<decltype(std::declval<T>().to_string()), std::string>::value
>>
std::ostream& operator<<(std::ostream& os, const T& obj) {
	return os << obj.to_string();
}


// The case mapping of a few bytes must inline into its caller (a call costs more than the mapping), so the
// part for longer strings is kept out of line.
#if defined(__GNUC__) || defined(__clang__)
#define STRINGS_ALWAYS_INLINE inline __attribute__((always_inline))
#define STRINGS_NEVER_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define STRINGS_ALWAYS_INLINE __forceinline
#define STRINGS_NEVER_INLINE __declspec(noinline)
#else
#define STRINGS_ALWAYS_INLINE inline
#define STRINGS_NEVER_INLINE
#endif

namespace strings {

namespace detail {

template <typename T>
inline constexpr bool join_is_string_like = std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;

// Integer types whose std::to_string result is the same text std::to_chars writes.
template <typename T>
inline constexpr bool join_is_integer = std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, wchar_t> &&
	!std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

// Copies n <= 16 bytes with no library call: two (overlapping) word moves. Every byte is read
// before any is written, so the ranges may overlap.
inline void copy_small(char* to, const char* from, std::size_t n) noexcept {
	if (n >= 8) {
		std::uint64_t head, tail;
		std::memcpy(&head, from, 8);
		std::memcpy(&tail, from + n - 8, 8);
		std::memcpy(to, &head, 8);
		std::memcpy(to + n - 8, &tail, 8);
	} else if (n >= 4) {
		std::uint32_t head, tail;
		std::memcpy(&head, from, 4);
		std::memcpy(&tail, from + n - 4, 4);
		std::memcpy(to, &head, 4);
		std::memcpy(to + n - 4, &tail, 4);
	} else if (n >= 2) {
		std::uint16_t head, tail;
		std::memcpy(&head, from, 2);
		std::memcpy(&tail, from + n - 2, 2);
		std::memcpy(to, &head, 2);
		std::memcpy(to + n - 2, &tail, 2);
	} else if (n == 1) {
		*to = *from;
	}
}

// Copies `piece` to `out` and returns the end. Unlike a bare memcpy, an empty
// view with a null data pointer is fine, and a short piece costs no library call.
inline char* put(char* out, std::string_view piece) noexcept {
	if (piece.size() <= 16) {
		copy_small(out, piece.data(), piece.size());
	} else {
		std::memcpy(out, piece.data(), piece.size());
	}
	return out + piece.size();
}

// Moves `n` bytes from `from` down to `to` (the ranges may overlap) and returns the end.
inline char* shift(char* to, const char* from, std::size_t n) noexcept {
	if (n <= 16) {
		copy_small(to, from, n);
	} else if (to != from) {
		std::memmove(to, from, n);
	}
	return to + n;
}

// True when `piece` lies inside the buffer of `str`.
inline bool overlaps(const std::string& str, std::string_view piece) noexcept {
	const std::less<const char*> before;
	return !piece.empty() && before(piece.data(), str.data() + str.size()) && before(str.data(), piece.data() + piece.size());
}

// Appends the text of one element. Strings are appended directly, integers are
// written with to_chars (no temporary string), anything else goes through to_string.
template <typename T>
inline void join_append(std::string& result, const T& value) {
	if constexpr (join_is_string_like<T>) {
		result.append(std::string_view(value));
	} else if constexpr (join_is_integer<T>) {
		char buf[24];
		auto r = std::to_chars(buf, buf + sizeof(buf), value);
		result.append(buf, static_cast<std::size_t>(r.ptr - buf));
	} else {
		result.append(std::to_string(value));
	}
}

// Joins `count` elements, `get(i)` returning the i-th as a const T&.
// The result is reserved up front when the size is cheap to know (strings).
template <typename T, typename Get>
inline std::string join_impl(std::size_t count, Get&& get, std::string_view delimiter, std::string_view last_delimiter) {
	std::string result;
	if (count == 0) {
		return result;
	}
	if constexpr (join_is_string_like<T>) {
		std::size_t total = delimiter.size() * (count > 2 ? count - 2 : 0) + (count > 1 ? last_delimiter.size() : 0);
		for (std::size_t i = 0; i < count; ++i) {
			total += std::string_view(get(i)).size();
		}
		result.reserve(total);
	} else if constexpr (join_is_integer<T>) {
		result.reserve(count * (delimiter.size() + 3));
	}
	join_append<T>(result, get(0));
	for (std::size_t i = 1; i < count; ++i) {
		const auto& d = (i + 1 == count) ? last_delimiter : delimiter;
		result.append(d.data(), d.size());
		join_append<T>(result, get(i));
	}
	return result;
}


// A single string, or up to four with the first shorter than 16 bytes: the result is small or one
// copy, so sizing it before the appends costs more than the growth it can save.
template <typename T>
inline bool join_is_short(const std::vector<T>& values) {
	return values.size() <= 1 || (values.size() <= 4 && std::string_view(values[0]).size() < 16);
}

template <typename T>
inline std::string join_short(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter) {
	std::string result;
	const auto count = values.size();
	if (count != 0) {
		join_append<T>(result, values[0]);
		for (std::size_t i = 1; i < count; ++i) {
			const auto& between = (i + 1 == count) ? last_delimiter : delimiter;
			result.append(between.data(), between.size());
			join_append<T>(result, values[i]);
		}
	}
	return result;
}

}  // namespace detail


template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter)
{
	if constexpr (detail::join_is_string_like<T>) {
		if (detail::join_is_short(values)) {
			return detail::join_short(values, delimiter, last_delimiter);
		}
	}
	// vector<bool> yields its elements by value; every other type is read in place.
	using Ref = std::conditional_t<std::is_same_v<T, bool>, bool, const T&>;
	return detail::join_impl<T>(values.size(), [&](std::size_t i) -> Ref { return values[i]; }, delimiter, last_delimiter);
}


template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter) {
	return join(values, delimiter, delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter, UnaryPredicate pred) {
	// Elements for which pred is true are dropped.
	if constexpr (std::is_same_v<T, bool>) {
		// vector<bool> has no addressable elements: filter a copy.
		std::vector<T> filtered_values;
		for (bool value : values) {
			if (!pred(value)) {
				filtered_values.push_back(value);
			}
		}
		return join(filtered_values, delimiter, last_delimiter);
	} else {
		// Collect pointers to the kept elements; the elements are not copied.
		std::vector<const T*> kept;
		kept.reserve(values.size());
		for (const auto& value : values) {
			if (!pred(value)) {
				kept.push_back(&value);
			}
		}
		return detail::join_impl<T>(kept.size(), [&](std::size_t i) -> const T& { return *kept[i]; }, delimiter, last_delimiter);
	}
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, UnaryPredicate pred) {
	return join(values, delimiter, delimiter, pred);
}


template <typename S, typename T>
inline std::vector<S> split(const S& value, const T& sep) {
	std::vector<S> values;
	Split<S, T>::split(value, sep, std::back_inserter(values));
	return values;
}


namespace detail {

// std::format normalization for format() arguments.
//
// Some libc++ versions ship an incomplete <format> that does NOT advertise
// __cpp_lib_format (e.g. the macOS 14 and FreeBSD 14 SDKs). Those builds reject
// user-defined std::formatter specializations when std::make_format_args builds
// its argument store, so a type that is only formattable through a custom
// formatter fails to compile there. static_string is exactly such a type: it
// converts implicitly to std::string_view, but std::format never uses an
// implicit conversion, so it needs either a formatter (unavailable on those
// libc++) or an explicit conversion here. Normalize static_string to a natively
// formattable std::string_view; every other argument passes through untouched.
//
// This must catch static_string in every value category (a non-const rvalue is
// the common case: a concatenated color constant). A pair of overloads does not
// work -- for a non-const rvalue the forwarding-reference overload binds better
// than a `const static_string&` one and wins -- so detect the type with a trait
// and branch with `if constexpr` instead.
template <typename T>
struct is_static_string : std::false_type {};

template <std::size_t N, typename U>
struct is_static_string<static_string::static_string<N, U>> : std::true_type {};

template <typename T>
constexpr decltype(auto) normalize_format_arg(T&& value) noexcept {
	if constexpr (is_static_string<std::remove_cvref_t<T>>::value) {
		return std::string_view(value.data(), value.size());
	} else {
		return std::forward<T>(value);
	}
}

}  // namespace detail


template <typename... Args>
inline std::string format(std::string_view format, Args&&... args) {
	std::string str;
	try {
		// Hold the normalized arguments as named lvalues before handing them to
		// std::make_format_args: it takes its arguments by (non-const, since
		// LWG3810) lvalue reference, so a normalized temporary -- the string_view
		// produced for a static_string -- must live in the tuple across the call.
		auto store = std::make_tuple(detail::normalize_format_arg(std::forward<Args>(args))...);
		str = std::apply([&](auto&... a) {
			return std::vformat(format, std::make_format_args(a...));
		}, store);
	} catch(...) {
		// A standalone string library must not log: on a format error fall
		// back silently to the raw, unformatted format string.
		str = format;
	}
	return str;
}


// Prefix each line of `str` with `level` copies of `sep`, like Python's
// textwrap.indent: lines are split on '\n', a line that is empty or only
// whitespace (space, \t, \r, \v, \f) is kept as it is without the prefix, and
// the text between and after the lines, including a trailing '\n', is preserved.
// With indent_first == false the first line is not prefixed. `level <= 0` adds
// nothing.
inline std::string indent(std::string_view str, char sep, int level, bool indent_first=true) {
	const std::size_t pad = level > 0 ? static_cast<std::size_t>(level) : 0;
	std::string result;
	result.reserve(str.size() + (static_cast<std::size_t>(std::count(str.begin(), str.end(), '\n')) + 1) * pad);

	bool first = true;
	std::size_t pos = 0;
	while (pos < str.size()) {
		auto nl = str.find('\n', pos);
		auto end = nl == std::string_view::npos ? str.size() : nl;
		auto line = str.substr(pos, end - pos);
		if (!(first && !indent_first) && line.find_first_not_of(" \t\r\v\f") != std::string_view::npos) {
			result.append(pad, sep);
		}
		result.append(line);
		if (nl == std::string_view::npos) {
			break;
		}
		result.push_back('\n');
		pos = nl + 1;
		first = false;
	}

	return result;
}


inline std::string left(std::string_view str, int width, bool fill = false) {
	std::string result;
	result.append(str.data(), str.size());
	if (fill) {
		for (auto idx = int(width - str.size()); idx > 0; --idx) {
			result += " ";
		}
	}
	return result;
}


inline std::string center(std::string_view str, int width, bool fill = false) {
	std::string result;
	auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2);
	width -= idx;
	width -= str.size();
	for (; idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
	if (fill) {
		for (; width > 0; --width) {
			result += " ";
		}
	}
	return result;
}


inline std::string right(std::string_view str, int width) {
	std::string result;
	for (auto idx = int(width - str.size()); idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
	return result;
}


inline bool startswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && text.compare(0, token.size(), token) == 0;
}


inline bool startswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(0) == ch;
}


inline bool hasupper(std::string_view str) {
	for (const auto& c : str) {
		// ASCII only, like upper() / lower(): independent of the C locale.
		if (static_cast<unsigned char>(c - 'A') < 26) {
			return true;
		}
	}

	return false;
}


inline bool endswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && std::equal(text.begin() + text.size() - token.size(), text.end(), token.begin());
}


inline bool endswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(text.size() - 1) == ch;
}


namespace detail {

// ASCII case mapping of up to W bytes at once (W an unsigned word type): a byte in
// 'A'..'Z' (Lower) or 'a'..'z' (not Lower) moves by 0x20, every other byte, including
// all bytes >= 0x80, stays. The marks are computed per byte lane without carries
// between lanes: the low seven bits are tested, bit 7 of the byte itself excludes >= 0x80.
template <bool Lower, typename W>
STRINGS_ALWAYS_INLINE W ascii_case_word(W x) noexcept {
	constexpr W ones = static_cast<W>(~W(0) / 255);
	constexpr unsigned first = Lower ? 'A' : 'a';
	const W low7 = x & static_cast<W>(ones * 0x7f);
	const W at_least_first = static_cast<W>(low7 + static_cast<W>(ones * (0x80 - first)));
	const W past_last = static_cast<W>(low7 + static_cast<W>(ones * (0x80 - first - 26)));
	const W letters = at_least_first & static_cast<W>(~past_last) & static_cast<W>(~x) & static_cast<W>(ones * 0x80);
	return Lower ? static_cast<W>(x + (letters >> 2)) : static_cast<W>(x - (letters >> 2));
}

template <bool Lower>
STRINGS_ALWAYS_INLINE char ascii_case_byte(char ch) noexcept {
	const unsigned char c = static_cast<unsigned char>(ch);
	if constexpr (Lower) {
		return static_cast<char>(c + (static_cast<unsigned char>(c - 'A') < 26 ? 32 : 0));
	} else {
		return static_cast<char>(c - (static_cast<unsigned char>(c - 'a') < 26 ? 32 : 0));
	}
}

// The two ASCII mappings as 256-byte tables, built once at compile time.
struct CaseTable {
	char lower[256];
	char upper[256];
	constexpr CaseTable() noexcept : lower(), upper() {
		for (int c = 0; c < 256; ++c) {
			lower[c] = upper[c] = static_cast<char>(c);
		}
		for (int c = 'A'; c <= 'Z'; ++c) {
			lower[c] = static_cast<char>(c + 32);
		}
		for (int c = 'a'; c <= 'z'; ++c) {
			upper[c] = static_cast<char>(c - 32);
		}
	}
};
inline constexpr CaseTable case_table{};

template <bool Lower>
STRINGS_ALWAYS_INLINE char case_table_byte(char c) noexcept {
	return (Lower ? case_table.lower : case_table.upper)[static_cast<unsigned char>(c)];
}

// Maps n >= 8 bytes from s to d (d == s is fine; other overlap is not): from 8 to 15 bytes two overlapping
// word operations (idempotent on the overlap); from 16 bytes on a plain loop that the compiler vectorizes.
template <bool Lower>
STRINGS_NEVER_INLINE void ascii_case_long(char* d, const char* s, std::size_t n) noexcept {
	if (n < 16) {
		std::uint64_t head, tail;
		std::memcpy(&head, s, 8);
		std::memcpy(&tail, s + n - 8, 8);
		head = ascii_case_word<Lower>(head);
		tail = ascii_case_word<Lower>(tail);
		std::memcpy(d, &head, 8);
		std::memcpy(d + n - 8, &tail, 8);
	} else {
		for (std::size_t i = 0; i < n; ++i) {
			d[i] = ascii_case_byte<Lower>(s[i]);
		}
	}
}

// The same for one string edited where it is. Kept apart from the two-pointer version: with one pointer the
// compiler knows the loop has no overlap to check, and vectorizes it; with two it checks at run time and a
// string that is its own source takes the scalar loop.
template <bool Lower>
STRINGS_NEVER_INLINE void ascii_case_long_in_place(char* p, std::size_t n) noexcept {
	if (n < 16) {
		std::uint64_t head, tail;
		std::memcpy(&head, p, 8);
		std::memcpy(&tail, p + n - 8, 8);
		head = ascii_case_word<Lower>(head);
		tail = ascii_case_word<Lower>(tail);
		std::memcpy(p, &head, 8);
		std::memcpy(p + n - 8, &tail, 8);
	} else {
		for (std::size_t i = 0; i < n; ++i) {
			p[i] = ascii_case_byte<Lower>(p[i]);
		}
	}
}

template <bool Lower>
STRINGS_ALWAYS_INLINE void ascii_case_in_place(char* p, std::size_t n) noexcept {
	if (n < 8) {
		std::transform(p, p + n, p, [](char c) { return case_table_byte<Lower>(c); });
	} else {
		ascii_case_long_in_place<Lower>(p, n);
	}
}

// Maps n bytes from s to d. Below 8 bytes one table lookup per byte, as the original did: a vector loop has
// set-up that a few bytes do not repay, and word moves do not beat a lookup this short.
template <bool Lower>
STRINGS_ALWAYS_INLINE void ascii_case(char* d, const char* s, std::size_t n) noexcept {
	if (n < 8) {
		std::transform(s, s + n, d, [](char c) { return case_table_byte<Lower>(c); });
	} else {
		ascii_case_long<Lower>(d, s, n);
	}
}

STRINGS_ALWAYS_INLINE void ascii_lower(char* d, const char* s, std::size_t n) noexcept {
	ascii_case<true>(d, s, n);
}

STRINGS_ALWAYS_INLINE void ascii_upper(char* d, const char* s, std::size_t n) noexcept {
	ascii_case<false>(d, s, n);
}

}  // namespace detail


inline void
inplace_upper(std::string& str)
{
	detail::ascii_case_in_place<false>(str.data(), str.size());
}


inline std::string
upper(std::string&& str)
{
	inplace_upper(str);
	return std::move(str);
}


inline std::string
upper(std::string_view str)
{
	// Up to seven bytes go one by one into the short-string buffer, with no set-up cost.
	std::string result(str.size() < 8 ? 0 : str.size(), '\0');
	if (str.size() < 8) {
		for (char c : str) {
			result.push_back(detail::case_table_byte<false>(c));
		}
		return result;
	}
	detail::ascii_upper(result.data(), str.data(), str.size());
	return result;
}


inline void
inplace_lower(std::string& str)
{
	detail::ascii_case_in_place<true>(str.data(), str.size());
}


inline std::string
lower(std::string&& str)
{
	inplace_lower(str);
	return std::move(str);
}


inline std::string
lower(std::string_view str)
{
	// Up to seven bytes go one by one into the short-string buffer, with no set-up cost.
	std::string result(str.size() < 8 ? 0 : str.size(), '\0');
	if (str.size() < 8) {
		for (char c : str) {
			result.push_back(detail::case_table_byte<true>(c));
		}
		return result;
	}
	detail::ascii_lower(result.data(), str.data(), str.size());
	return result;
}


// Matches that a growing inplace_replace records in an array on the stack; with more it uses a vector.
inline constexpr std::size_t replace_known_matches = 16;
// A string of at most this many bytes is edited with one replace call per match.
inline constexpr std::size_t replace_small_size = 32;

inline void
inplace_replace(std::string& str, std::string_view search, std::string_view replacement)
{
	const auto search_size = search.size();
	const auto replacement_size = replacement.size();
	if (search_size == 0) {
		// An empty search matches at every position, including both ends, as in
		// Python ("abc".replace("", "-") == "-a-b-c-").
		if (replacement_size != 0) {
			std::string out;
			out.reserve(str.size() + (str.size() + 1) * replacement_size);
			out.append(replacement);
			for (char c : str) {
				out.push_back(c);
				out.append(replacement);
			}
			str = std::move(out);
		}
		return;
	}
	auto pos = str.find(search);
	if (pos == std::string::npos) {
		return;
	}
	// An edit in place changes what a search or replacement viewing into str says, and a resize
	// can move it: such an edit copies the view first (the copy also does not alias).
	const auto aliased = [&](bool search_too) {
		return detail::overlaps(str, replacement) || (search_too && detail::overlaps(str, search));
	};
	const auto size = str.size();
	if (search_size == replacement_size) {
		if (aliased(true)) {
			const std::string search_copy(search), replacement_copy(replacement);
			inplace_replace(str, search_copy, replacement_copy);
			return;
		}
		do {
			detail::put(str.data() + pos, replacement);
			pos = str.find(search, pos + replacement_size);
		} while (pos != std::string::npos);
		return;
	}
	if (size <= replace_small_size) {
		// A short string: each replace moves a few bytes, so plain replace calls are the cheapest.
		if (aliased(true)) {
			const std::string search_copy(search), replacement_copy(replacement);
			inplace_replace(str, search_copy, replacement_copy);
			return;
		}
		do {
			str.replace(pos, search_size, replacement);
			pos = str.find(search, pos + replacement_size);
		} while (pos != std::string::npos);
		return;
	}
	auto next = str.find(search, pos + search_size);
	if (next == std::string::npos) {
		str.replace(pos, search_size, replacement);
		return;
	}
	if (replacement_size < search_size) {
		// Shrinking: close the gaps going forward in one pass. The write cursor never passes the
		// read cursor, and the next match is found before its gap is moved.
		if (aliased(true)) {
			const std::string search_copy(search), replacement_copy(replacement);
			inplace_replace(str, search_copy, replacement_copy);
			return;
		}
		char* data = str.data();
		std::size_t write = pos;
		std::size_t last;
		for (;;) {
			write = detail::put(data + write, replacement) - data;
			last = pos + search_size;
			pos = next;
			if (pos == std::string::npos) {
				break;
			}
			next = str.find(search, pos + search_size);
			write = detail::shift(data + write, data + last, pos - last) - data;
		}
		write = detail::shift(data + write, data + last, size - last) - data;
		str.resize(write);
		return;
	}
	// Growing: record the matches first (all searching happens before any edit).
	std::size_t known[replace_known_matches];
	std::size_t known_count = 2;
	known[0] = pos;
	known[1] = next;
	next = str.find(search, next + search_size);
	while (next != std::string::npos && known_count < replace_known_matches) {
		known[known_count++] = next;
		next = str.find(search, next + search_size);
	}
	std::vector<std::size_t> rest;
	if (next != std::string::npos) {
		// More matches than the array holds: record the rest, estimating how many there are from
		// the spacing of the first ones.
		rest.reserve(std::min((size - next) / search_size + 1,
			(size - next) * (replace_known_matches - 1) / std::max<std::size_t>(known[replace_known_matches - 1] - known[0], 1) + 16));
		for (; next != std::string::npos; next = str.find(search, next + search_size)) {
			rest.push_back(next);
		}
	}
	const auto count = known_count + rest.size();
	const auto match = [&](std::size_t k) { return k < known_count ? known[k] : rest[k - known_count]; };
	const auto growth = replacement_size - search_size;
	if (count <= replace_known_matches || str.capacity() >= size + count * growth) {
		// Few matches, or room already: edit in place. Two or three matches are as cheap as
		// replace calls (each match is at its offset plus the growth so far); more move each
		// tail once, last match first, so that no match moves the matches before it.
		if (aliased(false)) {
			const std::string replacement_copy(replacement);
			inplace_replace(str, search, replacement_copy);
			return;
		}
		if (count <= 3) {
			for (std::size_t k = 0; k < count; ++k) {
				str.replace(match(k) + k * growth, search_size, replacement);
			}
			return;
		}
		str.resize(size + count * growth);
		char* data = str.data();
		auto end = size;
		for (std::size_t k = count; k-- > 0;) {
			const auto from = match(k) + search_size;
			detail::shift(data + from + (k + 1) * growth, data + from, end - from);
			detail::put(data + match(k) + k * growth, replacement);
			end = match(k);
		}
		return;
	}
	// Many matches and no room: build the result in one buffer of the exact size.
	std::string out(size + count * growth, '\0');
	char* write = out.data();
	const char* data = str.data();
	std::size_t last = 0;
	for (std::size_t k = 0; k < count; ++k) {
		const auto at = match(k);
		write = detail::put(write, std::string_view(data + last, at - last));
		write = detail::put(write, replacement);
		last = at + search_size;
	}
	detail::put(write, std::string_view(data + last, size - last));
	str = std::move(out);
}


inline std::string
replace(std::string&& str, std::string_view search, std::string_view replacement)
{
	inplace_replace(str, search, replacement);
	return std::move(str);
}


inline std::string
replace(std::string_view str, std::string_view search, std::string_view replacement)
{
	return replace(std::string(str), search, replacement);
}


inline std::string_view
ltrim(std::string_view str)
{
	auto found = str.find_first_not_of(" \n\r\t\f\v");
	if (found == std::string_view::npos) {
		found = str.size();
	}
	str.remove_prefix(found);
	return str;
}


inline std::string_view
rtrim(std::string_view str)
{
	auto found = str.find_last_not_of(" \n\r\t\f\v");
	if (found == std::string_view::npos) {
		found = str.size();
	} else {
		found = str.size() - found - 1;
	}
	str.remove_suffix(found);
	return str;
}

inline std::string_view
trim(std::string_view str)
{
	return rtrim(ltrim(str));
}


std::string from_bytes(size_t bytes, const char* prefix = "", bool colored = false);
std::string from_small_time(long double seconds, const char* prefix = "", bool colored = false);
std::string from_time(long double seconds, const char* prefix = "", bool colored = false);
std::string from_delta(long double nanoseconds, const char* prefix = "", bool colored = false);

inline std::string
from_delta(const std::chrono::nanoseconds& nanoseconds, const char* prefix = "", bool colored = false) {
	return from_delta(nanoseconds.count(), prefix, colored);
}

template <typename T>
inline std::string
from_delta(const std::chrono::time_point<T>& start, const std::chrono::time_point<T>& end, const char* prefix = "", bool colored = false) {
	return from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), prefix, colored);
}

}  // namespace strings

#undef STRINGS_ALWAYS_INLINE
#undef STRINGS_NEVER_INLINE
