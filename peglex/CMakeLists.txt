add_library( peglex INTERFACE )
target_include_directories( peglex INTERFACE include )
target_compile_features( peglex INTERFACE cxx_std_20 )