import concurrent.futures
from pathlib import Path

from .common import require_tool, run

C_SEARCH_DIRS = ["Appli", "FSBL"]
C_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"}
C_EXCLUDE_DIRS = {"build", ".cache", "Proto"}

PY_SEARCH_DIRS = ["scripts"]
PY_ROOT_FILES = ["project.py"]


def collect_c_files(project_root):
    files = []
    for d in C_SEARCH_DIRS:
        for path in (project_root / d).rglob("*"):
            if path.suffix not in C_EXTENSIONS:
                continue
            if any(part in C_EXCLUDE_DIRS for part in path.parts):
                continue
            files.append(path)
    return files


def collect_py_files(project_root):
    files = []
    for d in PY_SEARCH_DIRS:
        for path in (project_root / d).rglob("*.py"):
            files.append(path)
    for name in PY_ROOT_FILES:
        path = project_root / name
        if path.exists():
            files.append(path)
    return files


def format_c_batch(batch):
    run(["clang-format", "-i"] + [str(f) for f in batch])


def format_python(files):
    run(["ruff", "format"] + [str(f) for f in files])


def cmd_format(project_root, batch_size=32):
    require_tool("clang-format")
    require_tool("ruff")

    c_files = collect_c_files(project_root)
    py_files = collect_py_files(project_root)

    if not c_files and not py_files:
        print("No files to format.")
        return

    if c_files:
        batches = [
            c_files[i : i + batch_size] for i in range(0, len(c_files), batch_size)
        ]
        print(f"Formatting {len(c_files)} C/C++ files in {len(batches)} batches...")
        with concurrent.futures.ThreadPoolExecutor() as executor:
            futures = [executor.submit(format_c_batch, b) for b in batches]
            for f in concurrent.futures.as_completed(futures):
                f.result()

    if py_files:
        print(f"Formatting {len(py_files)} Python files...")
        format_python(py_files)

    print("Format complete.")
