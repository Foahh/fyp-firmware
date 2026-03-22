from .common import (
    SIGN_CACHE_FILE,
    bin_to_hex,
    file_hash,
    read_cached_hash,
    remove_if_exists,
    require_tool,
    run,
    write_cached_hash,
)


def cmake_configure(project_dir, build_type, extra_args=None):
    cmd = ["cmake", "--preset", build_type]
    if extra_args:
        cmd.extend(extra_args)
    run(cmd, cwd=str(project_dir))


def cmake_build(project_dir, build_type):
    run(["cmake", "--build", "--preset", build_type], cwd=str(project_dir))


def elf_to_bin(elf, bin_file):
    if not elf.exists():
        raise RuntimeError(f"ELF not found: {elf}")
    run(["arm-none-eabi-objcopy", "-O", "binary", str(elf), str(bin_file)])


def sign_binary(bin_file, signed_bin, cfg):
    remove_if_exists(signed_bin)
    run(
        [
            "STM32_SigningTool_CLI",
            "-bin",
            str(bin_file),
            "-nk",
            "-of",
            cfg["offset_address"],
            "-t",
            cfg["signing_type"],
            "-o",
            str(signed_bin),
            "-hv",
            "2.3",
            "-dump",
            str(signed_bin),
            "-align",
        ]
    )


def build_project(project_root, name, cfg, build_type, extra_cmake_args=None):
    """Configure and compile a single project."""
    project_dir = project_root / cfg["sub_dir"]

    print(f"\n=== Building {name} ===")
    cmake_configure(project_dir, build_type, extra_cmake_args)
    cmake_build(project_dir, build_type)


def sign_project(project_root, name, cfg, build_type, name_prefix, force=False):
    """Sign and convert a compiled project to flashable HEX. Skips if ELF unchanged."""
    project_dir = project_root / cfg["sub_dir"]
    build_dir = project_dir / "build" / build_type
    full_name = f"{name_prefix}{name}"

    elf = build_dir / f"{full_name}.elf"
    cache = build_dir / SIGN_CACHE_FILE
    digest = file_hash(elf)
    if not force and read_cached_hash(elf, cache) == digest:
        print(f"\n--- Skipping sign {name} (unchanged) ---")
        return

    bin_file = build_dir / f"{full_name}.bin"
    signed_bin = build_dir / f"{full_name}-trusted.bin"
    signed_hex = build_dir / f"{full_name}-trusted.hex"
    for suffix in ("-trusted.bin", ".bin"):
        remove_if_exists(build_dir / f"{full_name}{suffix}")
    elf_to_bin(elf, bin_file)
    sign_binary(bin_file, signed_bin, cfg)
    bin_to_hex(signed_bin, signed_hex, cfg["flash_address"])
    write_cached_hash(elf, digest, cache)
    print(f"{name} done: {signed_hex}")


def cmd_build(
    project_root,
    projects,
    model,
    build_type,
    name_prefix,
    appli=True,
    fsbl=True,
    sign=True,
    force=False,
    snapshot=False,
    performance=False,
):
    require_tool("cmake")

    appli_cmake_args = [
        f"-DMODEL_DEFINE={model['define']}",
        f"-DNETWORK_NAME={model['network_name']}",
    ]

    if snapshot:
        appli_cmake_args.append("-DCAMERA_NN_SNAPSHOT_MODE=1")

    if performance:
        appli_cmake_args.append("-DPERFORMANCE_MODE=1")

    print(f"Building firmware (model: {model['define']})")

    if sign:
        require_tool("STM32_SigningTool_CLI")
        require_tool("arm-none-eabi-objcopy")

    if fsbl:
        build_project(project_root, "FSBL", projects["FSBL"], build_type)
        if sign:
            sign_project(
                project_root,
                "FSBL",
                projects["FSBL"],
                build_type,
                name_prefix,
                force=force,
            )

    if appli:
        build_project(
            project_root, "Appli", projects["Appli"], build_type, appli_cmake_args
        )
        if sign:
            sign_project(
                project_root,
                "Appli",
                projects["Appli"],
                build_type,
                name_prefix,
                force=force,
            )

    print("\n=== Build completed successfully ===")
