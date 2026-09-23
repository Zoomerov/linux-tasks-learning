from importlib.metadata import metadata
from packaging.requirements import Requirement

m = metadata("matplotlib")

print("digraph matplotlib {")
print("  rankdir=LR;")
print("  node [shape=box];")

for dep in m.get_all("Requires-Dist") or []:
    name = Requirement(dep).name
    print(f'  "matplotlib" -> "{name}";')

print("}")
