cmake_minimum_required( VERSION 3.23 )
project( peglex )

add_subdirectory( peglex )
add_subdirectory( samples )

enable_testing()
add_subdirectory( tests )
