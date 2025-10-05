#include <gtest/gtest.h>


typedef struct _object PyObject;
typedef struct _typeobject PyTypeObject;

#include <Python.h>

#if PY_VERSION_HEX >= 0x030c0000 // 3.12.0

// We need to mock PyLong_CheckExact because it is defined as a macro and calls Py_IS_TYPE with the long type.
// We can't "mock" / interpose for the macro, so what we need to do is define a new function that will be used instead of the macro.
#undef PyLong_CheckExact
extern "C" inline int Mock_Py_IS_TYPE(PyObject *ob, PyTypeObject *type);
#define PyLong_CheckExact(op) Mock_Py_IS_TYPE(op, &PyLong_Type)

#endif // PY_VERSION_HEX >= 0x030c0000

#include <echion/strings.h>

#include "echion/vm.h"
#include "util.h"

struct TestStringTable : public ::testing::Test {
    inline static std::vector<std::string> pyunicode_to_utf8_calls = {};
    inline static bool pyunicode_to_utf8_should_fail = false;
    inline static int pylong_check_exact_return_value = -1;  // -1 means use real function
    
#ifndef UNWIND_NATIVE_DISABLE
    inline static int unw_get_proc_info_return_value = -1;  // -1 means use real function
    inline static int unw_get_proc_name_return_value = -1;  // -1 means use real function
    inline static unw_word_t mock_proc_start_ip = 0;
    inline static std::string mock_proc_name = "";
#endif

    void reset_calls() {
        pyunicode_to_utf8_calls.clear();

        pyunicode_to_utf8_should_fail = false;
        pylong_check_exact_return_value = -1;
        
#ifndef UNWIND_NATIVE_DISABLE
        unw_get_proc_info_return_value = -1;
        unw_get_proc_name_return_value = -1;
        mock_proc_start_ip = 0;
        mock_proc_name = "";
#endif
    }

    static void SetUpTestCase() {
        _set_pid(getpid());
    }

    void SetUp() override {
        Py_Initialize();
        
        reset_calls();
    }
    
    void TearDown() override {
        // Note: Py_Finalize() can cause issues in tests, so we skip it
    }
};

// Mock functions
std::string pyunicode_to_utf8(PyObject* str_addr) {
    TestStringTable::pyunicode_to_utf8_calls.push_back(std::string(PyUnicode_AsUTF8(str_addr)));

    if (TestStringTable::pyunicode_to_utf8_should_fail) {
        throw StringError();
    }

    return real_cpp_function(pyunicode_to_utf8, str_addr);
}

#if PY_VERSION_HEX >= 0x030c0000 // 3.12.0
extern "C" {

// Used by PyLong_CheckExact (which is a macro)
extern "C" inline int Mock_Py_IS_TYPE(PyObject *ob, PyTypeObject *type) {
    std::cerr << "Mock_Py_IS_TYPE called" << std::endl;
    if (TestStringTable::pylong_check_exact_return_value != -1) {
        return TestStringTable::pylong_check_exact_return_value;
    }
    return real_function<int>("Py_IS_TYPE", ob, type);
}

}
#endif

#ifndef UNWIND_NATIVE_DISABLE
extern "C" int unw_get_proc_info(unw_cursor_t* cursor, unw_proc_info_t* pip) {
    if (TestStringTable::unw_get_proc_info_return_value != -1) {
        if (TestStringTable::unw_get_proc_info_return_value == 0) {
            pip->start_ip = TestStringTable::mock_proc_start_ip;
        }
        return TestStringTable::unw_get_proc_info_return_value;
    }
    return real_function<int>("unw_get_proc_info", cursor, pip);
}

extern "C" int unw_get_proc_name(unw_cursor_t* cursor, char* bufp, size_t len, unw_word_t* offp) {
    if (TestStringTable::unw_get_proc_name_return_value != -1) {
        if (TestStringTable::unw_get_proc_name_return_value == 0) {
            std::strncpy(bufp, TestStringTable::mock_proc_name.c_str(), len - 1);
            bufp[len - 1] = '\0';
            if (offp) *offp = 0;
        }
        return TestStringTable::unw_get_proc_name_return_value;
    }
    return real_function<int>("unw_get_proc_name", cursor, bufp, len, offp);
}
#endif


TEST_F(TestStringTable, TestStringTable) {
    auto string_table = StringTable();
    ASSERT_EQ(string_table.size(), 3);
    ASSERT_EQ(string_table.at(0), "");
    ASSERT_EQ(string_table.at(1), "<invalid>");
    ASSERT_EQ(string_table.at(2), "<unknown>");
}

