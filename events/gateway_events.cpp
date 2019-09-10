#define GATEWAY_EVENTS_DEFINE_STRINGS
#include "gateway_events.hpp"
#include <cstring>

namespace {
	uint64_t constexpr mix(char m, uint64_t s){
		return ((s<<7) + ~(s>>3)) + ~m;
	}
	uint64_t constexpr hash(const char *m){
		return (*m) ? mix(*m, hash(m+1)) : 0;
	}
}

events::eventCode events::strToCode(const char *str){
	switch(hash(str)){
#define E(x) case hash(#x): \
		return x;
#include "gateway_event_names.h"
#undef E
	default:
		return INVALID;
	}
}

events::eventCode events::strToCode_s(const char *str){
	auto e = events::strToCode(str);
	if(std::strcmp(str, events::eventStrs[e]) != 0)
		return INVALID;
	return e;
}

const char *events::codeToStr(events::eventCode e){
	if(e >= COUNT)
		throw std::out_of_range("invalid event code");
	return events::eventStrs[e];
}
