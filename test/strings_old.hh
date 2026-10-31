// Frozen copies of the pre-optimization join / upper / lower / replace, used as
// the oracle of the differential test. Do not edit.
#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "chars.hh"

namespace old_strings {

template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter)
{
	auto it = values.begin();
	auto it_e = values.end();

	auto rit = values.rbegin();
	auto rit_e = values.rend();
	if (rit != rit_e) ++rit;
	auto it_l = rit != rit_e ? rit.base() : it_e;

	std::string result;

	if (it != it_e) {
		result.append(std::to_string(*it++));
	}
	for (; it != it_l; ++it) {
		result.append(delimiter.data(), delimiter.size());
		result.append(std::to_string(*it));
	}
	if (it != it_e) {
		result.append(last_delimiter.data(), last_delimiter.size());
		result.append(std::to_string(*it));
	}

	return result;
}


template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter) {
	return join(values, delimiter, delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter, UnaryPredicate pred) {
	std::vector<T> filtered_values(values.size());
	filtered_values.erase(std::remove_copy_if(values.begin(), values.end(), filtered_values.begin(), pred), filtered_values.end());
	return join(filtered_values, delimiter, last_delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, UnaryPredicate pred) {
	return join(values, delimiter, delimiter, pred);
}


inline void
inplace_upper(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), chars::toupper);
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
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), chars::toupper);
	return result;
}


inline void
inplace_lower(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), chars::tolower);
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
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), chars::tolower);
	return result;
}


inline void
inplace_replace(std::string& str, std::string_view search, std::string_view replacement)
{
	auto pos = str.find(search);
	auto search_size = search.size();
	auto replacement_size = replacement.size();
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



}  // namespace old_strings
