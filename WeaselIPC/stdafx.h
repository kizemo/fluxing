// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>

#pragma warning(disable : 4819)
#pragma warning(disable : 4996)

#include <boost/interprocess/streams/bufferstream.hpp>
#ifndef WEASEL_IPC_NO_BOOST_ARCHIVE
// L142: the boost::archive headers declare an EXPLICIT SPECIALIZATION of
// codecvt_null<wchar_t> whose constructor, destructor and virtuals are
// defined in the COMPILED boost serialization library, not in the header. A
// translation unit that parses these headers therefore references that
// library, and the library NAME carries the boost version
// (libboost_serialization-vc143-mt-s-x32-1_83.lib). That is how the two IPC
// test projects ended up pinned to one developer's boost install and failed
// on the CI runner with
//   LINK : fatal error LNK1104: cannot open file
//          'libboost_wserialization-vc143-mt-s-x32-1_83.lib'
// The product build never notices: WeaselIPC compiles with stdafx.h as a
// PRECOMPILED HEADER, and a PCH does not carry the reference into the
// translation units. Verified with dumpbin /SYMBOLS - the product's
// PipeChannel.obj has no codecvt symbol at all, the test project's has four.
// The tests use no boost::archive at all, so they define
// WEASEL_IPC_NO_BOOST_ARCHIVE and compile the same source without the
// dependency. The product never defines it: the archive header still
// reaches every WeaselIPC translation unit through the PCH, unchanged.
#include <boost/archive/text_wiarchive.hpp>
#endif

#pragma warning(default: 4819)
#pragma warning(default: 4996)

#include <map>
#include <string>
#include <vector>
#include <sstream>

using boost::interprocess::wbufferstream;
