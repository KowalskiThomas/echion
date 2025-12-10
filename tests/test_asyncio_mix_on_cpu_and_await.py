import sys

from tests.utils import PY, DataSummary, dump_summary, retry_on_valueerror, run_target


@retry_on_valueerror()
def test_asyncio_mix_on_cpu_and_await_wall_time():
    result, data = run_target("target_asyncio_mix_on_cpu_and_await")
    assert result.returncode == 0, result.stderr.decode()

    assert data is not None
    md = data.metadata
    assert md["mode"] == "wall"
    assert md["interval"] == "1000"

    summary = DataSummary(data)
    dump_summary(summary, "summary_asyncio_mix_on_cpu_and_await.json")

    # Test stacks and expected values
    if PY >= (3, 11):
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "main",
                "run",
                "Runner.run",
                "BaseEventLoop.run_until_complete",
                "BaseEventLoop.run_forever",
                "BaseEventLoop._run_once",
                "KqueueSelector.select"
                if sys.platform == "darwin"
                else "EpollSelector.select",
                "Task-1",
                "async_main",
                "mixed_workload",
                "dependency",
                "sleep",
            ),
            lambda v: v >= 0,
        )
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "main",
                "run",
                "Runner.run",
                "BaseEventLoop.run_until_complete",
                "BaseEventLoop.run_forever",
                "BaseEventLoop._run_once",
                "Handle._run",
                "Task-1",
                "async_main",
                "mixed_workload",
            ),
            lambda v: v >= 0,
        )

    else:
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "main",
                "run",
                "run_until_complete",
                "run_forever",
                "_run_once",
                "select",
                "Task-1",
                "async_main",
                "mixed_workload",
                "dependency",
                "sleep",
            ),
            lambda v: v >= 0,
        )
        summary.assert_substack(
            "0:MainThread",
            (
                "_run_module_as_main",
                "_run_code",
                "<module>",
                "main",
                "run",
                "run_until_complete",
                "run_forever",
                "_run_once",
                "_run",
                "Task-1",
                "async_main",
                "mixed_workload",
            ),
            lambda v: v >= 0,
        )
