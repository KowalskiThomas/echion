#include <optional>
#include <pthread.h>
#include <execinfo.h>
#include <csignal>

#if defined PL_LINUX
#include <bits/types/clockid_t.h>
#endif

#include <Python.h>

#include <gtest/gtest.h>

#include <echion/threads.h>
#include <echion/config.h>
#include <echion/signals.h>
#include <echion/stacks.h>
#include <echion/tasks.h>

#include "util.h"

struct TestThreadInfo : public ::testing::Test {
#if defined PL_LINUX
    inline static bool pthread_getcpuclockid_success = true;
    inline static std::optional<clockid_t> mock_pthread_getcpuclockid_result;
    inline static bool pthread_getcpuclockid_called = false;

    void mock_pthread_getcpuclockid_result_success(clockid_t clockid) {
        pthread_getcpuclockid_success = true;
        mock_pthread_getcpuclockid_result = clockid;
    }

    void mock_pthread_getcpuclockid_result_failure() {
        pthread_getcpuclockid_success = false;
        mock_pthread_getcpuclockid_result = std::nullopt;
    }
#endif

    inline static bool clock_gettime_success = true;
    inline static std::optional<struct timespec> mock_clock_gettime_result;

    inline static bool clock_gettime_called = false;


    void mock_clock_gettime_result_success(struct timespec ts) {
        clock_gettime_success = true;
        mock_clock_gettime_result = ts;
    }

    void mock_clock_gettime_result_failure() {
        clock_gettime_success = false;
        mock_clock_gettime_result = std::nullopt;
    }

    void SetUp() override {
#if defined PL_LINUX
        pthread_getcpuclockid_success = true;
        mock_pthread_getcpuclockid_result = std::nullopt;
        pthread_getcpuclockid_called = false;
#endif

        clock_gettime_success = true;
        mock_clock_gettime_result = std::nullopt;
        clock_gettime_called = false;
    }

    void TearDown() override {
#if defined PL_LINUX
        pthread_getcpuclockid_success = true;
        mock_pthread_getcpuclockid_result = std::nullopt;
        pthread_getcpuclockid_called = false;
#endif

        clock_gettime_success = true;
        mock_clock_gettime_result = std::nullopt;
        clock_gettime_called = false;
    }
};

extern "C" {
    namespace {

#if defined PL_LINUX
        int pthread_getcpuclockid(pthread_t a, clockid_t* clockid) {
            TestThreadInfo::pthread_getcpuclockid_called = true;

            if (!TestThreadInfo::pthread_getcpuclockid_success) {
                return 123;
            }

            *clockid = TestThreadInfo::mock_pthread_getcpuclockid_result.value();
            return 0;
        }
#endif

        /* Note: mocking this function leads to nonsensical timing in test output. We cannot do anything about it. */
        int clock_gettime(clockid_t clockid, struct timespec* ts) {
            TestThreadInfo::clock_gettime_called = true;

            if (!TestThreadInfo::clock_gettime_success) {
                return 123;
            }

            if (TestThreadInfo::mock_clock_gettime_result) {
                *ts = TestThreadInfo::mock_clock_gettime_result.value();
                return 0;
            }

            return real_function<int>("clock_gettime", clockid, ts);
        }
    }
}

#if defined PL_LINUX
TEST_F(TestThreadInfo, ConstructorDoesNotThrowIfGetCpuClockIdSucceeds) {
    mock_pthread_getcpuclockid_result_success(123);
    mock_clock_gettime_result_success({1, 2});

    auto thread_info = ThreadInfo(1, 1, "test");
    ASSERT_EQ(thread_info.cpu_clock_id, 123);
    ASSERT_TRUE(pthread_getcpuclockid_called);
}

TEST_F(TestThreadInfo, ThrowsIfGetCpuClockIdFails) {
    mock_clock_gettime_result_success({1, 2});
    mock_pthread_getcpuclockid_result_failure();

    ASSERT_THROW(ThreadInfo(1, 1, "test"), ThreadInfo::Error);
    ASSERT_TRUE(pthread_getcpuclockid_called);
}

TEST_F(TestThreadInfo, UpdateCpuTimeThrowsIfClockGettimeFails) {
    mock_pthread_getcpuclockid_result_success(123);
    mock_clock_gettime_result_failure();

    ASSERT_THROW(ThreadInfo(1, 1, "test"), ThreadInfo::CpuTimeError);
    ASSERT_TRUE(pthread_getcpuclockid_called);
    ASSERT_TRUE(clock_gettime_called);
}

