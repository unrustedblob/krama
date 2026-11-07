import argparse
import sys
from collections import Counter
from pathlib import Path
from typing import NamedTuple

PROG = sys.argv[0]

MIN_VERSION = (3, 13)

# This check only fires for version 3.12, any version before it would fail with
# a SyntaxError
if sys.version_info < MIN_VERSION:
    sys.exit(
        f"{PROG}: VersionError: Minimum python version {MIN_VERSION[0]}.{MIN_VERSION[1]} required. Current {sys.version_info.major}.{sys.version_info.minor}"
    )

# We check the minimum python version before the import
import tomllib


class Context(NamedTuple):
    root: Path
    build: Path
    src: Path
    tests: Path
    stage: Path
    executable: Path
    header: Path


def init_context() -> Context:
    """Compute anchor paths relative to project root."""
    root_dir = Path(__file__).resolve().parents[2]

    return Context(
        root=root_dir,
        build=root_dir / "build",
        src=root_dir / "src",
        tests=root_dir / "tests",
        stage=root_dir / "tests" / "stage",
        executable=root_dir / "build" / "kramac-san",
        header=root_dir / "src" / "exit_codes.h",
    )


def validate_environment(ctx: Context) -> None:
    """Verifies critical system paths exist. Exits immediately on failure."""
    reqs = {
        "build directory": ctx.build,
        "src directory": ctx.src,
        "tests directory": ctx.tests,
        "stage directory": ctx.stage,
        "executable": ctx.executable,
        "header file": ctx.header,
    }
    missing = [f"{k}: {v}" for k, v in reqs.items() if not v.exists()]
    if missing:
        sys.exit(f"{PROG}: EnvironmentError: Critical paths missing\n{'\n'.join(missing)}")


def resolve_target_stages(stage: str | None, valid_stages: list[str]) -> list[str]:
    """Validate stage argument passed to script. Exits immediately if invalid."""
    if stage is None or stage == "all":
        return valid_stages
    if stage not in valid_stages:
        sys.exit(
            f"{PROG}: InputError: Invalid stage {stage}\nValid stages are {valid_stages} or 'all'"
        )
    return [stage]


def parse_c_int(n: str) -> int | None:
    """Parse an integer as string and returns the int value if possible.

    The string should be an integer literal from a C source file. It does
    not handle bit shift i.e., `2 << 1`

    Returns None on error."""
    base = 10
    if n.upper().startswith("0X"):
        base = 16
        n = n.upper().removeprefix("0X")
    elif n.upper().startswith("0B"):
        base = 2
        n = n.upper().removeprefix("0B")
    elif n.startswith("0"):
        base = 8
        n = n.upper().removeprefix("0")

    # Remove digit separator e.g., 1'000'000 -> 1000000
    n = n.replace("'", "")

    val = 0
    for c in n:
        val *= base
        try:
            val += int(c.upper(), base)
        except ValueError:
            return None
    return val


def extract_exit_codes(header_file: Path, error_accumulator: list[str]) -> dict[str, int]:
    """Extract the KRM exit codes from exit_codes.h"""
    with header_file.open() as f:
        lines = f.readlines()

    extracted_lines = [
        line.strip().removesuffix(",")
        for line in lines
        if not line.strip().startswith("//") and "KRM_EXIT_" in line
    ]
    exit_codes: dict[str, int] = {}

    DEFAULT = -1
    for exit_code in extracted_lines:
        if "=" in exit_code:
            k, v = exit_code.split("=")
            val = parse_c_int(v.strip())
            if val is None:
                error_accumulator.append(
                    f"Unable to parse exit code value `{v.strip()}` from exit code `{exit_code}`"
                )
            else:
                exit_codes[k.strip()] = val
        else:
            exit_codes[exit_code.strip()] = DEFAULT

    latest = 0
    for exit_code, value in exit_codes.items():
        if value == DEFAULT:
            exit_codes[exit_code] = latest
            latest += 1
        else:
            latest = value + 1

    expected = {
        "KRM_EXIT_SUCCESS": 0,
        "KRM_EXIT_ERR_USAGE": 2,
    }
    for code, want in expected.items():
        got = exit_codes.get(code)
        if got is None:
            error_accumulator.append(f"Missing expected code {code}")
        elif got != want:
            error_accumulator.append(f"{code}: Expected {want}, got {got}")

    # This is the contract; 1 is invalid for the enum as its reserved to detect
    # and report sanitizer errors
    invalid_codes = [code for code in exit_codes if exit_codes[code] == 1]
    if invalid_codes:
        error_accumulator.append(f"Invalid codes (value 1) found: {invalid_codes}")

    dupes = [d for d, n in Counter(exit_codes.values()).items() if n > 1]
    if dupes:
        codes = [(c, v) for c, v in exit_codes.items() if v in dupes]
        error_accumulator.append(f"Duplicate entries found in {codes}\n")

    if error_accumulator:
        return {}
    return exit_codes


