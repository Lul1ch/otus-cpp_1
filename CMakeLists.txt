cmake_minimum_required(VERSION 3.14)
project(fashion_mnist LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_TESTING "Build unit tests" ON)

# ---------- CatBoost ----------
# По умолчанию ищем библиотеку в third_party/catboost (см. scripts/download_catboost.sh).
# Для Docker-образа sdukshis/cppml: -DCATBOOST_ROOT=/usr/local
set(CATBOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/catboost" CACHE PATH "CatBoost location")

find_path(CATBOOST_INCLUDE_DIR c_api.h
    HINTS ${CATBOOST_ROOT} ${CATBOOST_ROOT}/include ${CATBOOST_ROOT}/include/catboost
    NO_DEFAULT_PATH)
find_library(CATBOOST_LIBRARY NAMES catboostmodel catboost
    HINTS ${CATBOOST_ROOT} ${CATBOOST_ROOT}/lib
    NO_DEFAULT_PATH)

if(NOT CATBOOST_INCLUDE_DIR OR NOT CATBOOST_LIBRARY)
    message(FATAL_ERROR "CatBoost not found in ${CATBOOST_ROOT}. "
                        "Run scripts/download_catboost.sh or pass -DCATBOOST_ROOT=<path>")
endif()
message(STATUS "CatBoost header: ${CATBOOST_INCLUDE_DIR}")
message(STATUS "CatBoost library: ${CATBOOST_LIBRARY}")

add_library(catboost_model SHARED IMPORTED)
set_target_properties(catboost_model PROPERTIES
    IMPORTED_LOCATION "${CATBOOST_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CATBOOST_INCLUDE_DIR}")

# ---------- Код проекта ----------
add_library(fashion_mnist_core STATIC
    src/dataset.cpp
    src/accuracy.cpp
    src/catboost_classifier.cpp)
target_include_directories(fashion_mnist_core PUBLIC src)
target_link_libraries(fashion_mnist_core PUBLIC catboost_model)
target_compile_options(fashion_mnist_core PRIVATE -Wall -Wextra -pedantic)

add_executable(fashion_mnist src/main.cpp)
target_link_libraries(fashion_mnist PRIVATE fashion_mnist_core)
target_compile_options(fashion_mnist PRIVATE -Wall -Wextra -pedantic)

# Чтобы бинарник находил .so без LD_LIBRARY_PATH
get_filename_component(CATBOOST_LIBRARY_DIR "${CATBOOST_LIBRARY}" DIRECTORY)
set_target_properties(fashion_mnist PROPERTIES BUILD_RPATH "${CATBOOST_LIBRARY_DIR}")

# ---------- Тесты ----------
if(BUILD_TESTING)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(unit_tests tests/test_dataset.cpp tests/test_model.cpp)
    target_link_libraries(unit_tests PRIVATE fashion_mnist_core GTest::gtest_main)
    target_compile_definitions(unit_tests PRIVATE
        TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
        MODEL_PATH="${CMAKE_SOURCE_DIR}/model.cbm")
    set_target_properties(unit_tests PROPERTIES BUILD_RPATH "${CATBOOST_LIBRARY_DIR}")

    include(GoogleTest)
    gtest_discover_tests(unit_tests)
endif()
