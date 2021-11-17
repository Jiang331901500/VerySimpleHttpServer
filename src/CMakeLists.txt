cmake_minimum_required(VERSION 2.8)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    message(STATUS "Release版本")
    set(BuildType "Release")
    set(CMAKE_C_FLAGS "$ENV{CFLAGS} -DNODEBUG -O3 -Wall")
    set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -DNODEBUG -O3 -Wall")
else()
    message(STATUS "Debug版本")
    set(BuildType "Debug")
    set(CMAKE_C_FLAGS "$ENV{CFLAGS} -O0 -g -Wall")
    set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -O0 -g -Wall")
endif()

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/release")

include_directories(
"${PROJECT_SOURCE_DIR}/inc"
"${PROJECT_SOURCE_DIR}/src/user")

set(SUB_SRC_DIR_LIST
"${CMAKE_CURRENT_SOURCE_DIR}/framework"
"${CMAKE_CURRENT_SOURCE_DIR}/user")

foreach(SUBDIR ${SUB_SRC_DIR_LIST})
    aux_source_directory(${SUBDIR} SRC_LIST)	# 遍历子目录将源文件都获取到变量 SRC_LIST 中
endforeach()

# 指定最终目标文件
add_executable(httpserver ${SRC_LIST})
# 指定目标文件链接的库名称，对应gcc -l 选项
target_link_libraries(httpserver crypto)  # 链接库