#pragma once

// Set by CMake
#ifndef NDEBUG
#define HELIUM_DEBUG 1
#else
#define HELIUM_DEBUG 0
#endif

#define HELIUM_MIXED_MODE

#if HELIUM_DEBUG
#define HELIUM_TRACE_GC 1
#define HELIUM_TRACE_VALUES 1
#else
#define HELIUM_TRACE_GC 0
#define HELIUM_TRACE_VALUES 0
#endif
