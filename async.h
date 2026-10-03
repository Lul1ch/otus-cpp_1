#pragma once

#include <cstddef>

#ifdef _WIN32
    #define ASYNC_EXPORT __declspec(dllexport)
#else
    #define ASYNC_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

ASYNC_EXPORT void* connect(std::size_t bulk);
ASYNC_EXPORT void receive(void* handle, const char* data, std::size_t size);
ASYNC_EXPORT void disconnect(void* handle);

}