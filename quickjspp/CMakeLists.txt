if(MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(MODULE_NAME quickjspp)
file(GLOB_RECURSE FILES
"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" 
"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(${MODULE_NAME} STATIC ${FILES})

target_include_directories(${MODULE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(${MODULE_NAME} PUBLIC quickjs)








