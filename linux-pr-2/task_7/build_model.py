import json


def version_tuple(version):
    return tuple(map(int, version.split(".")))


def satisfies(version, rule):
    v = version_tuple(version)

    if rule.startswith("^"):
        base = version_tuple(rule[1:])

        if base[0] > 0:
            upper = (base[0] + 1, 0, 0)
        elif base[1] > 0:
            upper = (0, base[1] + 1, 0)
        else:
            upper = (0, 0, base[2] + 1)

        return base <= v < upper

    if rule.startswith(">="):
        return v >= version_tuple(rule[2:])

    if rule.startswith("<="):
        return v <= version_tuple(rule[2:])

    if rule.startswith(">"):
        return v > version_tuple(rule[1:])

    if rule.startswith("<"):
        return v < version_tuple(rule[1:])

    return v == version_tuple(rule)


with open("packages.json", encoding="utf-8") as f:
    data = json.load(f)

packages = data["packages"]

# Сортируем версии каждого пакета
versions = {}

for name, package_versions in packages.items():
    versions[name] = sorted(
        package_versions.keys(),
        key=version_tuple
    )


def variable_name(package):
    return package.replace("-", "_")


def dependency_constraint(package, rule):
    allowed = []

    for i, version in enumerate(versions[package], start=1):
        if satisfies(version, rule):
            allowed.append(i)

    var = variable_name(package)

    if not allowed:
        return "false"

    return "(" + " \\/ ".join(
        f"{var} = {i}" for i in allowed
    ) + ")"


lines = []

lines.append("% Automatically generated MiniZinc model")
lines.append("")

# 0 означает, что пакет не установлен
for package, package_versions in versions.items():
    var = variable_name(package)
    lines.append(
        f"var 0..{len(package_versions)}: {var};"
    )

lines.append("")

# Зависимости root
for dependency, rule in data["root"]["dependencies"].items():
    condition = dependency_constraint(dependency, rule)
    lines.append(f"constraint {condition};")

lines.append("")

# Зависимости остальных пакетов
for package, package_versions in packages.items():

    package_var = variable_name(package)

    for index, version in enumerate(versions[package], start=1):

        dependencies = package_versions[version]

        for dependency, rule in dependencies.items():

            condition = dependency_constraint(
                dependency,
                rule
            )

            lines.append(
                f"constraint ({package_var} = {index}) -> {condition};"
            )

lines.append("")

# Не устанавливаем лишние пакеты
installed = [
    f"bool2int({variable_name(p)} != 0)"
    for p in packages
]

lines.append(
    "solve minimize sum([" +
    ", ".join(installed) +
    "]);"
)

lines.append("")

# Массивы названий версий
for package, package_versions in versions.items():

    var = variable_name(package)

    values = ['"not installed"'] + [
        f'"{v}"' for v in package_versions
    ]

    lines.append(
        f"array[0..{len(package_versions)}] of string: "
        f"{var}_versions = "
        f"array1d(0..{len(package_versions)}, "
        f"[{', '.join(values)}]);"
    )

lines.append("")

lines.append("output [")

lines.append(
    f'  "root = {data["root"]["version"]}\\n",'
)

for package in packages:
    var = variable_name(package)

    lines.append(
        f'  "{package} = " ++ '
        f'{var}_versions[fix({var})] ++ "\\n",'
    )

lines.append("];")

with open("generated.mzn", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print("generated.mzn created")
