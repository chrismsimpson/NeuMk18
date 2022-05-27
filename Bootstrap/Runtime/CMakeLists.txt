project (Runtime)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")

add_compile_options(-Wno-user-defined-literals)

add_library(runtime
    Format.cpp
    GenericLexer.cpp
    kmalloc.cpp
    StringBuilder.cpp
    StringImpl.cpp
    StringUtils.cpp
    StringView.cpp
    )
