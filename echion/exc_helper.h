#pragma once

#include <iostream>

struct Stats {
    std::string exception_type;
    size_t exception_count = 0;
    size_t return_count = 0;

    ~Stats() {
        std::cout << "Stats for " << exception_type << ": Return count: " << return_count << ", Exception count: " << exception_count << std::endl;
    }
};

template<typename Exception>
inline static void maybe_throw(bool do_throw) {
    static Stats stats {
        typeid(Exception).name(), 0, 0,  
    };
    
    stats.return_count++;
    if (do_throw) {
        stats.exception_count++;
        throw Exception{};
    }
}

template<>
inline void maybe_throw<std::runtime_error>(bool do_throw) {
    static Stats stats {
        "std::runtime_error", 0, 0,
    };
    
    stats.return_count++;
    if (stats.return_count%100==0) {
        std::cout << "std::runtime_error Return count: " << stats.return_count << ", Exception count: " << stats.exception_count << std::endl;
    }

    if (do_throw) {
        stats.exception_count++;
        throw std::runtime_error{"std::runtime_error"};
    }
}