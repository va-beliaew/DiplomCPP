cmake_minimum_required(VERSION 3.15)

find_package(boost REQUIRED COMPONENTS beast asio)
#find_package(libpqxx REQUIRED)

add_executable(Spider
	main.cpp
	http_utils.h
	http_utils.cpp
	link.h
	)


target_compile_features(Spider PRIVATE cxx_std_17) 

target_include_directories(Spider PRIVATE beast asio ssl)
#target_include_directories(Spider PRIVATE pqxx)

target_link_libraries(Spider pqxx)

target_link_libraries(Spider SSL)

target_link_libraries(Spider beast asio)