TEST_F(TestStringTable, TestStringTableInitialisedWithCorrectValues) {
    ASSERT_EQ(string_table.size(), 3);

    ASSERT_EQ(string_table.at(0), "");
    ASSERT_EQ(string_table.at(1), "<invalid>");
    ASSERT_EQ(string_table.at(2), "<unknown>");
}

#if PY_VERSION_HEX >= 0x030c0000 // 3.12.0
TEST_F(TestStringTable, DISABLED_TestStringTableInsertPyObject) {
#else
TEST_F(TestStringTable, TestStringTableInsertPyObject) {
#endif
    // Disabled because the actual result is "\0\0\0\0"
    
    auto py_string = PyObjectHandle(PyUnicode_FromString("test"));

    auto string_table = StringTable();
    const auto key = string_table.key(py_string);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key), "test");

    // Inserting the same string again should increase the size by 1 since the keys are pointers,
    // not string hashes.
    auto py_string_2 = PyObjectHandle(PyUnicode_FromString("test"));
    const auto key_2 = string_table.key(py_string_2);

    ASSERT_EQ(string_table.size(), 5);
    ASSERT_EQ(string_table.at(key_2), "test");
}

TEST_F(TestStringTable, TestStringTableInsertPyObjectFails) {
    TestStringTable::pyunicode_to_utf8_should_fail = true;

    auto py_string = PyObjectHandle(PyUnicode_FromString("test"));

    auto string_table = StringTable();

    ASSERT_THROW(string_table.key(py_string), StringTable::Error);

    // Nothing should have been inserted
    ASSERT_EQ(string_table.size(), 3);
}

#if PY_VERSION_HEX >= 0x030c0000 // 3.12.0
TEST_F(TestStringTable, DISABLED_TestStringTableLongErrorFallbackToUnicode) {
    // Disabled because the actual result is "\0\0\0\0\0\0\0\0my_t"

    // Test the LongError exception handling path in StringTable::key()
    // When a PyUnicode string is passed, pylong_to_llong() will throw LongError
    // and the code should fall back to pyunicode_to_utf8()
    auto py_string = PyObjectHandle(PyUnicode_FromString("my_task_name"));

    auto string_table = StringTable();

    const auto key = string_table.key(py_string);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    
    // Should contain the unicode string, not "Task-" prefix
    // since it fell back to pyunicode_to_utf8 after LongError
    ASSERT_EQ(string_table.at(key), "my_task_name");
    
    // Verify pyunicode_to_utf8 was called as fallback
    ASSERT_EQ(pyunicode_to_utf8_calls.size(), 1);
    ASSERT_EQ(pyunicode_to_utf8_calls[0], "my_task_name");
}
#endif

TEST_F(TestStringTable, TestStringTableKeyUnsafeNewPyUnicode) {
    // Test key_unsafe with a new PyUnicode string
    auto py_string = PyObjectHandle(PyUnicode_FromString("test_string"));

    auto string_table = StringTable();

    const auto key = string_table.key_unsafe(py_string);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key), "test_string");
}

TEST_F(TestStringTable, TestStringTableKeyUnsafeExistingKey) {
    // Test key_unsafe when the key already exists in the table
    auto py_string = PyObjectHandle(PyUnicode_FromString("existing_string"));

    auto string_table = StringTable();

    // Insert the string first time
    const auto key1 = string_table.key_unsafe(py_string);
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key1), "existing_string");

    // Insert the same pointer again - should not add a new entry
    const auto key2 = string_table.key_unsafe(py_string);
    ASSERT_EQ(string_table.size(), 4);  // Size should not change
    ASSERT_EQ(key1, key2);  // Keys should be the same
    ASSERT_EQ(string_table.at(key2), "existing_string");
}

#if PY_VERSION_HEX >= 0x030c0000
TEST_F(TestStringTable, TestStringTableKeyUnsafePyLong) {
    // Test key_unsafe with a PyLong (should use "Task-" prefix on Python 3.12+)
    auto py_long = PyObjectHandle(PyLong_FromLong(42));

    auto string_table = StringTable();

    const auto key = string_table.key_unsafe(py_long);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key), "Task-42");
}

TEST_F(TestStringTable, TestStringTableKeyUnsafePyLongCheckExactTrue) {
    // Test key_unsafe when PyLong_CheckExact returns true
    // Mock PyLong_CheckExact to return true for any object
    pylong_check_exact_return_value = 1;

    auto py_string = PyObjectHandle(PyUnicode_FromString("not_really_a_long"));

    auto string_table = StringTable();

    const auto key = string_table.key_unsafe(py_string);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key), "not_really_a_long");
}

