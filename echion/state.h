// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#if defined __GNUC__ && defined HAVE_STD_ATOMIC
#undef HAVE_STD_ATOMIC
#endif
#define Py_BUILD_CORE
#include <internal/pycore_pystate.h>

#include <condition_variable>
#include <mutex>
#include <thread>

inline _PyRuntimeState* runtime = &_PyRuntime;
inline PyThreadState* current_tstate = nullptr;

inline std::thread* sampler_thread = nullptr;

inline int running = 0;

inline std::unique_ptr<std::thread> where_thread;
inline std::condition_variable where_cv;
inline std::mutex where_lock;

inline PyObject* asyncio_current_tasks = nullptr;
inline PyObject* asyncio_scheduled_tasks = nullptr;  // WeakSet
inline PyObject* asyncio_eager_tasks = nullptr;      // set
