include(CTest)
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

set( TEST_SOURCES
    test_peglex.cpp
)

add_executable( tests ${TEST_SOURCES} )
target_compile_options( tests PRIVATE -fsanitize=address -fno-omit-frame-pointer )
target_link_libraries( tests PRIVATE peglex Catch2::Catch2WithMain )
target_link_options( tests PRIVATE -fsanitize=address )
catch_discover_tests( tests )