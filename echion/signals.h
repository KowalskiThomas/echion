// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#if PY_VERSION_HEX >= 0x030c0000
// https://github.com/python/cpython/issues/108216#issuecomment-1696565797
#undef _PyGC_FINALIZED
#endif

#include <mutex>
#include <csignal>

#include <echion/stacks.h>
#include <echion/state.h>

// ----------------------------------------------------------------------------
inline std::mutex sigprof_handler_lock;

// ----------------------------------------------------------------------------
void sigprof_handler([[maybe_unused]] int signum);

// ----------------------------------------------------------------------------
void sigquit_handler([[maybe_unused]] int signum);

// ----------------------------------------------------------------------------
void install_signals();

// ----------------------------------------------------------------------------
void restore_signals();
