// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <weakrefobject.h>

#if PY_VERSION_HEX >= 0x030b0000
#include <cpython/genobject.h>

#define Py_BUILD_CORE
#if PY_VERSION_HEX >= 0x030d0000
#include <opcode.h>
#else
#include <internal/pycore_opcode.h>
#endif  // PY_VERSION_HEX >= 0x030d0000
#else
#include <genobject.h>
#include <opcode.h>
#endif  // PY_VERSION_HEX >= 0x30b0000

#include <mutex>
#include <stack>
#include <unordered_map>
#include <vector>

#include <echion/config.h>
#include <echion/frame.h>
#include <echion/mirrors.h>
#include <echion/stacks.h>
#include <echion/state.h>
#include <echion/strings.h>
#include <echion/timing.h>

#include <echion/cpython/tasks.h>

class GenInfo
{
public:
    typedef std::unique_ptr<GenInfo> Ptr;

    PyObject* origin = NULL;
    PyObject* frame = NULL;

    GenInfo::Ptr await = nullptr;

    bool is_running = false;

    static Result<GenInfo> create(PyObject* gen_addr);

    GenInfo(GenInfo&& other) noexcept = default;
    GenInfo& operator=(GenInfo&& other) noexcept = default;

private:
    GenInfo() = default;
    GenInfo(const GenInfo&) = delete;
    GenInfo& operator=(const GenInfo&) = delete;
};

Result<GenInfo> GenInfo::create(PyObject* gen_addr)
{
    GenInfo gen_info;
    PyGenObject gen;

    if (copy_type(gen_addr, gen) || !PyCoro_CheckExact(&gen))
        return Result<GenInfo>::error();

    gen_info.origin = gen_addr;

#if PY_VERSION_HEX >= 0x030b0000
    // The frame follows the generator object
    gen_info.frame = (gen.gi_frame_state == FRAME_CLEARED)
                ? NULL
                : (PyObject*)((char*)gen_addr + offsetof(PyGenObject, gi_iframe));
#else
    gen_info.frame = (PyObject*)gen.gi_frame;
#endif

    PyFrameObject f;
    if (copy_type(gen_info.frame, f))
        return Result<GenInfo>::error();

    PyObject* yf = (gen_info.frame != NULL ? PyGen_yf(&gen, gen_info.frame) : NULL);
    if (yf != NULL && yf != gen_addr)
    {
        auto await_result = GenInfo::create(yf);
        if (await_result)
        {
            gen_info.await = std::make_unique<GenInfo>(std::move(*await_result));
        }
        else
        {
            gen_info.await = nullptr;
        }
    }

#if PY_VERSION_HEX >= 0x030b0000
    gen_info.is_running = (gen.gi_frame_state == FRAME_EXECUTING);
#elif PY_VERSION_HEX >= 0x030a0000
    gen_info.is_running = (gen_info.frame != NULL) ? _PyFrame_IsExecuting(&f) : false;
#else
    gen_info.is_running = gen.gi_running;
#endif

    return Result<GenInfo>(std::move(gen_info));
}

// ----------------------------------------------------------------------------

class TaskInfo
{
public:
    typedef std::unique_ptr<TaskInfo> Ptr;
    typedef std::reference_wrapper<TaskInfo> Ref;

    PyObject* origin = NULL;
    PyObject* loop = NULL;

    GenInfo::Ptr coro = nullptr;

    StringTable::Key name;

    // Information to reconstruct the async stack as best as we can
    TaskInfo::Ptr waiter = nullptr;

    static Result<TaskInfo> create(TaskObj*);

    static Result<TaskInfo> current(PyObject*);
    
    TaskInfo(TaskInfo&& other) noexcept = default;
    TaskInfo& operator=(TaskInfo&& other) noexcept = default;
    
    inline size_t unwind(FrameStack&);

private:
    TaskInfo() = default;
    TaskInfo(const TaskInfo&) = delete;
    TaskInfo& operator=(const TaskInfo&) = delete;
};

inline std::unordered_map<PyObject*, PyObject*> task_link_map;
inline std::mutex task_link_map_lock;

