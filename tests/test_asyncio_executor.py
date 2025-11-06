import json

from tests.utils import PY, DataSummary, run_target


def test_asyncio_gather_wall_time():
    result, data = run_target("target_async_executor")
    assert result.returncode == 0, result.stderr.decode()

    assert data is not None
    md = data.metadata
    assert md["mode"] == "wall"
    assert md["interval"] == "1000"

    summary = DataSummary(data)

    expected_nthreads = 3
    assert summary.nthreads == expected_nthreads, summary.threads
    assert summary.total_metric >= 1.4 * 1e6

    summary_json = {}
    for thread in summary.threads:
        summary_json[thread] = [
            {
                "stack": key,
                "metric": value,
            }
            for key, value in summary.threads[thread].items()
        ]

    with open("summary.json", "w") as f:
        json.dump(summary_json, f, indent=2)

    # Test stacks and expected values
    if PY >= (3, 11):
        # Main Thread
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "run",
                "Runner.run",
                "BaseEventLoop.run_until_complete",
                "BaseEventLoop.run_forever",
                "BaseEventLoop._run_once",
                "KqueueSelector.select",
                "Task-1",
                "main",
                "asynchronous_function",
            ),
            lambda v: v >= 0.1e6,
        )

        # Thread Pool Executor
        summary.assert_substack(
            "0:asyncio_0",
            (
                "Thread._bootstrap",
                "thread_bootstrap_inner",
                "Thread._bootstrap_inner",
                "Thread.run",
                "_worker",
                "_WorkItem.run",
                "slow_sync_function",
            ),
            lambda v: v >= 0.1e6,
        )
    else:
        # Main Thread
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "run",
                "run_until_complete",
                "run_forever",
                "_run_once",
                "select",
                "Task-1",
                "main",
                "asynchronous_function",
            ),
            lambda v: v >= 0.1e6,
        )

        # Thread Pool Executor
        summary.assert_substack(
            "0:asyncio_0",
            (
                "_bootstrap",
                "thread_bootstrap_inner",
                "_bootstrap_inner",
                "run",
                "_worker",
                "run",
                "slow_sync_function"
            ),
            lambda v: v >= 0.1e6,
        )
