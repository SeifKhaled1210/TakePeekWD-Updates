cmake_minimum_required(VERSION 3.20)
project(TakePeekWD VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(OUTPUT_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BUILD_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_BUILD_DIR})

foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} _CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG} ${OUTPUT_BUILD_DIR}/${cfg})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_CFG} ${OUTPUT_BUILD_DIR}/${cfg})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_CFG} ${OUTPUT_BUILD_DIR}/${cfg})
endforeach()

if(MSVC)
    add_compile_options(/W3 /MP /EHsc)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra)
endif()

# Reference shared deps from sibling TakePeek project
set(TAKEPEEK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../TakePeek\ 2.0\ Project)
set(IMGUI_DIR ${TAKEPEEK_ROOT}/Cheat/vendor/imgui)
option(DEV_AUTH_STUB "Build with developer auth stub (no auth)" OFF)
option(UNBRANDED "Build unbranded version" OFF)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
)

if(DEV_AUTH_STUB)
    message(STATUS "Building TakePeek WarDogs with DEV_AUTH_STUB")
    add_executable(TakePeekWD WIN32
        Overlay/src/main.cpp
        Auth/auth_stub.cpp
        ${IMGUI_SOURCES}
        resource.rc
    )
    target_compile_definitions(TakePeekWD PRIVATE DEV_AUTH_STUB)
else()
    message(STATUS "Building TakePeek WarDogs with production TpAuth")
    add_executable(TakePeekWD WIN32
        Overlay/src/main.cpp
        Auth/auth.cpp
        ${IMGUI_SOURCES}
        resource.rc
    )
endif()

if(UNBRANDED)
    message(STATUS "Building UNBRANDED version")
    target_compile_definitions(TakePeekWD PRIVATE UNBRANDED)
    # Swap resource file: remove branded, add unbranded
    get_target_property(_srcs TakePeekWD SOURCES)
    list(REMOVE_ITEM _srcs "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc" "resource.rc")
    set_target_properties(TakePeekWD PROPERTIES SOURCES "${_srcs}")
    target_sources(TakePeekWD PRIVATE resource_unbranded.rc)
    set_target_properties(TakePeekWD PROPERTIES OUTPUT_NAME "Overlay")
endif()

target_include_directories(TakePeekWD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Auth
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/kdmapper
    ${CMAKE_CURRENT_SOURCE_DIR}/kdmapper/include
)

target_link_libraries(TakePeekWD PRIVATE
    d3d11 dxgi dwmapi advapi32 Normaliz wldap32 crypt32 Ws2_32 ntdll winhttp bcrypt
)

if(NOT UNBRANDED)
    set_target_properties(TakePeekWD PROPERTIES OUTPUT_NAME "TakePeekWD")
endif()

if(MSVC)
    set_target_properties(TakePeekWD PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()
