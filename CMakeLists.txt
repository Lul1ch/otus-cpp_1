cmake_minimum_required(VERSION 3.12)

# Для новых версий CMake (3.30+) и современного Boost.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")
set(PROJECT_VERSION 0.0.${PATCH_VERSION})

project(bulk_server VERSION ${PROJECT_VERSION} LANGUAGES CXX)

find_package(Boost CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(bulk_server
    bulk_server.cpp
    lib.cpp
)

option(WITH_BOOST_TEST "Build unit tests" ON)

if(WITH_BOOST_TEST)
    find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)

    configure_file(version.h.in version.h)

    add_library(bulk_server_version STATIC
        lib.cpp
    )

    target_include_directories(bulk_server_version PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_compile_features(bulk_server_version PRIVATE cxx_std_17)

    add_executable(test_version
        test_version.cpp
    )

    target_compile_features(test_version PRIVATE cxx_std_17)

    target_compile_definitions(test_version PRIVATE
        BOOST_TEST_DYN_LINK
    )

    target_link_libraries(test_version PRIVATE
        bulk_server_version
        Boost::unit_test_framework
    )

    enable_testing()

    add_test(NAME test_version COMMAND test_version)
endif()

target_include_directories(bulk_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_features(bulk_server PRIVATE cxx_std_17)

target_link_libraries(bulk_server PRIVATE
    Boost::headers
    Threads::Threads
)

if(MSVC)
    target_compile_options(bulk_server PRIVATE
        /W4
    )
else()
    target_compile_options(bulk_server PRIVATE
        -Wall
        -Wextra
        -pedantic
        -Werror
    )
endif()

install(TARGETS bulk_server
    RUNTIME DESTINATION bin
)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "bulk_server")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "example@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Asynchronous TCP server for bulk command processing"
)

# Формат имени: bulk-server_0.0.N_amd64.deb
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)