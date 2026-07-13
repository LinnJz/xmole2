/// @file
/// Symbol visibility for the xmole2 base library.

#pragma once

#if defined(_WIN32) && defined(XMOLE2_BUILD_SHARED)
#  if defined(XMOLE2_BASE_EXPORTS)
#    define XMOLE2_BASE_API __declspec(dllexport)
#  else
#    define XMOLE2_BASE_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(XMOLE2_BUILD_SHARED)
#  define XMOLE2_BASE_API __attribute__((visibility("default")))
#else
#  define XMOLE2_BASE_API
#endif

