cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME "uninstaller")

# --- [1] Toolchain Setup (Keep as you have it) ---
set(TOOLCHAIN_BIN_DIR "D:/MainAppx/llvm/llvm-mingw-20260616-ucrt-x86_64/bin")
#set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN_DIR}/clang++.exe")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

project(${PROJECT_NAME} LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# --- [2] Find lrelease tool ---
find_program(LRELEASE_EXE lrelease HINTS "${Qt6_Core_DIR}/../bin")

add_subdirectory(api)

# --- [3] Generate .qm files from .ts (Fixed Version) ---

# 1. Find all .ts files using absolute paths to avoid ambiguity
file(GLOB TS_FILES CONFIGURE_PATHES "assets/i18n/*.ts")

# Note: If your CMake version doesn't support CONFIGURE_PATHES, use this instead:
file(GLOB TS_FILES "assets/i18n/*.ts")

set(GENERATED_QM_FILES "")

foreach(TS_FILE ${TS_FILES})
    # We must work with absolute paths so Ninja knows exactly which file is being made
    get_filename_component(ABS_TS "${TS_FILE}" REALPATH)

    # Get the directory and filename without extension
    get_filename_component(DIR_NAME "${ABS_TS}" DIRECTORY)
    get_filename_component(FILE_WE "${ABS_TS}" NAME_WE)

    # Define the output path as an absolute path
    set(OUT_QM "${DIR_NAME}/${FILE_WE}.qm")

    # Add the command
    add_custom_command(
            OUTPUT "${OUT_QM}"
            COMMAND ${LRELEASE_EXE} "${ABS_TS}"
            DEPENDS "${ABS_TS}"
            COMMENT "Compiling TS to QM: ${ABS_TS} -> ${OUT_QM}"
    )

    # Add the absolute path of the .qm to our list
    list(APPEND GENERATED_QM_FILES "${OUT_QM}")
endforeach()

message(NOTICE "-- Generated QM files (Absolute): ${GENERATED_QM_FILES}")

# --- [4] Main Executable ---
# Use ${RESOURCES_VAR} which contains the path to the generated .cpp file
add_executable(${PROJECT_NAME} main.cpp appicon.rc assets/res.qrc)

# --- [5] Linking & Windows Settings (Keep your existing logic) ---
target_link_libraries(${PROJECT_NAME} PRIVATE
        api
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        advapi32
        shell32
)

if(WIN32)
    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE)# /SUBSYSTEM:WINDOWS)
    else()
        target_link_options(${PROJECT_NAME} PRIVATE)# -mwindows)
    endif()
endif()

# --- [6] Deployment ---
find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_Core_DIR}/../bin")
if(WINDEPLOYQT)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT} --no-translations "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.exe"
            COMMENT "Deploying Qt runtime dependencies..."
    )
endif()