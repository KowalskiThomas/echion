#include <gtest/gtest.h>

#include <echion/tasks.h>


#include "util.h"

struct TestGenInfo : public ::testing::Test {

    inline static std::vector<bool> copy_memory_failure_calls;
    inline static std::vector<std::tuple<proc_ref_t, void*, ssize_t, void*>> copy_memory_calls;

    static void reset_calls() {
        copy_memory_failure_calls.clear();
        copy_memory_calls.clear();
    }

    static void set_copy_memory_failure_calls(std::vector<bool> calls) {
        copy_memory_failure_calls = std::move(calls);
    }

    static void SetUpTestCase() {
        _set_pid(getpid());
    }

    void SetUp() override {
        Py_Initialize();

        TestGenInfo::reset_calls();
    }

    void TearDown() override {
        Py_Finalize();

        reset_calls();
    }
};

int copy_memory(proc_ref_t proc_ref, void* addr, ssize_t len, void* buf)
{
    TestGenInfo::copy_memory_calls.push_back(std::make_tuple(proc_ref, addr, len, buf));

    if (!TestGenInfo::copy_memory_failure_calls.empty()) {
        if (TestGenInfo::copy_memory_calls.size()-1 >= TestGenInfo::copy_memory_failure_calls.size()) {
            throw std::runtime_error("copy_memory_failure_calls does not have enough items, call count: " + std::to_string(TestGenInfo::copy_memory_calls.size()));
        }

        if (TestGenInfo::copy_memory_failure_calls[TestGenInfo::copy_memory_calls.size()-1]) {
            return -1;
        }
    }

    return real_cpp_function(copy_memory, proc_ref, addr, len, buf);
}

TEST_F(TestGenInfo, TestGenInfoThrowsIfCopyTypeFails) {
    TestGenInfo::set_copy_memory_failure_calls({true});

    PyObject* some_object = Py_None;

    ASSERT_THROW(GenInfo gen_info(some_object), GenInfo::Error);
    ASSERT_EQ(TestGenInfo::copy_memory_calls.size(), 1);
    ASSERT_EQ(std::get<0>(TestGenInfo::copy_memory_calls[0]), getpid());
    ASSERT_EQ(std::get<1>(TestGenInfo::copy_memory_calls[0]), some_object);
    ASSERT_EQ(std::get<2>(TestGenInfo::copy_memory_calls[0]), 80);
    // Check that the destination buffer is not nullptr
    ASSERT_NE(std::get<3>(TestGenInfo::copy_memory_calls[0]), nullptr);
}

TEST_F(TestGenInfo, DISABLED_TestGenInfoThrowsIfPyCoro_CheckExactFails) {
    // TODO: Test for PyCoro_CheckExact
}

TEST_F(TestGenInfo, DISABLED_TestGenInfoThrowsIfSecondCopyTypeFails) {
    // DISABLED for the time being until I mock PyCoro_CheckExact as well.

    TestGenInfo::set_copy_memory_failure_calls({false, true});

    PyObject* some_object = Py_None;

    ASSERT_THROW(GenInfo gen_info(some_object), GenInfo::Error);
    // This fails because the check for PyCoro_CheckExact is not being mocked
    ASSERT_EQ(TestGenInfo::copy_memory_calls.size(), 2);
    ASSERT_EQ(std::get<0>(TestGenInfo::copy_memory_calls[1]), 0);
    ASSERT_EQ(std::get<1>(TestGenInfo::copy_memory_calls[1]), some_object);
    ASSERT_EQ(std::get<2>(TestGenInfo::copy_memory_calls[1]), 80);
    // Check that the destination buffer is not nullptr
    ASSERT_NE(std::get<3>(TestGenInfo::copy_memory_calls[1]), nullptr);   
}

// TODO: Make it possible to mock PyGen_yf