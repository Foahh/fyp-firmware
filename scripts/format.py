import concurrent.futures
from pathlib import Path

from .common import require_tool, run

SEARCH_DIRS = ["Appli", "FSBL"]
EXTENSIONS = {".c", ".h", ".cpp", ".hpp"}
EXCLUDE_DIRS = {"build", ".cache", "Proto"}


def collect_files(project_root):
    files = []
    for d in SEARCH_DIRS:
        for path in (project_root / d).rglob("*"):
            if path.suffix not in EXTENSIONS:
                continue
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            files.append(path)
    return files


def format_batch(batch):
    run(["clang-format", "-i"] + [str(f) for f in batch])


def cmd_format(project_root, batch_size=32):
    require_tool("clang-format")

    files = collect_files(project_root)
    if not files:
        print("No files to format.")
        return

    batches = [files[i : i + batch_size] for i in range(0, len(files), batch_size)]
    print(f"Formatting {len(files)} files in {len(batches)} batches...")

    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = [executor.submit(format_batch, b) for b in batches]
        for f in concurrent.futures.as_completed(futures):
            f.result()

    print("Format complete.")
