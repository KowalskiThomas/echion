
from tests.utils import DataSummary
from tests.utils import run_target
from tests.utils import dump_summary


def test_hack_wall_time():
    result, data = run_target("target_hack")
    assert result.returncode == 0, result.stderr.decode()

    assert data is not None
    md = data.metadata
    assert md["mode"] == "wall"
    assert md["interval"] == "1000"

    summary = DataSummary(data)
    dump_summary(summary, "summary_hack.json")
