
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories("${PROJECT_SOURCE_DIR}/include")
file(GLOB SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach(name ${SRCS})
    message(${name})
endforeach(name ${SRCS})
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -v -fsanitize=thread -stdlib=libc++ -std=c++20")
add_library(co_uring_srcs ${SRCS})
target_link_libraries(co_uring_srcs pthread atomic uring)