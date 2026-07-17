/// @file
/// Symbol visibility for the xmole2 CFB library.

#pragma once

#if defined(_WIN32) && defined(XMOLE2_BUILD_SHARED)
#  if defined(XMOLE2_CFB_EXPORTS)
#    define XMOLE2_CFB_API __declspec(dllexport)
#  else
#    define XMOLE2_CFB_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(XMOLE2_BUILD_SHARED)
#  define XMOLE2_CFB_API __attribute__((visibility("default")))
#else
#  define XMOLE2_CFB_API
#endif
