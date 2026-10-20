cmake_minimum_required(VERSION 3.16)
project(tap-client LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(BUILD_CLI          "Compile client with Ncurses"  ON)
option(BUILD_GUI          "Compile client with Qt"      ON)
option(WARNINGS_AS_ERRORS "Adds -Werror"           ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)               # -std=c++20 instead of -std=gnu++20

add_compile_options(-Wall -Wextra)
if(WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
endif()

# ---------------------------------------------------------------------------
# paths
# ---------------------------------------------------------------------------
set(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared")
set(CLI_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/cli")
set(GUI_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/gui")
set(LOG_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/logs")

set(DEFAULT_ARGS -i 127.0.0.1 -p 4242)

file(MAKE_DIRECTORY "${LOG_DIR}")

# ---------------------------------------------------------------------------
# Shared lib (../shared e shared/)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS "${SHARED_DIR}/source/*.cpp")

add_library(TapShared STATIC ${SHARED_SOURCES})
target_include_directories(TapShared PUBLIC "${SHARED_DIR}/include")

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
if(BUILD_CLI)
    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)

    file(GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS "${CLI_DIR}/source/*.cpp")

    add_executable(tap-client-cli ${CLI_SOURCES})
    target_include_directories(tap-client-cli PRIVATE
        "${CLI_DIR}/include"
        ${CURSES_INCLUDE_DIRS}
    )
    target_link_libraries(tap-client-cli PRIVATE TapShared ${CURSES_LIBRARIES})

    add_custom_target(run-cli
        COMMAND tap-client-cli ${DEFAULT_ARGS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()

# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------
if(BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS "${GUI_DIR}/source/*.cpp")
    file(GLOB_RECURSE GUI_HEADERS CONFIGURE_DEPENDS "${GUI_DIR}/include/*.hpp")

    add_executable(tap-client-gui ${GUI_SOURCES} ${GUI_HEADERS})
    set_target_properties(tap-client-gui PROPERTIES AUTOMOC ON)   # sostituisce le regole moc a mano
    target_include_directories(tap-client-gui PRIVATE "${GUI_DIR}/include")
    target_link_libraries(tap-client-gui PRIVATE TapShared Qt6::Widgets)

    add_custom_target(run-gui
        COMMAND tap-client-gui ${DEFAULT_ARGS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()
