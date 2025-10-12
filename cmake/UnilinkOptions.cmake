# Unilink build options
# This file centralizes all build options for better maintainability

# Build type options
option(UNILINK_BUILD_SHARED "Build shared library" ON)
option(UNILINK_BUILD_STATIC "Build static library" ON)
option(UNILINK_BUILD_EXAMPLES "Build examples" ON)
option(UNILINK_BUILD_TESTS "Build tests" ON)
option(UNILINK_BUILD_DOCS "Build documentation" ON)

# Feature options
option(UNILINK_ENABLE_CONFIG "Enable configuration management API" ON)
option(UNILINK_ENABLE_MEMORY_TRACKING "Enable memory tracking for debugging" ON)
option(UNILINK_ENABLE_PERFORMANCE_TESTS "Enable performance/benchmark tests" OFF)
option(UNILINK_ENABLE_SANITIZERS "Enable sanitizers in Debug builds" OFF)

# Installation options
option(UNILINK_ENABLE_INSTALL "Enable install/export targets" ON)
option(UNILINK_ENABLE_PKGCONFIG "Install pkg-config file" ON)
option(UNILINK_ENABLE_EXPORT_HEADER "Generate export header" ON)

# Compiler options
option(UNILINK_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(UNILINK_ENABLE_WERROR "Treat warnings as errors" OFF)
option(UNILINK_ENABLE_COVERAGE "Enable code coverage" OFF)

# Platform-specific options
option(UNILINK_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(UNILINK_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(UNILINK_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Performance options
option(UNILINK_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(UNILINK_ENABLE_PCH "Enable Precompiled Headers" OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Validate build type
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel")
if(NOT CMAKE_BUILD_TYPE IN_LIST CMAKE_CONFIGURATION_TYPES)
  message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}. "
          "Valid options are: ${CMAKE_CONFIGURATION_TYPES}")
endif()

# Set default C++ standard
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
