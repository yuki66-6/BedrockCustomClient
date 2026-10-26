cmake_minimum_required(VERSION 3.24)

project(BedrockCustomClient
    VERSION 1.0.0
    DESCRIPTION "Overlay ClickGUI/HUD non intrusif pour Minecraft Bedrock (Windows)"
    LANGUAGES C CXX)

if(NOT MSVC)
    message(FATAL_ERROR "BedrockCustomClient se compile avec MSVC (Visual Studio 2022).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Seule l'architecture x64 est prise en charge.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# CRT statique : l'exécutable distribué ne dépend pas du redistribuable VC++.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(BCC_BUILD_TESTS "Construire les tests unitaires" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)

# zlib officiel (madler/zlib 1.3.1) : décompression des sauvegardes de mondes Bedrock.
set(ZLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/zlib)
add_library(bcc_zlib STATIC
    ${ZLIB_DIR}/adler32.c ${ZLIB_DIR}/compress.c ${ZLIB_DIR}/crc32.c ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/infback.c ${ZLIB_DIR}/inffast.c ${ZLIB_DIR}/inflate.c ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/trees.c ${ZLIB_DIR}/uncompr.c ${ZLIB_DIR}/zutil.c)
target_include_directories(bcc_zlib PUBLIC ${ZLIB_DIR})
target_compile_definitions(bcc_zlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
target_compile_options(bcc_zlib PRIVATE /W0)

# Toutes les sources de l'application sauf le point d'entrée : elles forment une
# bibliothèque statique partagée par l'exécutable et les tests.
file(GLOB_RECURSE BCC_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM BCC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(bcc_lib STATIC ${BCC_SOURCES} ${IMGUI_SOURCES})
target_include_directories(bcc_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann/include)
target_compile_definitions(bcc_lib PUBLIC
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
    "IMGUI_USER_CONFIG=\"utils/ImGuiUserConfig.h\""
    "BCC_VERSION=\"${PROJECT_VERSION}\"")
target_compile_options(bcc_lib PUBLIC /utf-8 /permissive- /Zc:__cplusplus /EHsc /MP)
# Avertissements stricts uniquement sur notre code, pas sur les dépendances.
set_source_files_properties(${BCC_SOURCES} PROPERTIES COMPILE_OPTIONS "/W4;/wd4100")
target_link_libraries(bcc_lib PUBLIC bcc_zlib
    d3d11 dxgi dcomp dwmapi ws2_32 comdlg32 shell32 user32 gdi32 ole32 psapi)

add_executable(BedrockCustomClient WIN32 src/main.cpp assets/BedrockCustomClient.rc)
target_link_libraries(BedrockCustomClient PRIVATE bcc_lib)
set_source_files_properties(src/main.cpp PROPERTIES COMPILE_OPTIONS "/W4;/wd4100")

if(BCC_BUILD_TESTS)
    enable_testing()
    file(GLOB BCC_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    add_executable(bcc_tests ${BCC_TEST_SOURCES})
    target_include_directories(bcc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(bcc_tests PRIVATE bcc_lib)
    set_source_files_properties(${BCC_TEST_SOURCES} PROPERTIES COMPILE_OPTIONS "/W4;/wd4100")
    add_test(NAME unit_tests COMMAND bcc_tests)
endif()

# Paquet distribuable : exécutable + documentation + profil d'exemple.
install(TARGETS BedrockCustomClient RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)
install(DIRECTORY docs/ DESTINATION docs)
install(FILES configs/example-profile.json DESTINATION configs/examples)
install(FILES external/imgui/LICENSE.txt DESTINATION licenses RENAME imgui-LICENSE.txt)
install(FILES external/nlohmann/LICENSE.MIT DESTINATION licenses RENAME nlohmann-json-LICENSE.txt)
install(FILES external/zlib/LICENSE DESTINATION licenses RENAME zlib-LICENSE.txt)
install(DIRECTORY assets/map-extensions/ DESTINATION map-extensions)
