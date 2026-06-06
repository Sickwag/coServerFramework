cmake_minimum_required(VERSION 3.10.0)
project(azzatoBackend VERSION 0.1.0 LANGUAGES C CXX)

add_executable(azzatoBackend main.cpp
	log.h
	singleton.h
	nocopyable.h)

