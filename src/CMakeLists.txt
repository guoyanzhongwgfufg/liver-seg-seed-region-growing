cmake_minimum_required(VERSION 3.10)
project(LiverLungSegmentation)   

# 引入 ITK
find_package(ITK CONFIG REQUIRED)
include(${ITK_USE_FILE})

find_package(ITK CONFIG REQUIRED)


# 定义可执行目标
add_executable(src
    official_demo.cpp
    algo.cpp
    main.cpp
)

