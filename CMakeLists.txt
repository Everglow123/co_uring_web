cmake_minimum_required(VERSION 3.0.0)
project(co_uring_http VERSION 0.1.0)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(${PROJECT_SOURCE_DIR}/src)
message(${PROJECT_SOURCE_DIR}/include)
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -v  -stdlib=libc++ -std=c++20")
add_executable(co_uring_echo echo.cpp)
add_executable(static_web_http static_web_http.cpp)

target_link_libraries(co_uring_echo co_uring_srcs)

target_link_libraries(static_web_http co_uring_srcs)