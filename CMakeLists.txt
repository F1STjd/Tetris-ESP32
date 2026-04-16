cmake_minimum_required(VERSION 4.0)
project(TETRIS-ESP32 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckCXXCompilerFlag)

# (1) Build from the root-level main.cpp.
set(SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

# (2) Tworzymy plik wykonywalny
add_executable(${PROJECT_NAME} ${SRCS})

# (3) Dołączamy katalog z nagłówkami (opcjonalnie, jeśli są)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# (4) Wymagamy standardu C++23
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# (5) Warning policy: define once in CMake, shared by all presets/configurations.
set(PROJECT_WARNING_FLAGS
	-Wpedantic
	-pedantic-errors
	-Wall
	-Wextra
	-Wfatal-errors
	-Wabi
	-Wdouble-promotion
	-Wnoexcept
	-Wdeprecated
	-Wctor-dtor-privacy
	-Wredundant-tags
	-Weffc++
	-Wstrict-null-sentinel
	-Wold-style-cast
	-Wsign-promo
	-Wmismatched-tags
	-Wextra-semi
	-Wsuggest-final-types
	-Wsuggest-final-methods
	-Wsuggest-override
	-Wnull-dereference
	-Wnrvo
	-Wuseless-cast
	-Wuninitialized
	-Wstrict-overflow=4
	-Wsuggest-attribute=pure
	-Wsuggest-attribute=const
	-Wsuggest-attribute=noreturn
	-Wmissing-noreturn
	-Wsuggest-attribute=malloc
	-Wsuggest-attribute=returns_nonnull
	-Wsuggest-attribute=format
	-Wmissing-format-attribute
	-Wsuggest-attribute=cold
	-Walloc-size
	-Walloc-zero
	-Warith-conversion
	-Wduplicated-branches
	-Wduplicated-cond
	-Wzero-as-null-pointer-constant
	-Wtrampolines
	-Wshadow
	-Wunsafe-loop-optimizations
	-Wcast-qual
	-Wcast-align
	-Wconversion
	-Wenum-conversion
	-Wlogical-op
	-Wno-pedantic-ms-format
  -fanalyzer
	-fmessage-length=100
	-fdiagnostics-color
  -fdiagnostics-urls=always
	-fdiagnostics-parseable-fixits
  -fdiagnostics-generate-patch
	-Werror
	-Wno-error=analyzer-symbol-too-complex
	-Wno-error=analyzer-too-complex
	-Wno-error=analyzer-exposure-through-uninit-copy
)

set(SUPPORTED_PROJECT_WARNING_FLAGS)
foreach(flag IN LISTS PROJECT_WARNING_FLAGS)
	string(MAKE_C_IDENTIFIER "${flag}" flag_id)
	check_cxx_compiler_flag("${flag}" "HAS_${flag_id}")
	if(HAS_${flag_id})
		list(APPEND SUPPORTED_PROJECT_WARNING_FLAGS "${flag}")
	endif()
endforeach()

target_compile_options(${PROJECT_NAME} PRIVATE ${SUPPORTED_PROJECT_WARNING_FLAGS})
