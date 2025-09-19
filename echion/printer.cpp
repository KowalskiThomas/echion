// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#if PY_VERSION_HEX >= 0x030c0000
// https://github.com/python/cpython/issues/108216#issuecomment-1696565797
#undef _PyGC_FINALIZED
#endif

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined PL_DARWIN
#include <pthread.h>
#endif

#include <echion/config.h>
#include <echion/greenlets.h>
#include <echion/interp.h>
#include <echion/memory.h>
#include <echion/mojo.h>
#include <echion/signals.h>
#include <echion/stacks.h>
#include <echion/state.h>
#include <echion/threads.h>
#include <echion/timing.h>

struct DurationsPrinter {
    std::vector<std::chrono::nanoseconds> durations;

    ~DurationsPrinter() {
        std::cout << "[";
        for (size_t i = 0; i < durations.size(); i++) {
            const auto& duration = durations[i];
            std::cout << duration.count() << (i < durations.size() - 1 ? ", " : "");
            
        }
        std::cout << "]" << std::endl;
    }
};

DurationsPrinter printer;

/*
printer.durations.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
*/