cmake_minimum_required(VERSION 3.12)

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")
set(PROJECT_VERSION 0.0.${PATCH_VERSION})

project(async VERSION ${PROJECT_VERSION} LANGUAGES CXX)

option(WITH_BOOST_TEST "Whether to build Boost test" ON)

configure_file(version.h.in version.h)

add_library(async SHARED
    async.cpp
)

target_include_directories(async
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(async PRIVATE Threads::Threads)

add_executable(async_demo main.cpp)

set_target_properties(async async_demo PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(async_demo PRIVATE
    async
    Threads::Threads
)

set_target_properties(async_demo PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
)

if(WITH_BOOST_TEST)
    find_package(Boost REQUIRED COMPONENTS unit_test_framework)

    add_executable(test_version test_version.cpp)

    set_target_properties(test_version PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    target_compile_definitions(test_version PRIVATE
        BOOST_TEST_DYN_LINK
    )

    target_link_libraries(test_version PRIVATE
        Boost::unit_test_framework
        async
    )
endif()

if(MSVC)
    target_compile_options(async PRIVATE /W4)
    target_compile_options(async_demo PRIVATE /W4)

    if(WITH_BOOST_TEST)
        target_compile_options(test_version PRIVATE /W4)
    endif()
else()
    target_compile_options(async PRIVATE
        -Wall -Wextra -pedantic -Werror
    )
    target_compile_options(async_demo PRIVATE
        -Wall -Wextra -pedantic -Werror
    )

    if(WITH_BOOST_TEST)
        target_compile_options(test_version PRIVATE
            -Wall -Wextra -pedantic -Werror
        )
    endif()
endif()

install(TARGETS async async_demo
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES async.h DESTINATION include)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME async)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT example@example.com)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Asynchronous bulk command parser library")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

if(WITH_BOOST_TEST)
    enable_testing()
    add_test(NAME test_version COMMAND test_version)
endif()