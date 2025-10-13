// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2025 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define Py_BUILD_CORE

#include <Python.h>
#if PY_VERSION_HEX >= 0x030c0000
// https://github.com/python/cpython/issues/108216#issuecomment-1696565797
#undef _PyGC_FINALIZED
#endif

#include <echion/stacks.h>
#include <echion/strings.h>


#define FRAME_NOT_SET Py_False  // Sentinel for frame cell


class GreenletInfo
{
public:
    typedef std::unique_ptr<GreenletInfo> Ptr;
    typedef std::reference_wrapper<GreenletInfo> Ref;
    typedef uintptr_t ID;

    ID greenlet_id = 0;
    StringTable::Key name;
    PyObject* frame = NULL;

    GreenletInfo(ID id, PyObject* frame, StringTable::Key name);

    int unwind(PyObject*, PyThreadState*, FrameStack&);
};


// ----------------------------------------------------------------------------

// We make this a reference to a heap-allocated object so that we can avoid
// the destruction on exit. We are in charge of cleaning up the object. Note
// that the object will leak, but this is not a problem.
inline std::unordered_map<GreenletInfo::ID, GreenletInfo::Ptr>& greenlet_info_map =
    *(new std::unordered_map<GreenletInfo::ID, GreenletInfo::Ptr>());

// maps greenlets to their parent
inline std::unordered_map<GreenletInfo::ID, GreenletInfo::ID>& greenlet_parent_map =
    *(new std::unordered_map<GreenletInfo::ID, GreenletInfo::ID>());

// maps threads to any currently active greenlets
inline std::unordered_map<uintptr_t, GreenletInfo::ID>& greenlet_thread_map =
    *(new std::unordered_map<uintptr_t, GreenletInfo::ID>());

inline std::mutex greenlet_info_map_lock;

// ----------------------------------------------------------------------------

inline std::vector<std::unique_ptr<StackInfo>> current_greenlets;

// ----------------------------------------------------------------------------
