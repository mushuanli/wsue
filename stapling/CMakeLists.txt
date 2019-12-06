cmake_minimum_required(VERSION 3.4.1)
project(sc)

set(THREADS_PREFER_PTHREAD_FLAG ON)
set(SOURCE_FILES ssl.cpp ocsp_stapling.cpp )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -I${CMAKE_CURRENT_SOURCE_DIR} -DTEST_SSL_")


find_package (Threads REQUIRED)
find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    set(OPENSSL_USE_STATIC_LIBS FALSE)
endif()

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} Threads::Threads OpenSSL::Crypto OpenSSL::SSL)
add_executable(svc ocsp_stapling_serv.cpp)
target_link_libraries(svc Threads::Threads OpenSSL::Crypto OpenSSL::SSL)

