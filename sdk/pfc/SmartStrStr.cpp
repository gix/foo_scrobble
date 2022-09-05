﻿#include "pfc-lite.h"

#include "string-conv-lite.h"
#include "string_conv.h"
#include "SmartStrStr.h"
#include <algorithm>
#include "SmartStrStr-table.h"
#include "SmartStrStr-twoCharMappings.h"

SmartStrStr::SmartStrStr() {
	std::map<uint32_t, std::set<uint32_t> > substitutions;
	std::map<uint32_t, uint32_t > downconvert;

#if 1
	for (auto& walk : SmartStrStrTable) {
		downconvert[walk.from] = walk.to;
		substitutions[walk.from].insert(walk.to);
	}
#else
	for (uint32_t walk = 128; walk < 0x10000; ++walk) {
		uint32_t c = Transform(walk);
		if (c != walk) {
			downconvert[walk] = c;
			substitutions[walk].insert(c);
		}
	}
#endif

	for (uint32_t walk = 32; walk < 0x10000; ++walk) {
		auto lo = ToLower(walk);
		if (lo != walk) {
			auto & s = substitutions[walk]; s.insert(lo);

			auto iter = substitutions.find(lo);
			if (iter != substitutions.end()) {
				s.insert(iter->second.begin(), iter->second.end());
			}
		}
	}

	this->m_substitutions.initialize(std::move(substitutions));
	this->m_downconvert.initialize(std::move(downconvert));
	InitTwoCharMappings();
}

#ifdef _WIN32
const wchar_t * SmartStrStr::matchHereW(const wchar_t * pString, const wchar_t * pUserString) const {
	auto walkData = pString;
	auto walkUser = pUserString;
	for (;; ) {
		if (*walkUser == 0) return walkData;

		uint32_t cData, cUser;
		size_t dData = pfc::utf16_decode_char(walkData, &cData);
		size_t dUser = pfc::utf16_decode_char(walkUser, &cUser);
		if (dData == 0 || dUser == 0) return nullptr;

		if (cData != cUser) {
			bool gotMulti = false;
			{
				const char * cDataSubst = m_twoCharMappings.query(cData);
				if (cDataSubst != nullptr) {
					PFC_ASSERT(strlen(cDataSubst) == 2);
					if (matchOneChar(cUser, (uint32_t)cDataSubst[0])) {
						auto walkUser2 = walkUser + dUser;
						uint32_t cUser2;
						auto dUser2 = pfc::utf16_decode_char(walkUser2, &cUser2);
						if (matchOneChar(cUser2, (uint32_t)cDataSubst[1])) {
							gotMulti = true;
							dUser += dUser2;
						}
					}
				}
			}
			if (!gotMulti) {
				if (!matchOneChar(cUser, cData)) return nullptr;
			}
		}

		walkData += dData;
		walkUser += dUser;
	}
}
#endif // _WIN32

bool SmartStrStr::equals(const char * pString, const char * pUserString) const {
	auto p = matchHere(pString, pUserString);
	if ( p == nullptr ) return false;
	return *p == 0;
}

const char * SmartStrStr::matchHere(const char * pString, const char * pUserString) const {
	const char * walkData = pString;
	const char * walkUser = pUserString;
	for (;; ) {
		if (*walkUser == 0) return walkData;

		uint32_t cData, cUser;
		size_t dData = pfc::utf8_decode_char(walkData, cData);
		size_t dUser = pfc::utf8_decode_char(walkUser, cUser);
		if (dData == 0 || dUser == 0) return nullptr;

		if (cData != cUser) {
			bool gotMulti = false;
			{
				const char* cDataSubst = m_twoCharMappings.query(cData);
				if (cDataSubst != nullptr) {
					PFC_ASSERT(strlen(cDataSubst) == 2);
					if (matchOneChar(cUser, (uint32_t)cDataSubst[0])) {
						auto walkUser2 = walkUser + dUser;
						uint32_t cUser2;
						auto dUser2 = pfc::utf8_decode_char(walkUser2, cUser2);
						if (matchOneChar(cUser2, (uint32_t)cDataSubst[1])) {
							gotMulti = true;
							dUser += dUser2;
						}
					}
				}
			}
			if (!gotMulti) {
				if (!matchOneChar(cUser, cData)) return nullptr;
			}
		}

		walkData += dData;
		walkUser += dUser;
	}
}

const char * SmartStrStr::strStrEnd(const char * pString, const char * pSubString, size_t * outFoundAt) const {
	size_t walk = 0;
	for (;; ) {
		if (pString[walk] == 0) return nullptr;
		auto end = matchHere(pString+walk, pSubString);
		if (end != nullptr) {
			if ( outFoundAt != nullptr ) * outFoundAt = walk;
			return end;
		}

		size_t delta = pfc::utf8_char_len( pString + walk );
		if ( delta == 0 ) return nullptr;
		walk += delta;
	}
}

#ifdef _WIN32
const wchar_t * SmartStrStr::strStrEndW(const wchar_t * pString, const wchar_t * pSubString, size_t * outFoundAt) const {
	size_t walk = 0;
	for (;; ) {
		if (pString[walk] == 0) return nullptr;
		auto end = matchHereW(pString + walk, pSubString);
		if (end != nullptr) {
			if (outFoundAt != nullptr) * outFoundAt = walk;
			return end;
		}

		uint32_t dontcare;
		size_t delta = pfc::utf16_decode_char(pString + walk, & dontcare);
		if (delta == 0) return nullptr;
		walk += delta;
	}
}
#endif // _WIN32

