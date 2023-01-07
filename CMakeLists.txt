cmake_minimum_required(VERSION 3.11)

project(libGraphCpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchDependencies)
find_package(Eigen3 REQUIRED)

set(LIB_GRAPH_CPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${LIB_GRAPH_CPP_INCLUDE_DIR} ${EIGEN3_INCLUDE_DIR})

add_executable(${PROJECT_NAME}_bin app/main.cpp)
set_target_properties(${PROJECT_NAME}_bin PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
target_link_libraries(${PROJECT_NAME}_bin PUBLIC ${PROJECT_NAME} polyscope yaml-cpp::yaml-cpp)

enable_testing()
find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}_test_bin tests/graph_tests.cpp)
set_target_properties(${PROJECT_NAME}_test_bin PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
target_link_libraries(${PROJECT_NAME}_test_bin ${PROJECT_NAME} GTest::GTest yaml-cpp::yaml-cpp)