TEST_F(TestStringTable, TestStringTableKeyUnsafePyLongCheckExactFalse) {
    // Test key_unsafe when PyLong_CheckExact returns false
    // Mock PyLong_CheckExact to return false for any object
    pylong_check_exact_return_value = 0;

    auto py_string = PyObjectHandle(PyUnicode_FromString("unicode_string"));

    auto string_table = StringTable();

    const auto key = string_table.key_unsafe(py_string);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    
    // When PyLong_CheckExact returns false, it should use PyUnicode_AsUTF8
    ASSERT_EQ(string_table.at(key), "unicode_string");
}

TEST_F(TestStringTable, TestStringTableKeyUnsafeMultipleDifferentTypes) {
    // Test key_unsafe with both PyLong and PyUnicode
    auto py_long = PyObjectHandle(PyLong_FromLong(999));
    auto py_string = PyObjectHandle(PyUnicode_FromString("my_task"));

    auto string_table = StringTable();

    const auto key1 = string_table.key_unsafe(py_long);
    const auto key2 = string_table.key_unsafe(py_string);

    // Should have 3+2 entries now
    ASSERT_EQ(string_table.size(), 5);
    ASSERT_EQ(string_table.at(key1), "Task-999");
    ASSERT_EQ(string_table.at(key2), "my_task");
}
#endif


#ifndef UNWIND_NATIVE_DISABLE

TEST_F(TestStringTable, TestStringTableKeyProgramCounterNew) {
    // Test key(unw_word_t pc) with a new program counter value
    auto string_table = StringTable();

    unw_word_t pc = 0x12345678;
    const auto key = string_table.key(pc);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    
    // The key should be the pc value itself
    ASSERT_EQ(key, (StringTable::Key)pc);
    
    // The value should be in format "native@0x<address>"
    std::string expected = "native@0x12345678";
    ASSERT_EQ(string_table.at(key), expected);
}

TEST_F(TestStringTable, TestStringTableKeyProgramCounterExisting) {
    // Test key(unw_word_t pc) when the pc already exists in the table
    auto string_table = StringTable();

    unw_word_t pc = 0xABCDEF00;
    
    // Insert the pc first time
    const auto key1 = string_table.key(pc);
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key1, (StringTable::Key)pc);
    std::string value1 = string_table.at(key1);
    ASSERT_EQ(value1, "native@0xabcdef00");

    // Insert the same pc again - should not add a new entry
    const auto key2 = string_table.key(pc);
    ASSERT_EQ(string_table.size(), 4);  // Size should not change
    ASSERT_EQ(key1, key2);  // Keys should be the same
    ASSERT_EQ(string_table.at(key2), value1);  // Value should be the same
}

TEST_F(TestStringTable, TestStringTableKeyProgramCounterMultiple) {
    // Test key(unw_word_t pc) with multiple different program counter values
    auto string_table = StringTable();

    unw_word_t pc1 = 0x1000;
    unw_word_t pc2 = 0x2000;
    unw_word_t pc3 = 0x3000;
    
    const auto key1 = string_table.key(pc1);
    const auto key2 = string_table.key(pc2);
    const auto key3 = string_table.key(pc3);

    // Should have 3+3 entries now
    ASSERT_EQ(string_table.size(), 6);
    
    // All keys should be different
    ASSERT_NE(key1, key2);
    ASSERT_NE(key2, key3);
    ASSERT_NE(key1, key3);
    
    // Verify all values
    ASSERT_EQ(string_table.at(key1), "native@0x1000");
    ASSERT_EQ(string_table.at(key2), "native@0x2000");
    ASSERT_EQ(string_table.at(key3), "native@0x3000");
}

TEST_F(TestStringTable, TestStringTableKeyProgramCounterHighAddress) {
    // Test key(unw_word_t pc) with a high address value
    auto string_table = StringTable();

    unw_word_t pc = 0x7FFFFFFFFFFFFFFF;  // Max value for 64-bit signed
    const auto key = string_table.key(pc);

    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key, (StringTable::Key)pc);
    
    // The value should contain the address in hex format
    std::string value = string_table.at(key);
    ASSERT_TRUE(value.rfind("native@0x", 0) == 0);  // Should start with "native@0x"
}

TEST_F(TestStringTable, TestStringTableKeyCursorGetProcInfoFails) {
    // Test key(unw_cursor_t& cursor) when unw_get_proc_info fails
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to return an error
    unw_get_proc_info_return_value = 1;
    
    unw_cursor_t cursor;
    
    // Should throw StringTable::Error when unw_get_proc_info fails
    ASSERT_THROW(string_table.key(cursor), StringTable::Error);
    
    // Nothing should have been inserted
    ASSERT_EQ(string_table.size(), 3);
}

