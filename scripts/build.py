from .common import bin_to_hex, remove_if_exists, require_tool, run


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


def build_project(
    project_root, name, cfg, build_type, name_prefix, extra_cmake_args=None
):
    """Configure, compile, sign, and convert a single project to flashable HEX."""
    project_dir = project_root / cfg["sub_dir"]
    build_dir = project_dir / "build" / build_type
    full_name = f"{name_prefix}{name}"

    print(f"\n=== Building {name} ===")
    cmake_configure(project_dir, build_type, extra_cmake_args)
    cmake_build(project_dir, build_type)

    # Clean stale artifacts
    for suffix in ("-trusted.bin", ".bin"):
        remove_if_exists(build_dir / f"{full_name}{suffix}")

    elf = build_dir / f"{full_name}.elf"
    bin_file = build_dir / f"{full_name}.bin"
    signed_bin = build_dir / f"{full_name}-trusted.bin"
    signed_hex = build_dir / f"{full_name}-trusted.hex"

    elf_to_bin(elf, bin_file)
    sign_binary(bin_file, signed_bin, cfg)
    bin_to_hex(signed_bin, signed_hex, cfg["flash_address"])
    print(f"{name} done: {signed_hex}")


def cmd_build(project_root, projects, model, build_type, name_prefix):
    require_tool("cmake")
    require_tool("STM32_SigningTool_CLI")
    require_tool("arm-none-eabi-objcopy")

    print(f"Building firmware (model: {model['define']})")

    build_project(project_root, "FSBL", projects["FSBL"], build_type, name_prefix)

    appli_cmake_args = [
        f"-DMODEL_DEFINE={model['define']}",
        f"-DNETWORK_NAME={model['network_name']}",
    ]

    build_project(
        project_root,
        "Appli",
        projects["Appli"],
        build_type,
        name_prefix,
        extra_cmake_args=appli_cmake_args,
    )

    print("\n=== Build completed successfully ===")
