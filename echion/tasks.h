// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#include <memory>
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

    [[nodiscard]] static Result<std::unique_ptr<GenInfo>> create(PyObject* gen_addr);

    GenInfo(PyObject* origin, PyObject* frame, std::unique_ptr<GenInfo> await, bool is_running)
        : origin(origin), frame(frame), await(std::move(await)), is_running(is_running)
    {
    }

    GenInfo(GenInfo&& other) noexcept = default;
    GenInfo& operator=(GenInfo&& other) noexcept = default;

private:
    GenInfo(const GenInfo&) = delete;
    GenInfo& operator=(const GenInfo&) = delete;
};

static inline size_t recursion_count = 0;

[[nodiscard]] inline Result<std::unique_ptr<GenInfo>> GenInfo::create(PyObject* gen_addr)
{
    recursion_count++;
    std::cerr << "Recursion count: " << recursion_count << std::endl;

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    PyGenObject gen;

    if (copy_type(gen_addr, gen) || !PyCoro_CheckExact(&gen)) {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<std::unique_ptr<GenInfo>>::error(ErrorKind::GenInfoError);
    }

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto origin = gen_addr;

#if PY_VERSION_HEX >= 0x030b0000
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    // The frame follows the generator object
    auto frame = (gen.gi_frame_state == FRAME_CLEARED)
                ? NULL
                : (PyObject*)((char*)gen_addr + offsetof(PyGenObject, gi_iframe));
#else
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto frame = (PyObject*)gen.gi_frame;
#endif

    // NO CRASH HERE

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    PyFrameObject f;
    if (copy_type(frame, f))
    {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<std::unique_ptr<GenInfo>>::error(ErrorKind::GenInfoError);
    }

    // no crash until here

    PyObject* yf = (frame != NULL ? PyGen_yf(&gen, frame) : NULL);

    // no crash until here

    std::unique_ptr<GenInfo> await = nullptr;
    if (yf != NULL && yf != gen_addr)
    {
        // no crash if we return here        

        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

        // return Result<std::unique_ptr<GenInfo>>::error(ErrorKind::GenInfoError); // no crash until here???


        // THE CRASH SEEMS TO HAPPEN WHEN WE RECURSIVELY CALL GenInfo::create
        // auto maybe_gen_info = GenInfo::create(yf);
        // Try to say it returned an error?
        auto maybe_gen_info = Result<std::unique_ptr<GenInfo>>::error(ErrorKind::GenInfoError);
        if (maybe_gen_info) {
            // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
            await = std::move(*maybe_gen_info);
        }
    }
    
    // crash here

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

#if PY_VERSION_HEX >= 0x030b0000
    auto is_running = (gen.gi_frame_state == FRAME_EXECUTING);
#elif PY_VERSION_HEX >= 0x030a0000
    auto is_running = (frame != NULL) ? _PyFrame_IsExecuting(&f) : false;
#else
    auto is_running = gen.gi_running;
#endif

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    recursion_count--;
    return std::make_unique<GenInfo>(origin, frame, std::move(await), is_running);
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
    TaskInfo(PyObject* origin, PyObject* loop, std::unique_ptr<GenInfo> coro, StringTable::Key name, std::unique_ptr<TaskInfo> waiter)
        : origin(origin), loop(loop), coro(std::move(coro)), name(name), waiter(std::move(waiter))
    {
    }

    TaskInfo(const TaskInfo&) = delete;
    TaskInfo& operator=(const TaskInfo&) = delete;
};

inline std::unordered_map<PyObject*, PyObject*> task_link_map;
inline std::mutex task_link_map_lock;

