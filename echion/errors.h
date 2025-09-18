// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#include <utility>

// Common result wrapper for error handling
template<typename T>
struct Result {
    union {
        T value;
    };
    bool success;
    
    Result() : success(false) {}
    Result(const T& val) : value(val), success(true) {}
    Result(T&& val) : value(std::move(val)), success(true) {}
    
    ~Result() {
        if (success) {
            value.~T();
        }
    }
    
    Result(const Result& other) : success(other.success) {
        if (success) {
            new(&value) T(other.value);
        }
    }
    
    Result& operator=(const Result& other) {
        if (this != &other) {
            if (success) {
                value.~T();
            }
            success = other.success;
            if (success) {
                new(&value) T(other.value);
            }
        }
        return *this;
    }
    
    explicit operator bool() const { return success; }
    T& operator*() { return value; }
    const T& operator*() const { return value; }
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
    
    static Result error() { 
        return Result();
    }
};

// Specialization for void operations
template<>
struct Result<void> {
    bool success;
    
    Result() : success(false) {}
    Result(bool s) : success(s) {}
    
    explicit operator bool() const { return success; }
    
    static Result error() { return Result(false); }
    static Result ok() { return Result(true); }
};

// Cache module error codes
enum class CacheError {
    SUCCESS = 0,
    KEY_NOT_FOUND = 1,
    CAPACITY_EXCEEDED = 2,
    INVALID_KEY = 3
};

// String module error codes
enum class StringError {
    SUCCESS = 0,
    INVALID_UNICODE = 1,
    SIZE_TOO_LARGE = 2,
    COPY_FAILED = 3,
    LOOKUP_FAILED = 4,
    ENCODING_ERROR = 5
};

// Frame module error codes  
enum class FrameError {
    SUCCESS = 0,
    INVALID_FRAME = 1,
    LOCATION_ERROR = 2,
    COPY_FAILED = 3,
    CACHE_ERROR = 4
};

// Task module error codes
enum class TaskError {
    SUCCESS = 0,
    INVALID_TASK = 1,
    GENERATOR_ERROR = 2,
    COPY_FAILED = 3,
    STRING_ERROR = 4,
    MIRROR_ERROR = 5
};

// Generator info error codes
enum class GenError {
    SUCCESS = 0,
    INVALID_GENERATOR = 1,
    NOT_COROUTINE = 2,
    FRAME_ERROR = 3,
    COPY_FAILED = 4
};

// Mirror error codes
enum class MirrorError {
    SUCCESS = 0,
    INVALID_OBJECT = 1,
    ACCESS_FAILED = 2,
    TYPE_ERROR = 3
};

// Stack error codes
enum class StackError {
    SUCCESS = 0,
    UNWIND_FAILED = 1,
    INVALID_FRAME = 2,
    COPY_FAILED = 3,
    CHUNK_UPDATE_FAILED = 4,
    MEMORY_ALLOCATION_FAILED = 5
};

// Memory/VM error codes
enum class VMError {
    SUCCESS = 0,
    COPY_FAILED = 1,
    INVALID_ADDRESS = 2,
    PERMISSION_DENIED = 3
};

// Long integer error codes
enum class LongError {
    SUCCESS = 0,
    CONVERSION_FAILED = 1,
    OVERFLOW = 2,
    INVALID_OBJECT = 3
};

// Render error codes
enum class RenderError {
    SUCCESS = 0,
    INITIALIZATION_FAILED = 1,
    OUTPUT_ERROR = 2,
    INVALID_DATA = 3
};

// Thread error codes
enum class ThreadError {
    SUCCESS = 0,
    INVALID_THREAD = 1,
    ACCESS_FAILED = 2,
    COPY_FAILED = 3
};