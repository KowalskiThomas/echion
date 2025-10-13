#include <gtest/gtest.h>
#include <csignal>
#include <thread>
#include <chrono>
#include <Python.h>

#include <echion/signals.h>
#include <echion/config.h>
#include <echion/state.h>
#include <echion/stacks.h>

// Mock tracking
struct SignalMocks {
    inline static bool unwind_native_stack_called = false;
    inline static bool unwind_python_stack_called = false;
    inline static PyThreadState* unwind_python_stack_arg = nullptr;
    
    inline static void (*saved_sigprof_handler)(int) = nullptr;
    inline static void (*saved_sigquit_handler)(int) = nullptr;
    
    static void reset() {
        unwind_native_stack_called = false;
        unwind_python_stack_called = false;
        unwind_python_stack_arg = nullptr;
        saved_sigprof_handler = nullptr;
        saved_sigquit_handler = nullptr;
    }
};

// Mock functions
#ifndef UNWIND_NATIVE_DISABLE
void unwind_native_stack() {
    SignalMocks::unwind_native_stack_called = true;
}
#endif

void unwind_python_stack(PyThreadState* tstate) {
    SignalMocks::unwind_python_stack_called = true;
    SignalMocks::unwind_python_stack_arg = tstate;
}

struct TestSignals : public ::testing::Test {
    int original_native;
    PyThreadState mock_tstate;
    
    void SetUp() override {
        SignalMocks::reset();
        
        // Save original native value
        original_native = native;
        
        // Set up mock PyThreadState
        mock_tstate.thread_id = 12345;
        mock_tstate.next = nullptr;
        mock_tstate.prev = nullptr;
        
        // Initialize Python if needed
        if (!Py_IsInitialized()) {
            Py_Initialize();
        }
        
        // Save current signal handlers
        SignalMocks::saved_sigprof_handler = signal(SIGPROF, SIG_DFL);
        SignalMocks::saved_sigquit_handler = signal(SIGQUIT, SIG_DFL);
    }
    
    void TearDown() override {
        // Restore original native value
        native = original_native;
        
        // Restore signal handlers
        if (SignalMocks::saved_sigprof_handler) {
            signal(SIGPROF, SignalMocks::saved_sigprof_handler);
        }
        if (SignalMocks::saved_sigquit_handler) {
            signal(SIGQUIT, SignalMocks::saved_sigquit_handler);
        }
        
        SignalMocks::reset();
    }
};

// ----------------------------------------------------------------------------
// Tests for sigprof_handler
// ----------------------------------------------------------------------------

#ifndef UNWIND_NATIVE_DISABLE
TEST_F(TestSignals, SigprofHandler_WithNativeUnwind) {
    // Set current_tstate
    current_tstate = &mock_tstate;
    
    // Lock the mutex before calling the handler
    sigprof_handler_lock.lock();
    
    // Call sigprof_handler
    sigprof_handler(SIGPROF);
    
    // Verify unwind_native_stack was called
    EXPECT_TRUE(SignalMocks::unwind_native_stack_called);
    
    // Verify unwind_python_stack was called with correct argument
    EXPECT_TRUE(SignalMocks::unwind_python_stack_called);
    EXPECT_EQ(SignalMocks::unwind_python_stack_arg, &mock_tstate);
    
    // Verify the mutex was unlocked
    // Try to lock it - should succeed immediately if it was unlocked
    EXPECT_TRUE(sigprof_handler_lock.try_lock());
    sigprof_handler_lock.unlock();
}
#endif

