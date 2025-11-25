cmake_minimum_required(VERSION 3.15)

project(Diplom)



# TODO: Define by your own needs:
set(OPENSSL_ROOT_DIR "C:\vcpkg\vcpkg\installed\x64-windows\include\openssl")

set(BOOST_ROOT "C:\vcpkg\vcpkg\installed\x64-windows\include")

#set(libpqxx_DIR "C:\libpqxx-7.7.4\include\pqxx")


# TODO: If you are running on Windows below 10, change this:

#add_compile_definitions(_WIN32_WINNT=0x0A01)



# Boost:
find_package(Boost 1.80.0 REQUIRED COMPONENTS system thread locale)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find Boost")
endif()


# PQXX
set(PostgreSQL_ADDITIONAL_VERSIONS "15")

#add_subdirectory(${libpqxx_DIR} libpqxx-build) 


# OpenSSL
find_package(OpenSSL REQUIRED)



add_subdirectory(Spider)

add_subdirectory(HttpServer)