// ----------------------------------------------------------------------------
[[nodiscard]] inline Result<TaskInfo> TaskInfo::create(TaskObj* task_addr)
{
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    TaskObj task;
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    if (copy_type(task_addr, task)){
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);
    }

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;

    // NO CRASH HERE

    auto maybe_coro = GenInfo::create(task.task_coro);
    if (!maybe_coro) {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);
    }
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        
    // CRASH HERE
    return Result<TaskInfo>::error(ErrorKind::TaskInfoError); // no crash until here????

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto coro = std::move(*maybe_coro);

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto origin = (PyObject*)task_addr;

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto maybe_name = string_table.key(task.task_name);
    if (!maybe_name)
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto loop = task.task_loop;

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    std::unique_ptr<TaskInfo> waiter = nullptr;
    if (task.task_fut_waiter)
    {
        auto maybe_waiter = TaskInfo::create((TaskObj*)task.task_fut_waiter);
        if (maybe_waiter)
        {
            waiter = std::make_unique<TaskInfo>(std::move(*maybe_waiter));
        }
    }
    
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    return Result<TaskInfo>(TaskInfo(origin, loop, std::move(coro), *maybe_name, std::move(waiter)));
}

// ----------------------------------------------------------------------------
[[nodiscard]] inline Result<TaskInfo> TaskInfo::current(PyObject* loop)
{
    if (loop == NULL)
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);

    auto maybe_dict = MirrorDict::create(asyncio_current_tasks);
    if (!maybe_dict)
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);
    
    auto maybe_task = maybe_dict->get_item(loop);
    if (!maybe_task)
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);
        
    PyObject* task = *maybe_task;
    if (task == NULL)
        return Result<TaskInfo>::error(ErrorKind::TaskInfoError);

    return TaskInfo::create((TaskObj*)task);
}

// ----------------------------------------------------------------------------
// TODO: Make this a "for_each_task" function?
inline Result<std::vector<TaskInfo::Ptr>> get_all_tasks(PyObject* loop)
{
    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    std::vector<TaskInfo::Ptr> tasks;
    if (loop == NULL) {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return tasks;
    }

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto maybe_scheduled_set = MirrorSet::create(asyncio_scheduled_tasks);
    if (!maybe_scheduled_set) {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::MirrorError);
    }

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto maybe_scheduled_tasks = maybe_scheduled_set->as_unordered_set();
    if (!maybe_scheduled_tasks)
    {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::MirrorError);
    }
    
    auto scheduled_tasks = std::move(*maybe_scheduled_tasks);

    // no crash until here

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    for (auto task_wr_addr : scheduled_tasks)
    {
        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        PyWeakReference task_wr;
        if (copy_type(task_wr_addr, task_wr)) {
            // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
            continue;
        }


        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        auto maybe_task = TaskInfo::create((TaskObj*)task_wr.wr_object);
        continue;  // no crash until here
        if (maybe_task) {
            auto task = std::move(*maybe_task);
            // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
            auto task_info = std::make_unique<TaskInfo>(std::move(task));
            if (task_info->loop == loop)
                tasks.push_back(std::move(task_info));
        }

        // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    }

    // crash if we return here
    return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::MirrorError);

    // std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    if (asyncio_eager_tasks != NULL)
    {
        auto maybe_eager_set = MirrorSet::create(asyncio_eager_tasks);
        if (!maybe_eager_set) {
            return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::MirrorError);
        }

        auto maybe_eager_tasks = maybe_eager_set->as_unordered_set();
        if (!maybe_eager_tasks)
        {
            return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::MirrorError);
        }

        for (auto task_addr : *maybe_eager_tasks)
        {
            auto maybe_task_info = TaskInfo::create((TaskObj*)task_addr);
            if (!maybe_task_info) {
                continue;
            }

            auto task_info = std::make_unique<TaskInfo>(std::move(*maybe_task_info));
            if (task_info->loop == loop)
                tasks.push_back(std::move(task_info));
        }
    }

    return Result<std::vector<TaskInfo::Ptr>>::error(ErrorKind::TaskInfoError);
    // return tasks;
}

// ----------------------------------------------------------------------------

inline std::vector<std::unique_ptr<StackInfo>> current_tasks;
inline std::mutex current_tasks_lock;

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
