// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#if PY_VERSION_HEX >= 0x030b0000
#include <cpython/genobject.h>

#define Py_BUILD_CORE
#if PY_VERSION_HEX >= 0x030d0000
#include <opcode.h>
#else
#include <internal/pycore_opcode.h>
#include <internal/pycore_frame.h>
#endif  // PY_VERSION_HEX >= 0x030d0000
#else
#include <genobject.h>
#include <opcode.h>
#endif

#include <memory>

#include <echion/vm.h>

extern "C" {

typedef enum
{
    STATE_PENDING,
    STATE_CANCELLED,
    STATE_FINISHED
} fut_state;

#if PY_VERSION_HEX >= 0x030d0000
#define FutureObj_HEAD(prefix)                                  \
    PyObject_HEAD PyObject* prefix##_loop;                      \
    PyObject* prefix##_callback0;                               \
    PyObject* prefix##_context0;                                \
    PyObject* prefix##_callbacks;                               \
    PyObject* prefix##_exception;                               \
    PyObject* prefix##_exception_tb;                            \
    PyObject* prefix##_result;                                  \
    PyObject* prefix##_source_tb;                               \
    PyObject* prefix##_cancel_msg;                              \
    PyObject* prefix##_cancelled_exc;                           \
    fut_state prefix##_state;                                   \
    /* These bitfields need to be at the end of the struct      \
       so that these and bitfields from TaskObj are contiguous. \
    */                                                          \
    unsigned prefix##_log_tb : 1;                               \
    unsigned prefix##_blocking : 1;

#elif PY_VERSION_HEX >= 0x030b0000
#define FutureObj_HEAD(prefix)             \
    PyObject_HEAD PyObject* prefix##_loop; \
    PyObject* prefix##_callback0;          \
    PyObject* prefix##_context0;           \
    PyObject* prefix##_callbacks;          \
    PyObject* prefix##_exception;          \
    PyObject* prefix##_exception_tb;       \
    PyObject* prefix##_result;             \
    PyObject* prefix##_source_tb;          \
    PyObject* prefix##_cancel_msg;         \
    fut_state prefix##_state;              \
    int prefix##_log_tb;                   \
    int prefix##_blocking;                 \
    PyObject* dict;                        \
    PyObject* prefix##_weakreflist;        \
    PyObject* prefix##_cancelled_exc;

#elif PY_VERSION_HEX >= 0x030a0000
#define FutureObj_HEAD(prefix)             \
    PyObject_HEAD PyObject* prefix##_loop; \
    PyObject* prefix##_callback0;          \
    PyObject* prefix##_context0;           \
    PyObject* prefix##_callbacks;          \
    PyObject* prefix##_exception;          \
    PyObject* prefix##_exception_tb;       \
    PyObject* prefix##_result;             \
    PyObject* prefix##_source_tb;          \
    PyObject* prefix##_cancel_msg;         \
    fut_state prefix##_state;              \
    int prefix##_log_tb;                   \
    int prefix##_blocking;                 \
    PyObject* dict;                        \
    PyObject* prefix##_weakreflist;        \
    _PyErr_StackItem prefix##_cancelled_exc_state;

#elif PY_VERSION_HEX >= 0x03090000
#define FutureObj_HEAD(prefix)             \
    PyObject_HEAD PyObject* prefix##_loop; \
    PyObject* prefix##_callback0;          \
    PyObject* prefix##_context0;           \
    PyObject* prefix##_callbacks;          \
    PyObject* prefix##_exception;          \
    PyObject* prefix##_result;             \
    PyObject* prefix##_source_tb;          \
    PyObject* prefix##_cancel_msg;         \
    fut_state prefix##_state;              \
    int prefix##_log_tb;                   \
    int prefix##_blocking;                 \
    PyObject* dict;                        \
    PyObject* prefix##_weakreflist;        \
    _PyErr_StackItem prefix##_cancelled_exc_state;

#else
#define FutureObj_HEAD(prefix)             \
    PyObject_HEAD PyObject* prefix##_loop; \
    PyObject* prefix##_callback0;          \
    PyObject* prefix##_context0;           \
    PyObject* prefix##_callbacks;          \
    PyObject* prefix##_exception;          \
    PyObject* prefix##_result;             \
    PyObject* prefix##_source_tb;          \
    fut_state prefix##_state;              \
    int prefix##_log_tb;                   \
    int prefix##_blocking;                 \
    PyObject* dict;                        \
    PyObject* prefix##_weakreflist;
#endif

typedef struct
{
    FutureObj_HEAD(fut)
} FutureObj;

#if PY_VERSION_HEX >= 0x030d0000
typedef struct
{
    FutureObj_HEAD(task);
    unsigned task_must_cancel : 1;
    unsigned task_log_destroy_pending : 1;
    int task_num_cancels_requested;
    PyObject* task_fut_waiter;
    PyObject* task_coro;
    PyObject* task_name;
    PyObject* task_context;
} TaskObj;

#elif PY_VERSION_HEX >= 0x030a0000
typedef struct
{
    FutureObj_HEAD(task) PyObject* task_fut_waiter;
    PyObject* task_coro;
    PyObject* task_name;
    PyObject* task_context;
    int task_must_cancel;
    int task_log_destroy_pending;
    int task_num_cancels_requested;
} TaskObj;

#else
typedef struct
{
    FutureObj_HEAD(task) PyObject* task_fut_waiter;
    PyObject* task_coro;
    PyObject* task_name;
    PyObject* task_context;
    int task_must_cancel;
    int task_log_destroy_pending;
} TaskObj;
#endif

// ---- cr_await ----

#if PY_VERSION_HEX >= 0x030c0000
#define RESUME_QUICK INSTRUMENTED_RESUME
#endif

#if PY_VERSION_HEX >= 0x030b0000

// ---- config ---------------------------------------------------------------
// If your fork differs from upstream semantics, define this to 1 or 0 in your build:
//   - 1 => frame.stacktop is the *index of TOS*
//   - 0 => frame.stacktop is the *depth* (so TOS = stacktop - 1)
#ifndef DD_STACKTOP_IS_TOS
  // Heuristic based on upstream: 3.13+ treats stacktop as TOS index.
  #if PY_VERSION_HEX >= 0x030D0000  // 3.13.0
    #define DD_STACKTOP_IS_TOS 1
  #else
    #define DD_STACKTOP_IS_TOS 0
  #endif
#endif
// --------------------------------------------------------------------------

// Return false if we can't safely compute a TOS address.
static inline bool
dd_get_tos_addr(const _PyInterpreterFrame& f, const PyObject*** addr_out)
{
    if (!addr_out) return false;
    *addr_out = nullptr;

    // localsplus can be null in some transient/cleared states
    if (!f.localsplus) return false;

#if DD_STACKTOP_IS_TOS
    // stacktop is the index of TOS
    if (f.stacktop < 0) return false;
    *addr_out = reinterpret_cast<const PyObject**>(f.localsplus + f.stacktop);
#else
    // stacktop is the count (depth); TOS = stacktop - 1
    if (f.stacktop <= 0) return false;  // empty stack => no TOS
    *addr_out = reinterpret_cast<const PyObject**>(f.localsplus + (f.stacktop - 1));
#endif

    return true;
}

// Minimal, deterministic version: no opcode probing, no double-guessing.
inline PyObject* PyGen_yf(PyGenObject* gen, PyObject* frame_addr)
{
    // Only try if the generator is suspended (not created/cleared)
    if (gen->gi_frame_state >= FRAME_CLEARED || gen->gi_frame_state == FRAME_CREATED) {
        return NULL;
    }

    _PyInterpreterFrame frame;
    if (copy_type(frame_addr, frame)) {
        return NULL;
    }

    // Compute the address of TOS once, based on headers/version.
    const PyObject** tos_addr = nullptr;
    if (!dd_get_tos_addr(frame, &tos_addr) || !tos_addr) {
        return NULL;
    }

    // Read exactly one pointer from the computed TOS address.
    PyObject* yf = nullptr;
    if (copy_type(tos_addr, yf)) {
        return NULL;  // copy failed
    }

    return yf;  // may be NULL if the TOS is empty/cleared; that's fine
}

#elif PY_VERSION_HEX >= 0x030a0000
inline PyObject* PyGen_yf(PyGenObject* Py_UNUSED(gen), PyObject* frame_addr)
{
    PyObject* yf = NULL;
    PyFrameObject* f = (PyFrameObject*)frame_addr;

    if (f)
    {
        PyFrameObject frame;
        if (copy_type(f, frame))
            return NULL;

        if (frame.f_lasti < 0)
            return NULL;

        PyCodeObject code;
        if (copy_type(frame.f_code, code))
            return NULL;

        Py_ssize_t s = 0;
        auto c = pybytes_to_bytes_and_size(code.co_code, &s);
        if (c == nullptr)
            return NULL;

        if (c[(frame.f_lasti + 1) * sizeof(_Py_CODEUNIT)] != YIELD_FROM)
            return NULL;

        ssize_t nvalues = frame.f_stackdepth;
        if (nvalues < 1 || nvalues > (1 << 20))
            return NULL;

        auto stack = std::make_unique<PyObject*[]>(nvalues);

        if (copy_generic(frame.f_valuestack, stack.get(), nvalues * sizeof(PyObject*)))
            return NULL;

        yf = stack[nvalues - 1];
    }

    return yf;
}

#else
inline PyObject* PyGen_yf(PyGenObject* Py_UNUSED(gen), PyObject* frame_addr)
{
    PyObject* yf = NULL;
    PyFrameObject* f = (PyFrameObject*)frame_addr;

    if (frame_addr == NULL)
        return NULL;

    PyFrameObject frame;
    if (copy_type(f, frame))
        return NULL;

    if (frame.f_stacktop)
    {
        if (frame.f_lasti < 0)
            return NULL;

        PyCodeObject code;
        if (copy_type(frame.f_code, code))
            return NULL;

        Py_ssize_t s = 0;
        auto c = pybytes_to_bytes_and_size(code.co_code, &s);
        if (c == nullptr)
            return NULL;

        if (c[f->f_lasti + sizeof(_Py_CODEUNIT)] != YIELD_FROM)
            return NULL;

        auto stacktop = std::make_unique<PyObject*>();
        if (copy_generic(frame.f_stacktop - 1, stacktop.get(), sizeof(PyObject*)))
            return NULL;

        yf = *stacktop;
    }

    return yf;
}
#endif
}
