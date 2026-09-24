import subprocess


def git_text(*args):
    result = subprocess.run(
        ["git", *args],
        capture_output=True,
        text=True,
        check=True
    )
    return result.stdout


def git_bytes(*args):
    result = subprocess.run(
        ["git", *args],
        capture_output=True,
        check=True
    )
    return result.stdout


objects = git_text(
    "cat-file",
    "--batch-all-objects",
    "--batch-check=%(objectname) %(objecttype) %(objectsize)"
)


for line in objects.splitlines():
    object_hash, object_type, object_size = line.split()

    print("=" * 70)
    print(f"Hash: {object_hash}")
    print(f"Type: {object_type}")
    print(f"Size: {object_size}")
    print("-" * 70)

    content = git_bytes("cat-file", "-p", object_hash)

    try:
        print(content.decode("utf-8"))
    except UnicodeDecodeError:
        print("[Бинарный объект]")
        print(content.decode("utf-8", errors="backslashreplace"))
