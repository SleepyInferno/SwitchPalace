cmake_minimum_required(VERSION 3.10)

set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/borealis/library)

# build options
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Dependencies
option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

# toolchain
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# project info
project(SwitchPalace)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")
set(PACKAGE_NAME "com.switchpalace.app")
set(PROJECT_AUTHOR "SwitchPalace Contributors")
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

add_subdirectory(lib/borealis/library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# setting src and include
list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

file(GLOB_RECURSE MAIN_SRC
    source/*.cpp
)

set(PLATFORM_OPTION)
if (PLATFORM_DESKTOP)
    # Desktop build for development/testing
elseif (PLATFORM_SWITCH)
    # Use our own switch_wrapper.c instead of the borealis default
    # Our wrapper does NOT initialize network services at startup (TRUST-03)
    list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/switch_wrapper.c)
endif ()

# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# building release file
if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        # Ensure shaders output directory exists before compiling
        file(MAKE_DIRECTORY "${PROJECT_RESOURCES}/shaders")
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "SwitchPalace" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_LIBRARY}/../resources ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources --icon=${CMAKE_SOURCE_DIR}/resources/icon.jpg
    )
elseif (PLATFORM_DESKTOP)
    add_custom_target(${PROJECT_NAME}.data
        COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
    if (NOT USE_LIBROMFS)
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
    endif ()
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE include ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})

if (PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE borealis -lzstd -lmbedcrypto ${APP_PLATFORM_LIB})
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
endif ()
