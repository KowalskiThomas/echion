// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <dictobject.h>
#include <setobject.h>

#if PY_VERSION_HEX >= 0x030b0000
#define Py_BUILD_CORE
#if defined __GNUC__ && defined HAVE_STD_ATOMIC
#undef HAVE_STD_ATOMIC
#endif
#include <internal/pycore_dict.h>
#else
typedef struct
{
    Py_hash_t me_hash;
    PyObject* me_key;
    PyObject* me_value; /* This field is only meaningful for combined tables */
} PyDictKeyEntry;

typedef Py_ssize_t (*dict_lookup_func)(PyDictObject* mp, PyObject* key, Py_hash_t hash,
                                       PyObject** value_addr);

/* See dictobject.c for actual layout of DictKeysObject */
typedef struct _dictkeysobject
{
    Py_ssize_t dk_refcnt;

    /* Size of the hash table (dk_indices). It must be a power of 2. */
    Py_ssize_t dk_size;

    dict_lookup_func dk_lookup;

    /* Number of usable entries in dk_entries. */
    Py_ssize_t dk_usable;

    /* Number of used entries in dk_entries. */
    Py_ssize_t dk_nentries;

    char dk_indices[]; /* char is required to avoid strict aliasing. */

} PyDictKeysObject;

typedef PyObject* PyDictValues;
#endif

#include <unordered_set>

#include <echion/errors.h>
#include <echion/vm.h>

class MirrorObject
{
public:
    inline Result<PyObject*> reflect()
    {
        if (reflected == NULL)
            return Result<PyObject*>::error();
        return Result<PyObject*>(reflected);
    }

protected:
    std::unique_ptr<char[]> data = nullptr;
    PyObject* reflected = NULL;
};

// ----------------------------------------------------------------------------
class MirrorDict : public MirrorObject
{
public:
    static Result<MirrorDict> create(PyObject* dict_addr);

    Result<PyObject*> get_item(PyObject* key)
    {
        auto reflect_result = reflect();
        if (!reflect_result)
            return Result<PyObject*>::error();
        PyObject* item = PyDict_GetItem(*reflect_result, key);
        return Result<PyObject*>(item);
    }

    MirrorDict(MirrorDict&& other) noexcept = default;
    MirrorDict& operator=(MirrorDict&& other) noexcept = default;

private:
    MirrorDict() = default;
    MirrorDict(const MirrorDict&) = delete;
    MirrorDict& operator=(const MirrorDict&) = delete;
    PyDictObject dict;
};

Result<MirrorDict> MirrorDict::create(PyObject* dict_addr)
{
    MirrorDict mirror;
    
    if (copy_type(dict_addr, mirror.dict))
        return Result<MirrorDict>::error();

    PyDictKeysObject keys;
    if (copy_type(mirror.dict.ma_keys, keys))
        return Result<MirrorDict>::error();

    // Compute the full dictionary data size
#if PY_VERSION_HEX >= 0x030b0000
    size_t entry_size =
        keys.dk_kind == DICT_KEYS_UNICODE ? sizeof(PyDictUnicodeEntry) : sizeof(PyDictKeyEntry);
    size_t keys_size = sizeof(PyDictKeysObject) + (1 << keys.dk_log2_index_bytes) +
                       (keys.dk_nentries * entry_size);
#else
    size_t entry_size = sizeof(PyDictKeyEntry);
    size_t keys_size = sizeof(PyDictKeysObject) + (keys.dk_size * sizeof(Py_ssize_t)) +
                       (keys.dk_nentries * entry_size);
#endif
    size_t values_size = mirror.dict.ma_values != NULL ? keys.dk_nentries * sizeof(PyObject*) : 0;

    // Allocate the buffer
    ssize_t data_size = keys_size + (keys.dk_nentries * entry_size) + values_size;
    if (data_size < 0 || data_size > (1 << 20))
        return Result<MirrorDict>::error();

    mirror.data = std::make_unique<char[]>(data_size);

    // Copy the key data and update the pointer
    if (copy_generic(mirror.dict.ma_keys, mirror.data.get(), keys_size))
        return Result<MirrorDict>::error();

    mirror.dict.ma_keys = (PyDictKeysObject*)mirror.data.get();

    if (mirror.dict.ma_values != NULL)
    {
        // Copy the value data and update the pointer
        char* values_addr = mirror.data.get() + keys_size;
        if (copy_generic(mirror.dict.ma_values, values_addr, values_size))
            return Result<MirrorDict>::error();

        mirror.dict.ma_values = (PyDictValues*)values_addr;
    }

    mirror.reflected = (PyObject*)&mirror.dict;
    return Result<MirrorDict>(std::move(mirror));
}

// ----------------------------------------------------------------------------
class MirrorSet : public MirrorObject
{
public:
    static Result<MirrorSet> create(PyObject* set_addr);
    Result<std::unordered_set<PyObject*>> as_unordered_set();

    MirrorSet(MirrorSet&& other) noexcept = default;
    MirrorSet& operator=(MirrorSet&& other) noexcept = default;

private:
    MirrorSet() = default;
    MirrorSet(const MirrorSet&) = delete;
    MirrorSet& operator=(const MirrorSet&) = delete;
    size_t size;
    PySetObject set;
};

Result<MirrorSet> MirrorSet::create(PyObject* set_addr)
{
    MirrorSet mirror;
    
    if (copy_type(set_addr, mirror.set))
        return Result<MirrorSet>::error();

    mirror.size = mirror.set.mask + 1;
    ssize_t table_size = mirror.size * sizeof(setentry);
    if (table_size < 0 || table_size > (1 << 20))
        return Result<MirrorSet>::error();

    mirror.data = std::make_unique<char[]>(table_size);
    if (copy_generic(mirror.set.table, mirror.data.get(), table_size))
        return Result<MirrorSet>::error();

    mirror.set.table = (setentry*)mirror.data.get();
    mirror.reflected = (PyObject*)&mirror.set;
    
    return Result<MirrorSet>(std::move(mirror));
}

Result<std::unordered_set<PyObject*>> MirrorSet::as_unordered_set()
{
    if (data == nullptr)
        return Result<std::unordered_set<PyObject*>>::error();

    std::unordered_set<PyObject*> uset;

    for (size_t i = 0; i < size; i++)
    {
        auto entry = set.table[i];
        if (entry.key != NULL)
            uset.insert(entry.key);
    }

    return Result<std::unordered_set<PyObject*>>(std::move(uset));
}
