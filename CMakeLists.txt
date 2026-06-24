cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(azzatoBackend VERSION 0.1.0 LANGUAGES C CXX)

add_executable(azzatoBackend
    main.cpp
    src/log.cpp
    src/mutex.cpp
    src/utils/crypto_util.cpp
    src/utils/hash_util.cpp
    src/utils/json_util.cpp
    src/utils/util.cpp
)

target_include_directories(azzatoBackend PRIVATE include)
