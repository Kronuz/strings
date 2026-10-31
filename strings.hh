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
#include <ostream>            // for std::ostream
#include <string>             // for std::string
#include <string_view>        // for std::string_view
#include <tuple>              // for std::make_tuple, std::apply
#include <type_traits>        // for std::forward
#include <vector>             // for std::vector

#include "chars.hh"           // for chars::tolower
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


namespace strings {

namespace detail {

template <typename T>
inline constexpr bool join_is_string_like = std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;

// Integer types whose std::to_string result is the same text std::to_chars writes.
template <typename T>
inline constexpr bool join_is_integer = std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, wchar_t> &&
	!std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

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

}  // namespace detail


template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter)
{
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
		// Collect pointers instead of copying the elements.
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


inline std::string indent(std::string_view str, char sep, int level, bool indent_first=true) {
	std::string result;
	result.reserve(((indent_first ? 1 : 0) + std::count(str.begin(), str.end(), '\n')) * level);

	if (indent_first) {
		result.append(level, sep);
	}

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
		if (isupper(c) != 0) {
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

// ASCII case mapping with plain arithmetic instead of a table lookup, so the
// compiler turns the loop into vector code (SWAR/NEON/SSE). Bytes >= 0x80 and
// all non-letters are unchanged, like chars::tolower / chars::toupper.
inline void ascii_lower(char* d, const char* s, std::size_t n) noexcept {
	for (std::size_t i = 0; i < n; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		d[i] = static_cast<char>(c + (static_cast<unsigned char>(c - 'A') < 26 ? 32 : 0));
	}
}

inline void ascii_upper(char* d, const char* s, std::size_t n) noexcept {
	for (std::size_t i = 0; i < n; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		d[i] = static_cast<char>(c - (static_cast<unsigned char>(c - 'a') < 26 ? 32 : 0));
	}
}

}  // namespace detail


inline void
inplace_upper(std::string& str)
{
	detail::ascii_upper(str.data(), str.data(), str.size());
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
	std::string result(str);
	inplace_upper(result);
	return result;
}


inline void
inplace_lower(std::string& str)
{
	detail::ascii_lower(str.data(), str.data(), str.size());
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
	std::string result(str);
	inplace_lower(result);
	return result;
}


inline void
inplace_replace(std::string& str, std::string_view search, std::string_view replacement)
{
	auto pos = str.find(search);
	auto search_size = search.size();
	auto replacement_size = replacement.size();
	if (pos == std::string::npos) {
		return;
	}
	if (search_size != 0 && search_size != replacement_size) {
		// The size changes: replacing in place would shift the tail once per match.
		// With two or more matches, build the result in one pass instead.
		auto next = str.find(search, pos + search_size);
		if (next != std::string::npos) {
			std::string out;
			out.reserve(str.size() + (replacement_size > search_size ? (replacement_size - search_size) * 2 : 0));
			std::size_t last = 0;
			do {
				out.append(str, last, pos - last);
				out.append(replacement);
				last = pos + search_size;
				pos = next;
				next = pos != std::string::npos ? str.find(search, pos + search_size) : std::string::npos;
			} while (pos != std::string::npos);
			out.append(str, last, std::string::npos);
			str = std::move(out);
			return;
		}
	}
	while (pos != std::string::npos) {
		str.replace(pos, search_size, replacement);
		pos = str.find(search, pos + replacement_size);
	}
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