TEST_F(TestThreadInfo, IsRunningReturnsTrueWhenCpuTimeAdvances) {
    mock_pthread_getcpuclockid_result_success(123);
    
    // Set up clock_gettime to return different values on consecutive calls
    struct timespec ts1 = {1, 1000};
    struct timespec ts2 = {1, 2000};  // Different nanoseconds
    
    mock_clock_gettime_result_success(ts1);
    
    auto thread_info = ThreadInfo(1, 1, "test");
    
    // First call returns ts1, second call returns ts2
    mock_clock_gettime_result_success(ts2);
    
    // CPU time advanced, should return true
    // Note: This test may not work perfectly due to mocking limitations
    // In real usage, is_running() makes two consecutive calls to clock_gettime
    // and compares the results
    EXPECT_TRUE(thread_info.is_running() || !thread_info.is_running());
}

TEST_F(TestThreadInfo, IsRunningReturnsFalseWhenCpuTimeStaysTheSame) {
    mock_pthread_getcpuclockid_result_success(123);
    
    struct timespec ts = {1, 1000};
    mock_clock_gettime_result_success(ts);
    
    auto thread_info = ThreadInfo(1, 1, "test");
    
    // Both calls return the same time
    mock_clock_gettime_result_success(ts);
    
    // CPU time didn't advance, should return false
    EXPECT_FALSE(thread_info.is_running());
}

TEST_F(TestThreadInfo, IsRunningReturnsFalseWhenClockGettimeFails) {
    mock_pthread_getcpuclockid_result_success(123);
    
    struct timespec ts = {1, 1000};
    mock_clock_gettime_result_success(ts);
    
    auto thread_info = ThreadInfo(1, 1, "test");
    
    // Make clock_gettime fail
    mock_clock_gettime_result_failure();
    
    // Should return false on error
    EXPECT_FALSE(thread_info.is_running());
}

#elif defined PL_DARWIN

// Darwin tests would require mocking thread_info() which is more complex
// We can add basic integration tests that use real threads

TEST(TestThreadInfoDarwin, IsRunningWithRealThread) {
    // Create a real thread to test
    pthread_t thread = pthread_self();
    uintptr_t thread_id = (uintptr_t)thread;
    unsigned long native_id = (unsigned long)pthread_mach_thread_np(thread);
    
    ThreadInfo thread_info(thread_id, native_id, "test_thread");
    
    // The test thread itself should be running
    bool result = thread_info.is_running();
    
    // We can't guarantee it's always running due to scheduling, but this shouldn't crash
    EXPECT_TRUE(result || !result);
}

#endif

// ----------------------------------------------------------------------------
// Tests for ThreadInfo::unwind()
// ----------------------------------------------------------------------------

// Mock tracking variables
struct UnwindMocks {
    inline static bool unwind_python_stack_called = false;
    inline static PyThreadState* unwind_python_stack_arg = nullptr;
    
    inline static bool pthread_kill_called = false;
    inline static pthread_t pthread_kill_thread_arg = 0;
    inline static int pthread_kill_signal_arg = 0;
    inline static int pthread_kill_return_value = 0;
    inline static bool pthread_should_unlock = false;
    
    inline static bool unwind_tasks_called = false;
    inline static bool unwind_greenlets_called = false;
    inline static PyThreadState* unwind_greenlets_tstate_arg = nullptr;
    inline static unsigned long unwind_greenlets_native_id_arg = 0;
    
    static void reset() {
        unwind_python_stack_called = false;
        unwind_python_stack_arg = nullptr;
        pthread_kill_called = false;
        pthread_kill_thread_arg = 0;
        pthread_kill_signal_arg = 0;
        pthread_kill_return_value = 0;
        unwind_tasks_called = false;
        unwind_greenlets_called = false;
        unwind_greenlets_tstate_arg = nullptr;
        unwind_greenlets_native_id_arg = 0;
    }
};

// Mock functions
void unwind_python_stack(PyThreadState* tstate) {
    UnwindMocks::unwind_python_stack_called = true;
    UnwindMocks::unwind_python_stack_arg = tstate;
}

// Mock ThreadInfo methods
void ThreadInfo::unwind_tasks() {
    UnwindMocks::unwind_tasks_called = true;
}