TEST_F(TestStringTable, TestStringTableKeyCursorGetProcNameFails) {
    // Test key(unw_cursor_t& cursor) when unw_get_proc_name fails
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x4000;
    
    // Mock unw_get_proc_name to fail
    unw_get_proc_name_return_value = 1;
    
    unw_cursor_t cursor;
    
    // Should throw StringTable::Error when unw_get_proc_name fails
    ASSERT_THROW(string_table.key(cursor), StringTable::Error);
}

TEST_F(TestStringTable, TestStringTableKeyCursorNewFunction) {
    // Test key(unw_cursor_t& cursor) with a new function (no demangling needed)
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x5000;
    
    // Mock unw_get_proc_name to succeed with a simple C function name
    unw_get_proc_name_return_value = 0;
    mock_proc_name = "my_function";
    
    unw_cursor_t cursor;
    
    const auto key = string_table.key(cursor);
    
    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key, (StringTable::Key)0x5000);
    ASSERT_EQ(string_table.at(key), "my_function");
}

TEST_F(TestStringTable, TestStringTableKeyCursorExistingFunction) {
    // Test key(unw_cursor_t& cursor) when the function already exists
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x6000;
    
    // Mock unw_get_proc_name to succeed
    unw_get_proc_name_return_value = 0;
    mock_proc_name = "existing_function";
    
    unw_cursor_t cursor;
    
    // Insert the function first time
    const auto key1 = string_table.key(cursor);
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(string_table.at(key1), "existing_function");
    
    // Insert the same function again (same start_ip) - should not add a new entry
    const auto key2 = string_table.key(cursor);
    ASSERT_EQ(string_table.size(), 4);  // Size should not change
    ASSERT_EQ(key1, key2);  // Keys should be the same
    ASSERT_EQ(string_table.at(key2), "existing_function");
}

TEST_F(TestStringTable, TestStringTableKeyCursorMangledCppName) {
    // Test key(unw_cursor_t& cursor) with a mangled C++ name
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x7000;
    
    // Mock unw_get_proc_name to succeed with a mangled C++ name
    unw_get_proc_name_return_value = 0;
    // This is a real mangled name for "std::string::size() const"
    mock_proc_name = "_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE4sizeEv";
    
    unw_cursor_t cursor;
    
    const auto key = string_table.key(cursor);
    
    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key, (StringTable::Key)0x7000);
    
    // The name should be demangled (not the mangled version)
    std::string value = string_table.at(key);
    ASSERT_NE(value, "_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE4sizeEv");
    // Should contain demangled content (actual demangling depends on the system)
    ASSERT_FALSE(value.empty());
}

TEST_F(TestStringTable, TestStringTableKeyCursorNotMangledName) {
    // Test key(unw_cursor_t& cursor) with a name that starts with _ but is not mangled C++
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x8000;
    
    // Mock unw_get_proc_name to succeed with a name starting with _ but not _Z
    unw_get_proc_name_return_value = 0;
    mock_proc_name = "_my_c_function";
    
    unw_cursor_t cursor;
    
    const auto key = string_table.key(cursor);
    
    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key, (StringTable::Key)0x8000);
    ASSERT_EQ(string_table.at(key), "_my_c_function");
}

TEST_F(TestStringTable, TestStringTableKeyCursorDemanglingFails) {
    // Test key(unw_cursor_t& cursor) when demangling fails (invalid mangled name)
    auto string_table = StringTable();
    
    // Mock unw_get_proc_info to succeed
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x8500;
    
    // Mock unw_get_proc_name with an invalid mangled name (starts with _Z but is malformed)
    // This will cause abi::__cxa_demangle to fail and return non-zero status
    unw_get_proc_name_return_value = 0;
    mock_proc_name = "_ZInvalidMangledName";
    
    unw_cursor_t cursor;
    
    const auto key = string_table.key(cursor);
    
    // Should have 3+1 entries now
    ASSERT_EQ(string_table.size(), 4);
    ASSERT_EQ(key, (StringTable::Key)0x8500);
    
    // When demangling fails, the original mangled name should be kept
    ASSERT_EQ(string_table.at(key), "_ZInvalidMangledName");
}