// ----------------------------------------------------------------------------
Result<TaskInfo> TaskInfo::create(TaskObj* task_addr)
{
    TaskInfo task_info;
    TaskObj task;
    
    if (copy_type(task_addr, task))
        return Result<TaskInfo>::error();

    auto coro_result = GenInfo::create(task.task_coro);
    if (!coro_result)
        return Result<TaskInfo>::error();
        
    task_info.coro = std::make_unique<GenInfo>(std::move(*coro_result));

    task_info.origin = (PyObject*)task_addr;

    auto name_result = string_table.key(task.task_name);
    if (!name_result)
        return Result<TaskInfo>::error();
    task_info.name = *name_result;

    task_info.loop = task.task_loop;

    if (task.task_fut_waiter)
    {
        auto waiter_result = TaskInfo::create((TaskObj*)task.task_fut_waiter);
        if (waiter_result)
        {
            task_info.waiter = std::make_unique<TaskInfo>(std::move(*waiter_result));
        }
        else
        {
            task_info.waiter = nullptr;
        }
    }
    
    return Result<TaskInfo>(std::move(task_info));
}

// ----------------------------------------------------------------------------
Result<TaskInfo> TaskInfo::current(PyObject* loop)
{
    if (loop == NULL)
        return Result<TaskInfo>::error();

    auto dict_result = MirrorDict::create(asyncio_current_tasks);
    if (!dict_result)
        return Result<TaskInfo>::error();
    
    auto task_result = (*dict_result).get_item(loop);
    if (!task_result)
        return Result<TaskInfo>::error();
        
    PyObject* task = *task_result;
    if (task == NULL)
        return Result<TaskInfo>::error();

    return TaskInfo::create((TaskObj*)task);
}

// ----------------------------------------------------------------------------
// TODO: Make this a "for_each_task" function?
std::vector<TaskInfo::Ptr> get_all_tasks(PyObject* loop)
{
    std::vector<TaskInfo::Ptr> tasks;
    if (loop == NULL)
        return tasks;

    auto scheduled_set_result = MirrorSet::create(asyncio_scheduled_tasks);
    if (scheduled_set_result)
    {
        auto scheduled_tasks_result = (*scheduled_set_result).as_unordered_set();
        if (!scheduled_tasks_result)
            return tasks;

        for (auto task_wr_addr : *scheduled_tasks_result)
        {
            PyWeakReference task_wr;
            if (copy_type(task_wr_addr, task_wr))
                continue;

            auto task_result = TaskInfo::create((TaskObj*)task_wr.wr_object);
            if (task_result)
            {
                auto task_info = std::make_unique<TaskInfo>(std::move(*task_result));
                if (task_info->loop == loop)
                    tasks.push_back(std::move(task_info));
            }
        }

        if (asyncio_eager_tasks != NULL)
        {
            auto eager_set_result = MirrorSet::create(asyncio_eager_tasks);
            if (eager_set_result)
            {
                auto eager_tasks_result = (*eager_set_result).as_unordered_set();
                if (eager_tasks_result)
                {
                    for (auto task_addr : *eager_tasks_result)
                    {
                        auto task_result = TaskInfo::create((TaskObj*)task_addr);
                        if (task_result)
                        {
                            auto task_info = std::make_unique<TaskInfo>(std::move(*task_result));
                            if (task_info->loop == loop)
                                tasks.push_back(std::move(task_info));
                        }
                    }
                }
            }
        }
    }

    return tasks;
}

// ----------------------------------------------------------------------------

inline std::vector<std::unique_ptr<StackInfo>> current_tasks;

// ----------------------------------------------------------------------------

inline size_t TaskInfo::unwind(FrameStack& stack)
{
    // TODO: Check for running task.
    std::stack<PyObject*> coro_frames;

    // Unwind the coro chain
    for (auto coro = this->coro.get(); coro != NULL; coro = coro->await.get())
    {
        if (coro->frame != NULL)
            coro_frames.push(coro->frame);
    }

    int count = 0;

    // Unwind the coro frames
    while (!coro_frames.empty())
    {
        PyObject* frame = coro_frames.top();
        coro_frames.pop();

        count += unwind_frame(frame, stack);
    }

    return count;
}