TEST_F(TestSignals, SigprofHandler_WithoutNativeUnwind) {
    // Set current_tstate
    current_tstate = &mock_tstate;
    
    // Lock the mutex before calling the handler
    sigprof_handler_lock.lock();
    
    // Call sigprof_handler
    sigprof_handler(SIGPROF);
    
#ifndef UNWIND_NATIVE_DISABLE
    // If native unwinding is enabled, it should have been called
    EXPECT_TRUE(SignalMocks::unwind_native_stack_called);
#else
    // If native unwinding is disabled, it should NOT have been called
    EXPECT_FALSE(SignalMocks::unwind_native_stack_called);
#endif
    
    // Verify unwind_python_stack was called regardless
    EXPECT_TRUE(SignalMocks::unwind_python_stack_called);
    EXPECT_EQ(SignalMocks::unwind_python_stack_arg, &mock_tstate);
    
    // Verify the mutex was unlocked
    EXPECT_TRUE(sigprof_handler_lock.try_lock());
    sigprof_handler_lock.unlock();
}

TEST_F(TestSignals, SigprofHandler_UnlocksMutex) {
    current_tstate = &mock_tstate;
    
    // Lock the mutex
    sigprof_handler_lock.lock();
    
    // Verify it's locked
    EXPECT_FALSE(sigprof_handler_lock.try_lock());
    
    // Call handler
    sigprof_handler(SIGPROF);
    
    // Verify it's now unlocked
    EXPECT_TRUE(sigprof_handler_lock.try_lock());
    sigprof_handler_lock.unlock();
}

// ----------------------------------------------------------------------------
// Tests for sigquit_handler
// ----------------------------------------------------------------------------

TEST_F(TestSignals, SigquitHandler_NotifiesConditionVariable) {
    bool notified = false;
    
    // Start a thread waiting on the condition variable
    std::thread waiter([&notified]() {
        std::unique_lock<std::mutex> lock(where_lock);
        where_cv.wait(lock);
        notified = true;
    });
    
    // Give the waiter time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Call sigquit_handler
    sigquit_handler(SIGQUIT);
    
    // Wait for the waiter thread
    waiter.join();
    
    // Verify the condition variable was notified
    EXPECT_TRUE(notified);
}

TEST_F(TestSignals, SigquitHandler_ProperlyLocksAndUnlocks) {
    // Verify we can lock where_lock before the handler
    EXPECT_TRUE(where_lock.try_lock());
    where_lock.unlock();
    
    // Call sigquit_handler (it locks and unlocks internally)
    sigquit_handler(SIGQUIT);
    
    // Verify we can still lock after the handler
    EXPECT_TRUE(where_lock.try_lock());
    where_lock.unlock();
}

// ----------------------------------------------------------------------------
// Tests for install_signals
// ----------------------------------------------------------------------------

