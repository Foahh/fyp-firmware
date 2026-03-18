import shutil


def cmd_clean(project_root):
    dirs = [
        project_root / "FSBL" / "build",
        project_root / "Appli" / "build",
        project_root / "Appli" / ".cache",
        project_root / "build",
    ]
    removed = 0
    for d in dirs:
        if d.exists():
            shutil.rmtree(d)
            print(f"Removed: {d}")
            removed += 1
    if removed == 0:
        print("Nothing to clean.")
    else:
        print(f"Cleaned {removed} director{'y' if removed == 1 else 'ies'}.")
