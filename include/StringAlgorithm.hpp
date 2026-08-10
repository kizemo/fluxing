#pragma once
#include <algorithm>
#include <locale>
#include <cwctype>
#include <numeric>
#include <set>
#include <string>
#include <vector>

inline bool ends_with(const std::wstring& wstr, const std::wstring& wsub)
{
	if (wstr.size() < wsub.size())
		return false;
	else
		return std::equal(wsub.rbegin(), wsub.rend(), wstr.rbegin());
}

inline bool iequals(const std::wstring& str1, const std::wstring& str2)
{
	return std::equal(str1.begin(), str1.end(), str2.begin(), [](const wchar_t& wc1, const wchar_t& wc2)
	{
		return std::towlower(wc1) == std::towlower(wc2);
	});
}

inline void ireplace_last(std::wstring& input, const std::wstring& search, const std::wstring& sub)
{
	std::size_t pos = input.rfind(search);
	if (pos != std::wstring::npos)
		input.replace(pos, search.length(), sub);
}

inline std::string join(const std::set<std::string>& list, const std::string& delim)
{
	return std::accumulate(list.begin(), list.end(), std::string(), [&delim](std::string& str1, const std::string& str2)
	{
		return str1.empty() ? str2 : str1 + delim + str2;
	});
}

inline std::vector<std::wstring>& split(std::vector<std::wstring>& result, const std::wstring& input, const wchar_t* delim)
{
	result.clear();
	size_t current = 0;
	size_t next = std::wstring::npos;
	do
	{
		current = next + 1;
		next = input.find_first_of(delim, next + 1);
		result.push_back(input.substr(current, next - current));
	} while (next != std::wstring::npos);
	return result;
}

inline bool starts_with(const std::wstring& wstr, const std::wstring& wsub)
{
	if (wstr.size() < wsub.size())
		return false;
	else
		return std::equal(wsub.begin(), wsub.end(), wstr.begin());
}

inline void to_lower(std::wstring& wstr)
{
	// L##: setlocale(LC_ALL, "") 改为 std::locale("") —— 进程级 locale mutation违反 P2。
	// std::locale("") 等价于 setlocale(LC_ALL, "") 但**不**修改全局 locale (thread-safe)。
	// 用 std::ctype<wchar_t>::tolower 避免 MSVC std::towlower 不支持 2 参数。
	const std::locale loc("");
	const auto& facet = std::use_facet<std::ctype<wchar_t>>(loc);
	facet.tolower(&*wstr.begin(), &*wstr.end());
}
