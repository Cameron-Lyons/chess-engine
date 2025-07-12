cmake_minimum_required(VERSION 3.20)
project(ChessEngine 
    VERSION 1.0.0
    DESCRIPTION "A modern C++23 chess engine"
    LANGUAGES CXX)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -DDEBUG)
    endif()
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(src)

# Source files
set(CORE_SOURCES
    src/core/ChessBoard.cpp
    src/core/BitboardMoves.cpp
    src/search/ValidMoves.cpp
    src/utils/engine_globals.cpp
)

set(ENGINE_SOURCES
    src/search/search.cpp
    src/search/AdvancedSearch.cpp
    src/evaluation/Evaluation.cpp
    src/evaluation/EvaluationEnhanced.cpp
    src/evaluation/EvaluationTuning.cpp
    src/protocol/uci.cpp
    src/ai/NeuralNetwork.cpp
    src/ai/EndgameTablebase.cpp
)

set(ALL_SOURCES ${CORE_SOURCES} ${ENGINE_SOURCES})

# Main executable
add_executable(chess_engine
    src/main.cpp
    ${ALL_SOURCES}
)

target_link_libraries(chess_engine Threads::Threads)

# Test executables
set(TEST_NAMES
    test_parallel
    test_bitboard_moves
    test_comprehensive
    test_fen
    test_crash
    test_pawn
)

foreach(test_name ${TEST_NAMES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}.cpp")
        add_executable(${test_name}
            tests/${test_name}.cpp
            ${ALL_SOURCES}
        )
        target_link_libraries(${test_name} Threads::Threads)
    endif()
endforeach()

# Custom targets
add_custom_target(run
    COMMAND chess_engine
    DEPENDS chess_engine
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Enable testing
enable_testing()
foreach(test_name ${TEST_NAMES})
    if(TARGET ${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endforeach()

# Install targets
install(TARGETS chess_engine
    RUNTIME DESTINATION bin
)

# Package configuration
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack) 