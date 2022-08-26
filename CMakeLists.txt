cmake_minimum_required(VERSION 3.4...3.18)
project(main)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV 4.5.5 EXACT REQUIRED PATHS /opt/opencv) # Ref: https://stackoverflow.com/questions/49816206/cmake-find-package-specify-path
include_directories(${OpenCV_INCLUDE_DIRS})

set(CMAKE_BUILD_TYPE Debug)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 开编译优化
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

    # 输出链接 map
    # Ref: https://stackoverflow.com/questions/23573096/how-to-express-that-a-map-file-depends-on-add-executable
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=output.map")

    # 增加编译器宏
    add_definitions(-DWITH_GNU_COMPILER)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Ref: https://localcoder.org/possible-to-force-cmake-msvc-to-use-utf-8-encoding-for-source-files-without-a-bo
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>") 
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
else()
    MESSAGE("Compiler is not supported")
endif()

aux_source_directory(./src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(${PROJECT_NAME} serial)
    target_link_libraries(${PROJECT_NAME} pthread)
    
    set_source_files_properties("main.cpp" PROPERTIES OBJECT_OUTPUTS "output.map")
endif()