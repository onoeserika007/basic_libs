cmake_minimum_required(VERSION 3.10)
project(base_lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加jsoncpp作为子目录
add_subdirectory(third_party/jsoncpp)

# 获取源文件目录的绝对路径
get_filename_component(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)

aux_source_directory(${SRC_DIR} SRC_LIST)

# Build shared library
add_library(base_lib SHARED ${SRC_LIST})

# Make the include directory available to users of this library
target_include_directories(base_lib 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(base_lib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# Link with jsoncpp and pthread
target_link_libraries(base_lib 
    PUBLIC 
        jsoncpp_static 
        pthread
)