// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

//DMA
#include "libs/vmmdll.h"

// add headers that you want to pre-compile here
#include "framework.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <filesystem>

enum class DmaLogLevel : uint8_t
{
	Error = 0,
	Warning = 1,
	Info = 2,
	Debug = 3,
	Silent = 255
};

void DmaSetLogLevel(DmaLogLevel level);
DmaLogLevel DmaGetLogLevel();
void DmaLogPrintf(const char* fmt, ...);
void DmaLogWPrintf(const wchar_t* fmt, ...);

#define LOG(fmt, ...) DmaLogPrintf(fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) DmaLogWPrintf(fmt, ##__VA_ARGS__)

#define THROW_EXCEPTION
#ifdef THROW_EXCEPTION
#define THROW(fmt, ...) do { \
    char _throw_buf[512]; \
    std::snprintf(_throw_buf, sizeof(_throw_buf), fmt, ##__VA_ARGS__); \
    throw std::runtime_error(_throw_buf); \
} while(0)
#endif

#endif //PCH_H
