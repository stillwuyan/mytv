cmake_minimum_required(VERSION 3.15)
project(MyTv VERSION 1.0.0)

set(MODULE_NAME mytv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${MODULE_NAME}
    src/main.cpp
    src/https_json_client.cpp
    src/json_parser.cpp
    src/web_server.cpp
)

target_include_directories(${MODULE_NAME} PRIVATE
    src
    3rdparty
)

target_link_libraries(${MODULE_NAME} PRIVATE
    curl
)

target_compile_options(${MODULE_NAME} PRIVATE
)

target_link_options(${MODULE_NAME} PRIVATE
    -pthread
)
