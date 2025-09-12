from tests.utils import DataSummary
from austin.format.mojo import MojoFile

m = MojoFile(open("profiles2/test_memory.mojo", "rb"))

summary = DataSummary(m)
print(summary.threads)

# m.unwind()
# print(m.metadata)
# print(m.samples)


