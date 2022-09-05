#pragma once


#include <set>
#include <map>
#include <string>
#include <vector>
#include "fixed_map.h"

//! Implementation of string matching for search purposes, such as media library search or typefind in list views. \n
//! Inspired by Unicode asymetic search, but not strictly implementing the Unicode asymetric search specifications. \n
//! Bootstraps its character mapping data from various Win32 API methods, requires no externally provided character mapping data. \n
//! Windows-only code. \n
//! \n
//! Keeping a global instance of it is recommended, due to one time init overhead. \n
//! Thread safety: safe to call concurrently once constructed.

class SmartStrStr {
public:
	SmartStrStr();

	//! Returns ptr to the end of the string if positive (for continuing search), nullptr if negative.
	const char * strStrEnd(const char * pString, const char * pSubString, size_t * outFoundAt = nullptr) const;
#ifdef _WIN32
	const wchar_t * strStrEndW(const wchar_t * pString, const wchar_t * pSubString, size_t * outFoundAt = nullptr) const;
#endif
	//! Returns ptr to the end of the string if positive (for continuing search), nullptr if negative.
	const char * matchHere(const char * pString, const char * pUserString) const;
#ifdef _WIN32
	const wchar_t * matchHereW( const wchar_t * pString, const wchar_t * pUserString) const;
#endif

	//! String-equals tool, compares strings rather than searching for occurance
	bool equals( const char * pString, const char * pUserString) const;

	//! One-char match. Doesn't use twoCharMappings, use only if you have to operate on char by char basis rather than call the other methods.
	bool matchOneChar(uint32_t cInput, uint32_t cData) const;

	static SmartStrStr& global();

	pfc::string8 transformStr(const char * str) const;
	void transformStrHere(pfc::string8& out, const char* in) const;
private:

	static uint32_t Transform(uint32_t c);
	static uint32_t ToLower(uint32_t c);

	void InitTwoCharMappings();

	fixed_map< uint32_t, uint32_t > m_downconvert;
	fixed_map< uint32_t, std::set<uint32_t> > m_substitutions;
	
	
	fixed_map<uint32_t, const char* > m_twoCharMappings;
};


class SmartStrFilter {
	typedef std::map<std::string, t_size> t_stringlist;
	t_stringlist m_items;

public:
	SmartStrFilter() { }
	SmartStrFilter(const char* p) { init(p, strlen(p)); }
	SmartStrFilter(const char* p, size_t l) { init(p, l); }

	static bool is_spacing(char c) { return c == ' ' || c == 10 || c == 13 || c == '\t'; }

	void init(const char* ptr, size_t len);
	bool test(const char* src) const;
	bool test_disregardCounts(const char* src) const;
};
