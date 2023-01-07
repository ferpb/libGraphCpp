include(FetchContent)

# yaml-cpp

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 1b50109)
FetchContent_GetProperties(yaml-cpp)

if(NOT yaml-cpp_POPULATED)
    message(STATUS "Fetching yaml-cpp...")
    FetchContent_Populate(yaml-cpp)
    add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
endif()

# polyscope

FetchContent_Declare(polyscope
    GIT_REPOSITORY https://github.com/nmwsharp/polyscope.git
    GIT_TAG 56b82cd
)

FetchContent_GetProperties(polyscope)
if(NOT polyscope_POPULATED)
    message(STATUS "Fetching polyscope...")
    FetchContent_Populate(polyscope)
    add_subdirectory(${polyscope_SOURCE_DIR} ${polyscope_BINARY_DIR})
endif()
