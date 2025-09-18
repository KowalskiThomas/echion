import json
import math
from typing import List, Tuple


def compute_stats(data: List[List[float]]) -> Tuple[float, float, float]:
    """Compute average, P99, and standard deviation for duration data."""
    # Flatten all individual timing measurements
    all_durations = []
    for batch in data:
        all_durations.extend(batch)
    
    # Calculate average
    avg_duration = sum(all_durations) / len(all_durations)
    
    # Calculate standard deviation
    variance = sum((d - avg_duration) ** 2 for d in all_durations) / len(all_durations)
    std_deviation = math.sqrt(variance)
    
    # Calculate P99
    sorted_durations = sorted(all_durations)
    p99_index = int(0.99 * len(sorted_durations))
    p99 = sorted_durations[p99_index]
    
    return avg_duration, p99, std_deviation


with open("output_no_exceptions.json", "r") as f:
    lines = f.read().splitlines()
    data_no_exceptions = [json.loads(line) for line in lines]

with open("output_exceptions.json", "r") as f:
    lines = f.read().splitlines()
    data_exceptions = [json.loads(line) for line in lines]

# Compute statistics for both datasets
avg_no_exc, p99_no_exc, std_no_exc = compute_stats(data_no_exceptions)
avg_exc, p99_exc, std_exc = compute_stats(data_exceptions)

# Display results
print("Performance Analysis Results")
print("=" * 40)
print(f"{'Metric':<20} {'No Exceptions':<15} {'With Exceptions':<15} {'Difference':<15}")
print("-" * 70)
print(f"{'Average (μs)':<20} {avg_no_exc*1e6:<15.2f} {avg_exc*1e6:<15.2f} {(avg_exc-avg_no_exc)*1e6:<15.2f}")
print(f"{'P99 (μs)':<20} {p99_no_exc*1e6:<15.2f} {p99_exc*1e6:<15.2f} {(p99_exc-p99_no_exc)*1e6:<15.2f}")
print(f"{'Std Dev (μs)':<20} {std_no_exc*1e6:<15.2f} {std_exc*1e6:<15.2f} {(std_exc-std_no_exc)*1e6:<15.2f}")

print("\nPerformance Impact:")
print(f"Average overhead: {((avg_exc/avg_no_exc - 1) * 100):.2f}%")
print(f"P99 overhead: {((p99_exc/p99_no_exc - 1) * 100):.2f}%")

