cmake_minimum_required(VERSION 3.14)
project(strings LANGUAGES CXX)

include(FetchContent)

# --- Dependencies ---
# strings.hh / strings.cc pull in a handful of sibling headers from the rest of
# the family, each its own already-extracted standalone library. FetchContent
# pulls them in at their tips (GIT_TAG main) and puts their headers on the
# include path so the `#include`s in the sources resolve unchanged:
#   "chars.hh"          -> char-classify   (chars::toupper / chars::tolower for the case helpers)
#   "split.h"           -> split           (Split<>, used by split() and indent())
#   "static_string.hh"  -> static-string   (the to_string() overload for static_string)
#   "repr.hh"           -> repr            (kept from the original header)
#   "colors.h"          -> term-color      (the rgb()/CLEAR_COLOR palette the from_* humanizers color with)
FetchContent_Declare(
  char_classify
  GIT_REPOSITORY https://github.com/Kronuz/char-classify.git
  GIT_TAG        main
)
FetchContent_Declare(
  split
  GIT_REPOSITORY https://github.com/Kronuz/split.git
  GIT_TAG        main
)
FetchContent_Declare(
  static_string
  GIT_REPOSITORY https://github.com/Kronuz/static-string.git
  GIT_TAG        main
)
FetchContent_Declare(
  repr
  GIT_REPOSITORY https://github.com/Kronuz/repr.git
  GIT_TAG        main
)
FetchContent_Declare(
  term_color
  GIT_REPOSITORY https://github.com/Kronuz/term-color.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(char_classify split static_string repr term_color)

# --- Library target ---
# strings is not header-only: strings.cc carries the byte/time humanizers
# (from_bytes / from_time / from_small_time / from_delta and the Humanize
# helper), so it must be compiled and linked. The header (strings.hh) holds the
# inline/template utilities (join, split, format, indent, trim, case, etc.).
add_library(strings strings.cc)
target_include_directories(strings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(strings PUBLIC cxx_std_20)

# Link the five deps PUBLIC so their headers ride onto the consumer's include
# path: strings.hh is part of the public surface and itself includes chars.hh /
# split.h / static_string.hh / repr.hh, so a consumer of strings.hh needs all of
# those resolvable too. term-color (colors.h) is only used by strings.cc, but is
# linked PUBLIC for a consistent include path. Note: split exposes only the bare
# `split` target (no split::split alias), the others carry namespaced aliases.
target_link_libraries(strings PUBLIC
  char_classify::char_classify
  split
  static_string::static_string
  repr::repr
  term_color::term_color
)

# Alias so consumers can link `strings::strings` whether they add this via
# add_subdirectory or FetchContent.
add_library(strings::strings ALIAS strings)

# Only build the test when this repo is the top-level project, so consumers
# (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(strings_test test/test.cc)
  target_link_libraries(strings_test PRIVATE strings)
  add_test(NAME strings COMMAND strings_test)
endif()
