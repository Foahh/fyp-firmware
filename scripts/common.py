import hashlib
import shutil
import stat
import subprocess
import sys

SIGN_CACHE_FILE = ".sign_cache"


def run(args, **kwargs):
    """Run a subprocess, printing the command. Raises on failure."""
    print(f"  > {' '.join(str(a) for a in args)}")
    result = subprocess.run(args, **kwargs)
    if result.returncode != 0:
        cmd_str = " ".join(str(a) for a in args)
        raise RuntimeError(f"Command failed (exit {result.returncode}): {cmd_str}")


def require_tool(name):
    if shutil.which(name):
        return
    print(f"ERROR: '{name}' not found on PATH.", file=sys.stderr)
    sys.exit(1)


def remove_if_exists(path):
    """Delete a file, handling read-only flags (common on Windows build artifacts)."""
    if path.exists():
        path.chmod(path.stat().st_mode | stat.S_IWRITE)
        path.unlink()


def file_hash(path):
    """Compute SHA-256 hex digest of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def read_cached_hash(elf_file, cache_file):
    """Read the previously cached hash for a file, or None."""
    if not cache_file.exists():
        return None
    try:
        for line in cache_file.read_text().splitlines():
            name, _, digest = line.partition("=")
            if name.strip() == elf_file.name:
                return digest.strip()
    except OSError:
        pass
    return None


def write_cached_hash(elf_file, digest, cache_file):
    """Write/update the cached hash for a file."""
    entries = {}
    if cache_file.exists():
        try:
            for line in cache_file.read_text().splitlines():
                name, _, d = line.partition("=")
                if name.strip():
                    entries[name.strip()] = d.strip()
        except OSError:
            pass
    entries[elf_file.name] = digest
    cache_file.write_text(
        "\n".join(f"{k}={v}" for k, v in sorted(entries.items())) + "\n"
    )


AXISRAM2_END = 0x3420_0000
APPLI_SRAM_ORIGIN = 0x3400_0400
MAX_CPURAM2_SIZE_KB = 1024


def compute_memory_replacements(sram_size_kb):
    """Derive mpool/linker placeholder values from a model's sram_size_kb."""
    if not 1 <= sram_size_kb <= MAX_CPURAM2_SIZE_KB:
        raise ValueError(
            f"sram_size_kb must be 1..{MAX_CPURAM2_SIZE_KB}, got {sram_size_kb}"
        )
    cpuram2_offset = AXISRAM2_END - sram_size_kb * 1024
    sram_length = cpuram2_offset - APPLI_SRAM_ORIGIN
    if sram_length <= 0:
        raise ValueError(
            f"sram_size_kb={sram_size_kb} leaves no SRAM for Appli "
            f"(AXISRAM1_2_S length would be {sram_length})"
        )
    return {
        "#CPURAM2_OFFSET#": f"0x{cpuram2_offset:08x}",
        "#CPURAM2_SIZE_KB#": str(sram_size_kb),
        "#SRAM_LENGTH#": f"0x{sram_length:X}",
    }


def generate_from_template(template_path, output_path, replacements):
    """Read *template_path*, substitute every key in *replacements*, write *output_path*."""
    content = template_path.read_text()
    for tag, value in replacements.items():
        content = content.replace(tag, value)
    output_path.write_text(content)
    print(f"  Generated {output_path} from {template_path.name}")


def bin_to_hex(bin_file, hex_file, base_address):
    remove_if_exists(hex_file)
    run(
        [
            "arm-none-eabi-objcopy",
            "-I",
            "binary",
            "-O",
            "ihex",
            "--change-addresses",
            base_address,
            str(bin_file),
            str(hex_file),
        ]
    )
    if not hex_file.exists():
        raise RuntimeError(f"HEX conversion failed: {hex_file}")
