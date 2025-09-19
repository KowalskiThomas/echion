#pragma once

#include <atomic>
#include <new>
#include <cstdlib>

namespace allocmon {
    inline std::atomic<size_t> g_new_calls{0};
    inline std::atomic<size_t> g_delete_calls{0};
    inline std::atomic<size_t> g_bytes{0};
    
    struct Snapshot { size_t news, deletes, bytes; };
    inline Snapshot snap() { return {g_new_calls.load(), g_delete_calls.load(), g_bytes.load()}; }
    inline Snapshot delta(Snapshot a, Snapshot b) { return {b.news-a.news, b.deletes-a.deletes, b.bytes-a.bytes}; }
    } // namespace allocmon
    
    // Override global operators (link this TU first in your test binary)
    void* operator new(std::size_t n) {
      allocmon::g_new_calls.fetch_add(1, std::memory_order_relaxed);
      allocmon::g_bytes.fetch_add(n, std::memory_order_relaxed);
      if (void* p = std::malloc(n)) return p;
      exit(1);
    }
    void operator delete(void* p) noexcept { 
      allocmon::g_delete_calls.fetch_add(1, std::memory_order_relaxed);
      std::free(p);
    }
    void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
      allocmon::g_new_calls.fetch_add(1, std::memory_order_relaxed);
      allocmon::g_bytes.fetch_add(n, std::memory_order_relaxed);
      return std::malloc(n);
    }
    void operator delete(void* p, const std::nothrow_t&) noexcept {
      allocmon::g_delete_calls.fetch_add(1, std::memory_order_relaxed);
      std::free(p);
}