#include <iostream>
#include <chrono>
#include <stdexcept>

// Function using return code
int may_fail_return(int i) {
    if (i % 100 == 0) { // fail every 100th call
        return -1;
    }
    return 0;
}

// Function using exceptions
void may_fail_exception(int i) {
    if (i % 100 == 0) { // fail every 100th call
        throw std::runtime_error("Failure");
    }
}

int main() {
    constexpr int ITERATIONS = 1'000'000'000;

    // Benchmark return code approach
    auto start = std::chrono::high_resolution_clock::now();
    int errorCount = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        if (may_fail_return(i) != 0) {
            errorCount++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto return_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Benchmark exception approach
    start = std::chrono::high_resolution_clock::now();
    int exceptionCount = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        try {
            may_fail_exception(i);
        } catch (const std::exception&) {
            exceptionCount++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto exception_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Return codes: " << return_duration << " ms, errors = " << errorCount << '\n';
    std::cout << "Exceptions  : " << exception_duration << " ms, errors = " << exceptionCount << '\n';
}
