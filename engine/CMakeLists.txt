cmake_minimum_required(VERSION 3.16)
project(custom_chess_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_library(engine_core
  src/board.cpp
  src/engine.cpp
  src/evaluation.cpp
  src/opening_book.cpp
  src/search.cpp
)
target_include_directories(engine_core PUBLIC src)
target_compile_options(engine_core PRIVATE
  $<$<AND:$<CONFIG:Release>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>,$<NOT:$<BOOL:${EMSCRIPTEN}>>>:-march=native>
)

add_executable(chess_engine
  src/main.cpp
)
target_link_libraries(chess_engine PRIVATE engine_core)

add_executable(engine_tests
  tests/engine_tests.cpp
)
target_link_libraries(engine_tests PRIVATE engine_core)

add_executable(engine_bench
  tests/engine_bench.cpp
)
target_link_libraries(engine_bench PRIVATE engine_core)

if(EMSCRIPTEN)
  add_executable(chess_engine_wasm src/wasm_api.cpp)
  target_link_libraries(chess_engine_wasm PRIVATE engine_core)
  set_target_properties(chess_engine_wasm PROPERTIES OUTPUT_NAME chess_engine)
  target_link_options(chess_engine_wasm PRIVATE
    "-O3"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createChessEngine"
    "-sEXPORTED_FUNCTIONS=['_engine_best_move','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    "-sENVIRONMENT=web,worker"
    "-sFILESYSTEM=0"
    "-sNO_EXIT_RUNTIME=1"
  )
endif()
