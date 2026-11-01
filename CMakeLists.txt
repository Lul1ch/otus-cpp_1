cmake_minimum_required(VERSION 3.16)

project(
    pets_monitor
    VERSION 1.0.0
    LANGUAGES CXX
)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(Threads REQUIRED)
find_package(Boost 1.75 CONFIG REQUIRED COMPONENTS json)

add_executable(pets_monitor
    main.cpp
)

target_compile_features(pets_monitor PRIVATE cxx_std_17)

set_target_properties(pets_monitor PROPERTIES
    CXX_EXTENSIONS OFF
)

target_link_libraries(pets_monitor PRIVATE
    Boost::json
    Threads::Threads
)

if(MSVC)
    target_compile_options(pets_monitor PRIVATE
        /W4
        /utf-8
    )
else()
    target_compile_options(pets_monitor PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
    "${CMAKE_CURRENT_BINARY_DIR}/config.json"
    COPYONLY
)