def validate_case(case: dict, idx: int) -> list[str]:
    """Scans a raw case dictionary. Returns a list of errors found."""
    required = ["name", "args", "return_code", "stderr", "expected"]

    errors: list[str] = []
    missing = [k for k in required if case.get(k) is None]
    if missing:
        errors.append(f"Missing required fields {missing}\nHas: {case.keys()}")

    invalid = [k for k in case if k not in required]
    if invalid:
        errors.append(f"Has invalid fields {invalid}\nRequires: {required}\nHas: {case.keys()}")

    if errors:
        errors.insert(0, f"Case #{idx}")
        return errors

    if not isinstance(case["name"], str):
        errors.append(f"'name': expected str, got {type(case['name']).__name__}")
    if not isinstance(case["args"], list):
        errors.append(f"'args': expected list, got {type(case['args']).__name__}")
    else:
        elem_types = {type(x) for x in case["args"]}
        if elem_types - {str}:
            errors.append(f"Elements of 'args': expected str got {elem_types}")
    if not isinstance(case["return_code"], str):
        errors.append(f"'return_code': expected str got {type(case['return_code']).__name__}")
    if not isinstance(case["stderr"], bool):
        errors.append(f"'stderr': expected bool got {type(case['stderr']).__name__}")
    if not isinstance(case["expected"], str):
        errors.append(f"'expected': expected str got {type(case['expected']).__name__}")

    if errors:
        errors.insert(0, f"Case #{idx}")
        return errors

    commands = {"#derive", "#none"}
    if case["expected"].startswith("#"):
        if case["expected"] not in commands:
            errors.append(f"'expected': Invalid command {case['expected']}\nValid: {commands}")
        elif case["expected"] == "#derive":
            krm_files = [arg for arg in case["args"] if arg.endswith(".krm")]
            if len(krm_files) != 1:
                errors.append(
                    f"'expected' is #derive: Expected: 1 .krm file in 'args'\nHas: {krm_files}"
                )
    elif not case["expected"].endswith(".expected"):
        errors.append(f"'expected': Expected .expected file or a command\nGot: {case['expected']}")

    if errors:
        errors.insert(0, f"Case #{idx}")
    return errors


def load_stage_manifest(stage: str, toml_path: Path, error_accumulator: list[str]) -> list[dict]:
    """Parses cases.toml file and collects configuration anomalies."""
    if not toml_path.exists():
        error_accumulator.append(
            f"ManifestError: Stage {stage}: could not find manifest\nSearching: {toml_path}"
        )
        return []

    try:
        with open(toml_path, "rb") as f:
            toml_data = tomllib.load(f)
    except tomllib.TOMLDecodeError as e:
        error_accumulator.append(f"ManifestError: Stage {stage}: TOMLDecodeError: {e}")
        return []
    except OSError as e:
        sys.exit(f"{PROG}: OSError: {e}\nexiting...")

    if toml_data == {}:
        error_accumulator.append(
            f"ManifestError: Stage {stage}: Manifest is empty\nChecked: {toml_path}"
        )
        return []

    toml_keys = {k for k in toml_data}
    extras = toml_keys - {"case"}
    if extras:
        error_accumulator.append(
            f"ManifestError: Stage {stage}: Root element should only be 'case'"
        )
        return []

    if not isinstance(toml_data["case"], list):
        error_accumulator.append(
            f"ManifestError: Stage {stage}: Root element 'case' must be a list"
        )
        return []

    if not toml_data["case"]:
        error_accumulator.append(f"ManifestError: Stage {stage} 'case' is empty")
        return []

    for elem in toml_data["case"]:
        if not isinstance(elem, dict):
            error_accumulator.append(
                f"ManifestError: Stage {stage}: Elements of 'case' must be tables"
            )
            return []

    case_errors: list[str] = []
    for idx, case in enumerate(toml_data["case"], start=1):
        case_errors.extend(validate_case(case, idx))

    if case_errors:
        error_accumulator.append(f"ManifestError: Stage {stage}\n{'\n'.join(case_errors)}")
        return []

    names = [c.get("name") for c in toml_data["case"]]
    dupes = [v for v, n in Counter(names).items() if n > 1]
    if dupes:
        error_accumulator.append(f"ManifestError: Stage {stage}: Duplicate names found")
        for i, n in [(i, n) for i, n in enumerate(names, start=1) if n in dupes]:
            error_accumulator.append(f"Case #{i}: {n}")
        return []

    return toml_data["case"]


def main():
    ctx = init_context()

    parser = argparse.ArgumentParser(description="Test harness for kramac")
    _ = parser.add_argument(
        "-s",
        "--stage",
        help="Execute tests for a specific stage ('all' to run all stages)",
        default="all",
    )
    args = parser.parse_args()

    validate_environment(ctx)

    valid_stages = [s.name for s in ctx.stage.iterdir() if s.is_dir()]
    if not valid_stages:
        sys.exit(
            f"{PROG}: EnvironmentError: No stages available to test\nSearching directory: {ctx.stage}"
        )
    if "all" in valid_stages:
        sys.exit(f"{PROG}: EnvironmentError: Found stage named 'all'; reserved to run all stages")

    target_stages = sorted(resolve_target_stages(args.stage, valid_stages))

    contract_errors: list[str] = []
    exit_codes = extract_exit_codes(ctx.header, contract_errors)
    if contract_errors:
        sys.exit(
            f"{PROG}: ContractError: Error extracting error codes\n{'\n~'.join(contract_errors)}"
        )
    _ = exit_codes  # TODO(#21) - Needs to check against the output of kramac --dump-exit-codes

    manifest_errors: list[str] = []
    runnable = {}
    for stage in target_stages:
        toml_path = ctx.stage / stage / "cases.toml"
        cases = load_stage_manifest(stage, toml_path, manifest_errors)
        if cases:
            runnable[stage] = cases

    if manifest_errors:
        print(f"{PROG}: Manifest errors detected\n", file=sys.stderr)
        print(f"{'\n~'.join(manifest_errors)}", file=sys.stderr)
        sys.exit(1)

    # [TODO] Run cases...


if __name__ == "__main__":
    main()