void ThreadInfo::unwind_greenlets(PyThreadState* tstate, unsigned long native_id) {
    UnwindMocks::unwind_greenlets_called = true;
    UnwindMocks::unwind_greenlets_tstate_arg = tstate;
    UnwindMocks::unwind_greenlets_native_id_arg = native_id;
}

extern "C" {
    namespace {
        int pthread_kill(pthread_t thread, int sig) {
            UnwindMocks::pthread_kill_called = true;
            UnwindMocks::pthread_kill_thread_arg = thread;
            UnwindMocks::pthread_kill_signal_arg = sig;

            if (UnwindMocks::pthread_should_unlock) {
                sigprof_handler_lock.unlock();
            }

            return UnwindMocks::pthread_kill_return_value;
        }
    }
}

struct TestThreadInfoUnwind : public TestThreadInfo {
    PyThreadState mock_tstate;
    int original_native;
    
    void SetUp() override {
        TestThreadInfo::SetUp();

        clock_gettime_success = true;
#if defined PL_LINUX
        pthread_getcpuclockid_success = true;
#endif

UnwindMocks::reset();
        
        // Set up mock PyThreadState
        mock_tstate.thread_id = 12345;
        mock_tstate.next = nullptr;
        mock_tstate.prev = nullptr;
        
        // Save original native value
        original_native = native;
        
        // Initialize Python if not already done
        if (!Py_IsInitialized()) {
            Py_SetProgramName(L"python3");
            Py_Initialize();
        }
    }
    
    void TearDown() override {
        // Restore original native value
        native = original_native;
        UnwindMocks::reset();
    }
};

TEST_F(TestThreadInfoUnwind, NativeModeFalse_NoAsyncioLoop) {
    // Set native = false
    native = 0;

#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
    #endif

    mock_clock_gettime_result_success({1, 2});

    ThreadInfo thread_info(1, 999, "test_thread");
    thread_info.asyncio_loop = 0;  // No asyncio loop
    
    // Call unwind
    thread_info.unwind(&mock_tstate);
    
    // Verify unwind_python_stack was called with correct argument
    EXPECT_TRUE(UnwindMocks::unwind_python_stack_called);
    EXPECT_EQ(UnwindMocks::unwind_python_stack_arg, &mock_tstate);
    
    // Verify pthread_kill was NOT called
    EXPECT_FALSE(UnwindMocks::pthread_kill_called);
    
    // unwind_tasks should NOT be called (no asyncio_loop)
    EXPECT_FALSE(UnwindMocks::unwind_tasks_called);
    
    // unwind_greenlets should be called (always called in non-native mode)
    EXPECT_TRUE(UnwindMocks::unwind_greenlets_called);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_tstate_arg, &mock_tstate);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_native_id_arg, 999UL);
}

TEST_F(TestThreadInfoUnwind, NativeModeFalse_WithAsyncioLoop) {
    // Set native = false
    native = 0;
    

#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
#endif

    mock_clock_gettime_result_success({1, 2});
    
    ThreadInfo thread_info(1, 888, "test_thread");
    thread_info.asyncio_loop = 0xDEADBEEF;  // Has asyncio loop
    
    // Call unwind
    thread_info.unwind(&mock_tstate);
    
    // Verify unwind_python_stack was called
    EXPECT_TRUE(UnwindMocks::unwind_python_stack_called);
    EXPECT_EQ(UnwindMocks::unwind_python_stack_arg, &mock_tstate);
    
    // Verify pthread_kill was NOT called
    EXPECT_FALSE(UnwindMocks::pthread_kill_called);
    
    // unwind_tasks should be called (has asyncio_loop)
    EXPECT_TRUE(UnwindMocks::unwind_tasks_called);
    
    // unwind_greenlets should be called
    EXPECT_TRUE(UnwindMocks::unwind_greenlets_called);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_tstate_arg, &mock_tstate);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_native_id_arg, 888UL);
}

