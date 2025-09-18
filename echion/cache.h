// This file is part of "echion" which is released under MIT.
//
// Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

#pragma once

#include <echion/errors.h>
#include <list>
#include <memory>
#include <unordered_map>

#define CACHE_MAX_ENTRIES 2048

template <typename K, typename V>
class LRUCache
{
public:
    LRUCache(size_t capacity) : capacity(capacity) {}

    Result<V*> lookup(const K& k);

    Result<void> store(const K& k, std::unique_ptr<V> v);

private:
    size_t capacity;
    std::list<std::pair<K, std::unique_ptr<V>>> items;
    std::unordered_map<K, typename std::list<std::pair<K, std::unique_ptr<V>>>::iterator> index;
};

template <typename K, typename V>
Result<void> LRUCache<K, V>::store(const K& k, std::unique_ptr<V> v)
{
    if (!v)
        return Result<void>::error();

    // Check if cache is full
    if (items.size() >= capacity)
    {
        index.erase(items.back().first);
        items.pop_back();
    }

    // Insert the new item at front of the list
    items.emplace_front(k, std::move(v));

    // Insert in the map
    index[k] = items.begin();

    return Result<void>::ok();
}

template <typename K, typename V>
Result<V*> LRUCache<K, V>::lookup(const K& k)
{
    auto itr = index.find(k);
    if (itr == index.end())
        return Result<V*>::error();

    // Move to the front of the list
    items.splice(items.begin(), items, itr->second);

    return Result<V*>(itr->second->second.get());
}
