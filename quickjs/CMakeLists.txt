
option(JSX_SUPPORT "Enable JSX support" OFF)
option(STORAGE_SUPPORT "Enable persistent storage" OFF)
option(QJS_DEBUGGER_SUPPORT "Enable debugger support" OFF)
if(JSX_SUPPORT)
    message(STATUS "JSX support enabled")
endif()

if(STORAGE_SUPPORT)
    message(STATUS "Storage support enabled")
endif()
if(QJS_DEBUGGER_SUPPORT)
	message(STATUS "Debugger support enabled")
	endif()

set(FILES 
 "cutils.h"
		"cutils.c"
		"libregexp.c"
		"libunicode.c"
		"quickjs.c"
		"quickjs-libc.c"
		"libbf.c"
		"libregexp.h"
		"libregexp-opcode.h"
		"libunicode.h"
		"libunicode-table.h"
		"list.h"
		"include/quickjs.h"
		"quickjs-atom.h"
		"include/quickjs-libc.h"
		"quickjs-opcode.h"
		"quickjs-jsx.h"
)
message(STATUS "Files: ${FILES}")
if(SUPPROT_STORAGE)
file(GLOB_RECURSE FILES1 "storage/dybase/src/*.cpp")
file(GLOB_RECURSE FILES2 "storage/dybase/src/*.h")
file(GLOB_RECURSE FILES3 "storage/dybase/include/*.h")
set(FILES ${FILES}
"storage/quickjs-storage.c",
"storage/quickjs-storage.h",
${FILES1},
${FILES2},
${FILES3},
)
endif()


add_library(quickjs STATIC ${FILES})
target_include_directories(quickjs PUBLIC "include")
target_compile_definitions(quickjs PUBLIC
JS_STRICT_NAN_BOXING # this option enables x64 build on Windows/MSVC
CONFIG_BIGNUM
)
if(QJS_DEBUGGER_SUPPORT)
target_compile_definitions(quickjs PUBLIC CONFIG_DEBUGGER)
endif()
if(JSX_SUPPORT)
    target_compile_definitions(quickjs PUBLIC CONFIG_JSX)
    target_include_directories(quickjs PUBLIC "storage/dybase/include")
endif()
if(STORAGE_SUPPORT)
    target_compile_definitions(quickjs PUBLIC CONFIG_STORAGE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(quickjs PUBLIC DEBUG)
    endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(quickjs PUBLIC NDEBUG)
    endif()
# disable exceptions and RTTI for performance
#target_compile_options(quickjs PRIVATE
#    $<$<CXX_COMPILER_ID:MSVC>:/EHs-c- /GR->
#    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions -fno-rtti>
#)

add_compile_definitions(_AMD64_)


add_executable(quickjs_tests tests/test_eval.cpp)
target_link_libraries(quickjs_tests PUBLIC quickjs)

add_executable(qjsc qjsc.c)
target_link_libraries(qjsc PUBLIC quickjs)


add_executable(qjs "qjs.c"
		#"repl.js"
		"repl.c"
		#"qjscalc.js"
		"qjscalc.c"
)
target_link_libraries(qjs PUBLIC quickjs)


target_compile_definitions(qjs PUBLIC
JS_STRICT_NAN_BOXING # this option enables x64 build on Windows/MSVC
CONFIG_BIGNUM
)