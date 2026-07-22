cmake_minimum_required(VERSION 3.16)
project(azzatoBackend VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -Wall -Wno-deprecated-declarations")

option(BUILD_TEST "Build tests" ON)
add_compile_definitions(AZZATO_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/bin/conf")

# =============================================================================
# Package dependencies
# =============================================================================
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# vcpkg: CONFIG mode packages
find_package(yaml-cpp CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(Libevent CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# MySQL: system package
find_path(MYSQL_INCLUDE_DIR mysql.h PATHS /usr/include/mysql /usr/local/include/mysql NO_DEFAULT_PATH)
find_library(MYSQL_LIBRARY mysqlclient)

# hiredis-vip: custom compiled, path from CMakePresets.json
if(NOT HIREDIS_VIP_DIR)
	set(HIREDIS_VIP_DIR "/home/azzato/Programs/dependencies/hiredis-vip"
		CACHE PATH "Path to hiredis-vip installation")
endif()
find_library(HIREDIS_VIP_LIB hiredis_vip PATHS ${HIREDIS_VIP_DIR} NO_DEFAULT_PATH REQUIRED)

# =============================================================================
# Library target
# =============================================================================
file(GLOB_RECURSE AZZATO_SRC CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
list(FILTER AZZATO_SRC EXCLUDE REGEX "src/main\\.cpp$")

add_library(azzato STATIC ${AZZATO_SRC})
target_include_directories(azzato
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
	${MYSQL_INCLUDE_DIR}
	${HIREDIS_VIP_DIR}
)

target_link_libraries(azzato
	PUBLIC
	dl
	pthread
	Boost::boost
	yaml-cpp::yaml-cpp
	JsonCpp::JsonCpp
	libevent::core
	libevent::extra
	libevent::pthreads
	protobuf::libprotobuf
	OpenSSL::SSL
	OpenSSL::Crypto
	ZLIB::ZLIB
	unofficial::sqlite3::sqlite3
	${HIREDIS_VIP_LIB}
	${MYSQL_LIBRARY}
)

# =============================================================================
# Executables
# =============================================================================
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_executable(azzatoBackend main.cpp)
target_link_libraries(azzatoBackend PRIVATE azzato)

# =============================================================================
# Examples
# =============================================================================
file(GLOB EXAMPLE_SRCS CONFIGURE_DEPENDS "examples/*.cpp")
foreach(example_src ${EXAMPLE_SRCS})
	get_filename_component(example_name ${example_src} NAME_WE)
	add_executable(${example_name} ${example_src})
	target_link_libraries(${example_name} PRIVATE azzato)
endforeach()

# =============================================================================
# Tests: each tests/*.cpp is a standalone executable with its own main()
# =============================================================================
if(BUILD_TEST)
	enable_testing()
	file(GLOB TEST_SRCS CONFIGURE_DEPENDS "tests/*.cpp")
	foreach(test_src ${TEST_SRCS})
		get_filename_component(test_name ${test_src} NAME_WE)
		add_executable(${test_name} ${test_src})
		target_link_libraries(${test_name} PRIVATE azzato)
		add_test(NAME ${test_name} COMMAND ${test_name})
	endforeach()
endif()
