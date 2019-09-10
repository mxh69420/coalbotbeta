#ifndef GATEWAY_EVENTS
#define GATEWAY_EVENTS

#include <stdexcept>

namespace events {
	enum eventCode {
#define E(x) x,
#include "gateway_event_names.h"
#undef E
		COUNT,
		INVALID = COUNT
	};
#ifdef GATEWAY_EVENTS_DEFINE_STRINGS
	const char *eventStrs[] = {
#define E(x) #x,
#include "gateway_event_names.h"
#undef E
	};
#else
	extern const char **eventStrs;
#endif
	eventCode strToCode(const char *);
	const char *codeToStr(eventCode); //includes bounds checking

	eventCode strToCode_s(const char *); /*includes collision check,
		collision checks dont cause huge slowdown but usually
		not necessary, plus in my tests
		(gateway_event_name_tests.cpp) they do add a couple
		of milliseconds so if your program can handle it use
		strToCode instead of _s*/
}

#endif
