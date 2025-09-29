// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#include <utility>


enum class ErrorKind {
    Undefined,
    LookupError,
    PyBytesError,
    BytecodeError,
    FrameError,
    MirrorError,
    PyLongError,
    PyUnicodeError,
    UnwindError,
    StackChunkError,
    GenInfoError,
    TaskInfoError,
    ThreadInfoError,
    CpuTimeError,
};

template<typename T>
struct Result {
    union {
        ErrorKind error_value;
        T value;
    };
    bool success;
    
    Result(const T& val) : value(val), success(true) {}
    Result(T&& val) : value(std::move(val)), success(true) {}
    Result(ErrorKind error_value) : error_value(error_value), success(false) {}

    ~Result() {
        if (success) {
            value.~T();
        }
    }
    
    Result(const Result& other) {
        error_value = other.error_value;
        success = other.success;
        if (success) {
            new(&value) T(other.value);
        }
    }
    
    Result& operator=(const Result& other) {
        if (this != &other) {
            if (success) {
                value.~T();
            }

            error_value = other.error_value;
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
    
    static Result error(ErrorKind error_value) { 
        return Result(error_value);
    }
    static Result ok(const T& val) {
        return Result(val);
    }
    static Result ok(T&& val) {
        return Result(std::move(val));
    }
};

// Specialization for void operations
template<>
struct Result<void> {
    explicit operator bool() const { return error_value_ == ErrorKind::Undefined; }
    
    static Result error(ErrorKind error_value) { return Result(error_value); }
    static Result ok() { return Result(ErrorKind::Undefined); }

    ErrorKind error_value() const { return error_value_; }

    private:
    ErrorKind error_value_;
    
    Result(ErrorKind error_value) : error_value_(error_value) {}
    Result() : error_value_(ErrorKind::Undefined) {}
};