// stdafx.h : include file for standard system include files.
#pragma once

// windows.h ships min/max macros that collide with std::min / std::max
// (C2589). The test does not use them today, but keeping the macros out
// avoids the trap for whoever extends it next.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
