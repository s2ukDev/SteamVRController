#pragma once

#include <openvr_driver.h>
#include <MinHook.h>

#include <sstream>
#include <wtypes.h>

// Fun fact: vr:VRDriverLog()->Log(buf) actually looks something like this in a real call:
// vr::IVRDriverLog* logger = vr::VRDriverLog();
// if (logger) {
// 	void** vtable = *(void***)(logger);
// 	((void(*)(vr::IVRDriverLog*, const char*))vtable[0])(logger, "message");
// }

// TODO: if possible: imgui logging window (if so, it will be on imgui branch)

inline void Log(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
#if defined(_MSC_VER)
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
#else
	vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
	va_end(ap);

	vr::VRDriverLog()->Log(buf);
}

#ifndef LOG
#define LOG(...) Log(__VA_ARGS__)
#endif