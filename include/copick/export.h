// Symbol-visibility macros for the copick shared/static library.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_EXPORT_H
#define COPICK_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef COPICK_BUILDING_LIBRARY
#define COPICK_API __declspec(dllexport)
#elif defined(COPICK_STATIC)
#define COPICK_API
#else
#define COPICK_API __declspec(dllimport)
#endif
#else
#if defined(COPICK_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define COPICK_API __attribute__((visibility("default")))
#else
#define COPICK_API
#endif
#endif

#endif  // COPICK_EXPORT_H
