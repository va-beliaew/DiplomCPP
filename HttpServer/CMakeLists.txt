cmake_minimum_required(VERSION 3.15)

find_package(boost REQUIRED COMPONENTS beast asio)

#find_package (libpqxx REQUIRED)

add_executable(HttpServer 
	main.cpp
	http_connection.h
	http_connection.cpp
	)


target_compile_features(HttpServer PRIVATE cxx_std_17) 

#target_link_libraries(HttpServer PRIVATE pqxx)  

target_include_directories(HttpServer PRIVATE beast asio ssl)

target_link_libraries(HttpServer beast asio)

target_link_libraries(HttpServer SSL)