bool SmartStrStr::matchOneChar(uint32_t cInput, uint32_t cData) const {
	if (cInput == cData) return true;
	auto v = m_substitutions.query_ptr(cData);
	if (v == nullptr) return false;
	return v->count(cInput) > 0;
}

pfc::string8 SmartStrStr::transformStr(const char* str) const {
	pfc::string8 ret; transformStrHere(ret, str); return ret;
}

void SmartStrStr::transformStrHere(pfc::string8& out, const char* in) const {
	out.prealloc(strlen(in));
	out.clear();
	for (;; ) {
		unsigned c;
		size_t d = pfc::utf8_decode_char(in, c);
		if (d == 0) break; 
		in += d;
		const char* alt = m_twoCharMappings.query(c);
		if (alt != nullptr) {
			out << alt; continue;
		}
		unsigned alt2 = m_downconvert.query(c);
		if (alt2 != 0) {
			out.add_char(alt2); continue;
		}
		out.add_char(c);
	}
}

#if 0 // Windows specific code
uint32_t SmartStrStr::Transform(uint32_t c) {
	wchar_t wide[2] = {}; char out[4] = {};
	pfc::utf16_encode_char(c, wide);
	BOOL fail = FALSE;
	if (WideCharToMultiByte(pfc::stringcvt::codepage_ascii, 0, wide, 2, out, 4, "?", &fail) > 0) {
		if (!fail) {
			if (out[0] > 0 && out[1] == 0) {
				c = out[0];
			}
		}
	}
	return c;
}
#endif

uint32_t SmartStrStr::ToLower(uint32_t c) {
	return pfc::charLower(c);
}

#if 0
// UNREALIBLE!
// It's 2022 and compilers still break Unicode at random without warnings
static std::map<uint32_t, const char* > makeTwoCharMappings() {
	std::map<uint32_t, const char* > twoCharMappings;
	auto ImportTwoCharMappings = [&](const wchar_t* list, const char* replacement) {
		PFC_ASSERT(strlen(replacement) == 2);
		for (const wchar_t* ptr = list; ; ) {
			unsigned c = *ptr++;
			if (c == 0) break;
			twoCharMappings[(uint32_t)c] = replacement;
			// pfc::outputDebugLine(pfc::format("{0x", pfc::format_hex(c,4), ", \"", replacement, "\"},"));
		}
	};

	ImportTwoCharMappings(L"ÆǢǼ", "AE");
	ImportTwoCharMappings(L"æǣǽ", "ae");
	ImportTwoCharMappings(L"Œ", "OE");
	ImportTwoCharMappings(L"œɶ", "oe");
	ImportTwoCharMappings(L"ǄǱ", "DZ");
	ImportTwoCharMappings(L"ǆǳʣʥ", "dz");
	ImportTwoCharMappings(L"ß", "ss");
	ImportTwoCharMappings(L"Ǉ", "LJ");
	ImportTwoCharMappings(L"ǈ", "Lj");
	ImportTwoCharMappings(L"ǉ", "lj");
	ImportTwoCharMappings(L"Ǌ", "NJ");
	ImportTwoCharMappings(L"ǋ", "Nj");
	ImportTwoCharMappings(L"ǌ", "nj");
	ImportTwoCharMappings(L"Ĳ", "IJ");
	ImportTwoCharMappings(L"ĳ", "ij");

	return twoCharMappings;
}
#else
static std::map<uint32_t, const char*> makeTwoCharMappings() {
	std::map<uint32_t, const char* > ret;
	for (auto& walk : twoCharMappings) {
		ret[walk.from] = walk.to;
	}
	return ret;
}
#endif


void SmartStrStr::InitTwoCharMappings() {
	m_twoCharMappings.initialize(makeTwoCharMappings());
}

SmartStrStr& SmartStrStr::global() {
	static SmartStrStr g;
	return g;
}


void SmartStrFilter::init(const char* ptr, size_t len) {
	pfc::string_formatter current, temp;
	bool inQuotation = false;

	auto addCurrent = [&] {
		if (!current.is_empty()) {
			++m_items[current.get_ptr()]; current.reset();
		}
	};

	for (t_size walk = 0; walk < len; ++walk) {
		const char c = ptr[walk];
		if (c == '\"') inQuotation = !inQuotation;
		else if (!inQuotation && is_spacing(c)) {
			addCurrent();
		} else {
			current.add_byte(c);
		}
	}
	if (inQuotation) {
		// Allow unbalanced quotes, take the whole string *with* quotation marks
		m_items.clear();
		current.set_string_nc(ptr, len);
	}

	addCurrent();
}


bool SmartStrFilter::test_disregardCounts(const char* src) const {
	if (m_items.size() == 0) return false;

	auto& dc = SmartStrStr::global();

	for (auto& walk : m_items) {
		if (!dc.strStrEnd(src, walk.first.c_str())) return false;
	}
	return true;
}

bool SmartStrFilter::test(const char* src) const {

	if (m_items.size() == 0) return false;

	auto& dc = SmartStrStr::global();

	for (auto & walk : m_items) {
		const t_size count = walk.second;
		const std::string& str = walk.first;
		const char* strWalk = src;
		for (t_size walk = 0; walk < count; ++walk) {
			const char* next = dc.strStrEnd(strWalk, str.c_str());
			if (next == nullptr) return false;
			strWalk = next;
		}
	}
	return true;
}
