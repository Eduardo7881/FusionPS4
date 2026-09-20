cmake_minimum_required(VERSION 3.20)

project(FusionPS4
    VERSION 0.1.0
    DESCRIPTION "PlayStation 4 -> Linux x86_64 compatibility runtime"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

file(GLOB_RECURSE FUSIONPS4_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/*.cpp"
)

add_executable(fusionps4 ${FUSIONPS4_SOURCES})

target_include_directories(fusionps4 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${SDL2_INCLUDE_DIRS})

target_compile_options(fusionps4 PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(fusionps4 PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(fusionps4 PRIVATE Threads::Threads ${SDL2_LIBRARIES})

install(TARGETS fusionps4 RUNTIME DESTINATION bin)