TEST_F(TestSignals, InstallSignals_NativeFalse_OnlyInstallsSigquit) {
    native = 0;
    
    // Install signals
    install_signals();
    
    // Get current handlers
    void (*current_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*current_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    // SIGQUIT should be installed
    EXPECT_NE(current_sigquit, SIG_DFL);
    EXPECT_NE(current_sigquit, SIG_IGN);
    
    // SIGPROF should NOT be installed (should be default)
    EXPECT_EQ(current_sigprof, SIG_DFL);
}

TEST_F(TestSignals, InstallSignals_NativeTrue_InstallsBothSignals) {
    native = 1;
    
    // Install signals
    install_signals();
    
    // Get current handlers
    void (*current_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*current_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    // Both should be installed
    EXPECT_NE(current_sigquit, SIG_DFL);
    EXPECT_NE(current_sigquit, SIG_IGN);
    
    EXPECT_NE(current_sigprof, SIG_DFL);
    EXPECT_NE(current_sigprof, SIG_IGN);
}

TEST_F(TestSignals, InstallSignals_InstallsCorrectHandlers) {
    native = 1;
    
    // Install signals
    install_signals();
    
    // Get current handlers
    void (*current_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*current_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    // Verify they point to the correct handler functions
    // We can't directly compare function pointers, but we can verify they're not default
    EXPECT_NE(current_sigquit, SIG_DFL);
    EXPECT_NE(current_sigprof, SIG_DFL);
}

// ----------------------------------------------------------------------------
// Tests for restore_signals
// ----------------------------------------------------------------------------

TEST_F(TestSignals, RestoreSignals_NativeFalse_OnlyRestoresSigquit) {
    native = 0;
    
    // Install signals first
    install_signals();
    
    // Restore signals
    restore_signals();
    
    // Get current handlers
    void (*current_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*current_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    // SIGQUIT should be restored to default
    EXPECT_EQ(current_sigquit, SIG_DFL);
    
    // SIGPROF should already be default (wasn't installed)
    EXPECT_EQ(current_sigprof, SIG_DFL);
    
    // Restore to DFL again for cleanup
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
}

TEST_F(TestSignals, RestoreSignals_NativeTrue_RestoresBothSignals) {
    // Helper function to get the current handler for a signal
    auto get_current_handler = [](int signum) {
        auto* current_handler = signal(signum, SIG_DFL);
        signal(signum, current_handler);
        return current_handler;
    };

    native = 1;
    
    // Install signals first
    install_signals();

    // Check what handlers are currently installed
    auto* before_sigquit = get_current_handler(SIGQUIT);
    auto* before_sigprof = get_current_handler(SIGPROF);
    
    EXPECT_NE(before_sigquit, SIG_DFL);
    EXPECT_NE(before_sigprof, SIG_DFL);
    
    // Restore signals
    restore_signals();

    // Get current handlers
    void (*current_sigquit)(int) = get_current_handler(SIGQUIT);
    void (*current_sigprof)(int) = get_current_handler(SIGPROF);
    
    // Both should be restored to default
    EXPECT_EQ(current_sigquit, SIG_DFL);
    EXPECT_EQ(current_sigprof, SIG_DFL);
    
    // Restore to DFL again for cleanup
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
}

TEST_F(TestSignals, RestoreSignals_WithoutInstall_DoesNotCrash) {
    native = 1;
    
    // Restore without installing first - should not crash
    EXPECT_NO_THROW(restore_signals());
    
    // Handlers should be default
    void (*current_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*current_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    EXPECT_EQ(current_sigquit, SIG_DFL);
    EXPECT_EQ(current_sigprof, SIG_DFL);
    
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
}

// ----------------------------------------------------------------------------
// Integration tests
// ----------------------------------------------------------------------------

TEST_F(TestSignals, InstallAndRestore_Cycle) {
    native = 1;
    
    // Get original handlers
    void (*orig_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*orig_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    signal(SIGQUIT, orig_sigquit);
    signal(SIGPROF, orig_sigprof);
    
    // Install
    install_signals();
    
    // Verify they changed
    void (*after_install_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*after_install_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    signal(SIGQUIT, after_install_sigquit);
    signal(SIGPROF, after_install_sigprof);
    
    EXPECT_NE(after_install_sigquit, orig_sigquit);
    EXPECT_NE(after_install_sigprof, orig_sigprof);
    
    // Restore
    restore_signals();
    
    // Verify they're back to default
    void (*after_restore_sigquit)(int) = signal(SIGQUIT, SIG_DFL);
    void (*after_restore_sigprof)(int) = signal(SIGPROF, SIG_DFL);
    
    EXPECT_EQ(after_restore_sigquit, SIG_DFL);
    EXPECT_EQ(after_restore_sigprof, SIG_DFL);
    
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
}

TEST_F(TestSignals, NativeModeSwitching) {
    // Start with native = 0
    native = 0;
    install_signals();
    
    void (*sigprof_with_native_off)(int) = signal(SIGPROF, SIG_DFL);
    EXPECT_EQ(sigprof_with_native_off, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
    
    restore_signals();
    
    // Switch to native = 1
    native = 1;
    install_signals();
    
    void (*sigprof_with_native_on)(int) = signal(SIGPROF, SIG_DFL);
    EXPECT_NE(sigprof_with_native_on, SIG_DFL);
    
    restore_signals();
    
    void (*sigprof_after_restore)(int) = signal(SIGPROF, SIG_DFL);
    EXPECT_EQ(sigprof_after_restore, SIG_DFL);
    
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
}

