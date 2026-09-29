cmake_minimum_required(VERSION 3.15)
project(CppWebServer)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(server 
    src/main.cpp 
    src/Http.cpp 
    src/Router.cpp 
    src/Server.cpp
)