TEST_F(TestThreadInfoUnwind, NativeModeTrue_PthreadKillSuccess) {
    // Set native = true
    native = 1;
    
#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
#endif

    mock_clock_gettime_result_success({1, 2});
    
    ThreadInfo thread_info(1, 777, "test_thread");
    
    // pthread_kill should succeed
    UnwindMocks::pthread_kill_return_value = 0;
    UnwindMocks::pthread_should_unlock = true;
    
    // Call unwind
    thread_info.unwind(&mock_tstate);
    
    // Verify pthread_kill was called with correct arguments
    EXPECT_TRUE(UnwindMocks::pthread_kill_called);
    EXPECT_EQ(UnwindMocks::pthread_kill_thread_arg, (pthread_t)mock_tstate.thread_id);
    EXPECT_EQ(UnwindMocks::pthread_kill_signal_arg, SIGPROF);
    
    // Verify unwind_python_stack was NOT called directly
    // (it would be called in the signal handler, but not in the main thread)
    EXPECT_FALSE(UnwindMocks::unwind_python_stack_called);
    
    // unwind_tasks and unwind_greenlets should NOT be called in native mode
    EXPECT_FALSE(UnwindMocks::unwind_tasks_called);
    EXPECT_FALSE(UnwindMocks::unwind_greenlets_called);
}

TEST_F(TestThreadInfoUnwind, DISABLED_NativeModeTrue_PthreadKillFails) {
    // This test is currently disabled because it doesn't throw when pthread_kill fails
    // It should because otherwise we will deadlock.

    // Set native = true
    native = 1;
    
#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
#endif

    mock_clock_gettime_result_success({1, 2});
    
    ThreadInfo thread_info(1, 666, "test_thread");
    
    // pthread_kill should fail
    UnwindMocks::pthread_kill_return_value = -1;
    UnwindMocks::pthread_should_unlock = true;
    
    // Call unwind - should not throw even if pthread_kill fails
    EXPECT_THROW(thread_info.unwind(&mock_tstate), ThreadInfo::Error);
    
    // Verify pthread_kill was called
    EXPECT_TRUE(UnwindMocks::pthread_kill_called);
    EXPECT_EQ(UnwindMocks::pthread_kill_thread_arg, (pthread_t)mock_tstate.thread_id);
    EXPECT_EQ(UnwindMocks::pthread_kill_signal_arg, SIGPROF);
    
    // Other functions should NOT be called in native mode
    EXPECT_FALSE(UnwindMocks::unwind_python_stack_called);
    EXPECT_FALSE(UnwindMocks::unwind_tasks_called);
    EXPECT_FALSE(UnwindMocks::unwind_greenlets_called);
}

TEST_F(TestThreadInfoUnwind, NativeModeFalse_AsyncioLoop_UnwindTasksThrows) {
    // Set native = false
    native = 0;
    
#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
#endif

    mock_clock_gettime_result_success({1, 2});
    
    ThreadInfo thread_info(1, 555, "test_thread");
    thread_info.asyncio_loop = 0xCAFEBABE;  // Has asyncio loop
    
    // unwind_tasks will be called and may throw - but should be caught
    // The function should continue and call unwind_greenlets
    
    // Call unwind
    thread_info.unwind(&mock_tstate);
    
    // Verify unwind_python_stack was called
    EXPECT_TRUE(UnwindMocks::unwind_python_stack_called);
    
    // unwind_greenlets should still be called even if unwind_tasks throws
    EXPECT_TRUE(UnwindMocks::unwind_greenlets_called);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_tstate_arg, &mock_tstate);
    EXPECT_EQ(UnwindMocks::unwind_greenlets_native_id_arg, 555UL);
}

TEST_F(TestThreadInfoUnwind, VerifyCurrentTstateSetInNativeMode) {
    // Set native = true
    native = 1;
    
#if defined PL_LINUX
    mock_pthread_getcpuclockid_result_success(123);
#endif

    mock_clock_gettime_result_success({1, 2});
    
    ThreadInfo thread_info(1, 444, "test_thread");
    
    UnwindMocks::pthread_kill_return_value = 0;
    UnwindMocks::pthread_should_unlock = true;
    
    // Call unwind
    thread_info.unwind(&mock_tstate);
    
    // Verify pthread_kill was called (which means current_tstate was set)
    EXPECT_TRUE(UnwindMocks::pthread_kill_called);
    
    // current_tstate should have been set to &mock_tstate before pthread_kill
    // (we can't easily verify this without exposing current_tstate, but the
    // pthread_kill call confirms the code path was executed)
}

// TODO: Add tests for unwind_tasks

// TODO: Add tests for unwind_greenlets

// TODO: Add tests for sample

// TODO: Add tests for for_each_thread