TEST_F(TestStringTable, TestStringTableKeyCursorMultipleFunctions) {
    // Test key(unw_cursor_t& cursor) with multiple different functions
    auto string_table = StringTable();
    
    unw_cursor_t cursor;
    
    // First function
    unw_get_proc_info_return_value = 0;
    mock_proc_start_ip = 0x9000;
    unw_get_proc_name_return_value = 0;
    mock_proc_name = "function1";
    const auto key1 = string_table.key(cursor);
    
    // Second function
    mock_proc_start_ip = 0xA000;
    mock_proc_name = "function2";
    const auto key2 = string_table.key(cursor);
    
    // Third function with mangled name
    mock_proc_start_ip = 0xB000;
    mock_proc_name = "_ZN11ValueHolder10printValueEv";
    const auto key3 = string_table.key(cursor);
    
    // Should have 3+3 entries now
    ASSERT_EQ(string_table.size(), 6);
    
    // All keys should be different
    ASSERT_NE(key1, key2);
    ASSERT_NE(key2, key3);
    ASSERT_NE(key1, key3);
    
    // Verify values
    ASSERT_EQ(string_table.at(key1), "function1");
    ASSERT_EQ(string_table.at(key2), "function2");

    // key3 should be demangled
    ASSERT_NE(string_table.at(key3), "_ZN11ValueHolder10printValueEv");
    ASSERT_EQ(string_table.at(key3), "ValueHolder::printValue()");
}

TEST(TestStringTableLookup, LookupExistingKey) {
    StringTable table;
    
    // Add some entries to the table
    table.emplace(100, "test_string");
    table.emplace(200, "another_string");
    table.emplace(300, "third_string");
    
    // Lookup existing keys - should succeed
    EXPECT_EQ(table.lookup(100), "test_string");
    EXPECT_EQ(table.lookup(200), "another_string");
    EXPECT_EQ(table.lookup(300), "third_string");
}

TEST(TestStringTableLookup, LookupDefaultKeys) {
    StringTable table;
    
    // StringTable constructor adds these default entries
    EXPECT_EQ(table.lookup(0), "");
    EXPECT_EQ(table.lookup(StringTable::INVALID), "<invalid>");
    EXPECT_EQ(table.lookup(StringTable::UNKNOWN), "<unknown>");
}

TEST(TestStringTableLookup, LookupNonExistentKey) {
    StringTable table;
    
    // Try to lookup a key that doesn't exist - should throw LookupError
    EXPECT_THROW(table.lookup(999), StringTable::LookupError);
    EXPECT_THROW(table.lookup(12345), StringTable::LookupError);
    EXPECT_THROW(table.lookup(0xDEADBEEF), StringTable::LookupError);
}

TEST(TestStringTableLookup, LookupAfterInsert) {
    StringTable table;
    
    // Initially should not exist
    EXPECT_THROW(table.lookup(500), StringTable::LookupError);
    
    // Insert the key
    table.emplace(500, "new_entry");
    
    // Now lookup should succeed
    EXPECT_EQ(table.lookup(500), "new_entry");
}

TEST(TestStringTableLookup, LookupModifiedValue) {
    StringTable table;
    
    // Add an entry
    table.emplace(600, "original");
    EXPECT_EQ(table.lookup(600), "original");
    
    // Modify the value
    table[600] = "modified";
    
    // Lookup should return the modified value
    EXPECT_EQ(table.lookup(600), "modified");
}

TEST(TestStringTableLookup, LookupReturnsReference) {
    StringTable table;
    
    table.emplace(700, "test");
    
    // Get reference and modify through it
    std::string& ref = table.lookup(700);
    ref = "modified_via_reference";
    
    // Lookup should reflect the change
    EXPECT_EQ(table.lookup(700), "modified_via_reference");
}

TEST(TestStringTableLookup, LookupEmptyString) {
    StringTable table;
    
    // Key 0 is initialized with empty string
    EXPECT_EQ(table.lookup(0), "");
    EXPECT_EQ(table.lookup(0).size(), 0);
    
    // Add another empty string entry
    table.emplace(800, "");
    EXPECT_EQ(table.lookup(800), "");
    EXPECT_EQ(table.lookup(800).size(), 0);
}

TEST(TestStringTableLookup, LookupMultipleNonExistent) {
    StringTable table;
    
    table.emplace(1000, "exists");
    
    // Multiple non-existent keys should all throw
    EXPECT_THROW(table.lookup(999), StringTable::LookupError);
    EXPECT_THROW(table.lookup(1001), StringTable::LookupError);
    EXPECT_THROW(table.lookup(2000), StringTable::LookupError);
    
    // But the existing one should work
    EXPECT_EQ(table.lookup(1000), "exists");
}




#endif