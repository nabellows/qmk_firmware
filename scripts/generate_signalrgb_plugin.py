#!/usr/bin/env python3
"""Generate a SignalRGB plugin from QMK's resolved keyboard metadata.

The script is intended to live at:

    <generator_repo>/scripts/generate_signalrgb_from_qmk.py

Its default JavaScript template is therefore:

    <generator_repo>/templates/QMK_Keyboard_SignalRGB_Plugin.js.template

QMK itself is the single source of truth. The generator runs either:

    qmk info -kb <keyboard> -f json --api

or, when no keyboard argument is supplied:

    qmk info -f json --api

This lets QMK resolve the keyboard from its own configuration or directory-aware
logic. The generator consumes ``rgb_matrix.layout`` from the resolved output. QMK may obtain that
layout directly from JSON metadata or extract a legacy handwritten
``g_led_config`` from keyboard C source; this script does not need to know which.
The array order is the firmware LED order, while each entry supplies its matrix
association, physical x/y position, and flags. For optional README image
discovery, the script asks ``qmk env QMK_FIRMWARE`` for the checkout root,
appends ``keyboards/<keyboard_folder>``, and walks that directory's parents.

Any template line consisting only of:

    ###generatorName###

is replaced by the corresponding generated JavaScript section. Leading
indentation on the placeholder line is applied to every emitted line.

Built-in generators:
    name, version, vendorId, productId, publisher, imageUrl, size,
    vKeys, vKeyNames, vKeyPositions
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Mapping, Sequence


PLACEHOLDER_RE = re.compile(
    r"^(?P<indent>[ \t]*)###(?P<name>[A-Za-z_][A-Za-z0-9_]*)###[ \t]*$"
)
MATRIX_RE = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*$")
MARKDOWN_IMAGE_RE = re.compile(
    r"!\[[^\]]*\]\(\s*(?:<(?P<angle>[^>]+)>|(?P<plain>[^\s)]+))"
    r"(?:\s+(?:\"[^\"]*\"|'[^']*'|\([^)]*\)))?\s*\)",
    re.MULTILINE,
)


class GenerationError(RuntimeError):
    """A user-facing generation or validation error."""


@dataclass(frozen=True)
class Led:
    index: int
    matrix: tuple[int, int] | None
    name: str
    position: tuple[int, int]
    flags: int


@dataclass(frozen=True)
class Context:
    name: str
    keyboard_target: str
    version: str
    vendor_id: int
    product_id: int
    publisher: str
    image_url: str | None
    size: tuple[int, int]
    center_point: tuple[int, int] | None
    leds: tuple[Led, ...]


Generator = Callable[[Context], str]
GENERATORS: dict[str, Generator] = {}


def generator(name: str) -> Callable[[Generator], Generator]:
    def register(fn: Generator) -> Generator:
        if name in GENERATORS:
            raise RuntimeError(f"duplicate generator registration: {name}")
        GENERATORS[name] = fn
        return fn

    return register


def parse_int(value: Any, field_name: str) -> int:
    if isinstance(value, bool):
        raise GenerationError(f"{field_name} must be an integer, not boolean")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:
            raise GenerationError(f"invalid {field_name}: {value!r}") from exc
    raise GenerationError(
        f"invalid {field_name}: expected an integer or numeric string, got {value!r}"
    )


def parse_number(value: Any, field_name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise GenerationError(f"{field_name} must be a number, got {value!r}")
    result = float(value)
    if not math.isfinite(result):
        raise GenerationError(f"{field_name} must be finite, got {value!r}")
    return result


def parse_matrix(value: Any, field_name: str) -> tuple[int, int] | None:
    if value is None:
        return None
    if not isinstance(value, list) or len(value) != 2:
        raise GenerationError(f"{field_name} must be [row, col], got {value!r}")
    row = parse_int(value[0], f"{field_name} row")
    col = parse_int(value[1], f"{field_name} column")
    if row < 0 or col < 0:
        raise GenerationError(f"{field_name} entries must be non-negative")
    return row, col


def js_string(value: str) -> str:
    # JSON strings are valid JavaScript strings and correctly escape content.
    return json.dumps(value, ensure_ascii=False)


def default_template_path() -> Path:
    """Return <generator_repo>/templates/... for a script under scripts/."""
    return (
        Path(__file__).resolve().parent.parent
        / "templates"
        / "QMK_Keyboard_SignalRGB_Plugin.js.template"
    )


def find_qmk_root(path: Path) -> Path | None:
    """Find the nearest ancestor that looks like a QMK repository root."""
    current = path.expanduser().resolve()
    if current.is_file():
        current = current.parent

    for candidate in (current, *current.parents):
        if not (candidate / "keyboards").is_dir():
            continue
        if any(
            (candidate / marker).exists()
            for marker in ("quantum", "builddefs", "platforms", "lib/python/qmk", ".git")
        ):
            return candidate
    return None


def keyboard_target_from_directory(path: Path, qmk_root: Path) -> str | None:
    """Return the deepest buildable keyboard ancestor containing keyboard.json."""
    keyboards_root = (qmk_root / "keyboards").resolve()
    current = path.expanduser().resolve()
    if current.is_file():
        current = current.parent

    try:
        current.relative_to(keyboards_root)
    except ValueError:
        return None

    while current != keyboards_root:
        if (current / "keyboard.json").is_file():
            return current.relative_to(keyboards_root).as_posix()
        current = current.parent
    return None


def normalize_keyboard_target(value: str, qmk_root: Path | None) -> str:
    """Normalize either a QMK target name or a path to/inside its directory."""
    candidate = Path(value).expanduser()
    if candidate.exists():
        if qmk_root is None:
            qmk_root = find_qmk_root(candidate)
        if qmk_root is None:
            raise GenerationError(f"cannot locate a QMK repository for path {candidate}")
        target = keyboard_target_from_directory(candidate, qmk_root)
        if target is None:
            raise GenerationError(
                f"path is not inside a buildable QMK keyboard target: {candidate}"
            )
        return target

    raw = value.strip().replace("\\", "/").strip("/")
    if raw.startswith("keyboards/"):
        raw = raw[len("keyboards/") :]

    parts = [part for part in raw.split("/") if part and part != "."]
    if "keymaps" in parts:
        parts = parts[: parts.index("keymaps")]
    if not parts or any(part == ".." for part in parts):
        raise GenerationError(f"invalid QMK keyboard target: {value!r}")
    return "/".join(parts)


def resolve_qmk_invocation(args: argparse.Namespace) -> tuple[Path, str | None]:
    """Choose the working directory and optional explicit keyboard target.

    The QMK CLI remains responsible for resolving a keyboard when --keyboard is
    omitted. This allows user.keyboard from QMK's config, QMK's configured home,
    and its directory-aware behavior to work normally.
    """
    explicit_root = args.qmk_root.expanduser().resolve() if args.qmk_root else None
    if explicit_root is not None and not explicit_root.is_dir():
        raise GenerationError(f"QMK repository root is not a directory: {explicit_root}")

    target: str | None = None
    root = explicit_root

    if args.keyboard:
        keyboard_path = Path(args.keyboard).expanduser()
        if keyboard_path.exists():
            if root is None:
                root = find_qmk_root(keyboard_path)
            if root is None:
                raise GenerationError(
                    f"cannot locate a QMK repository for keyboard path {keyboard_path}"
                )
            target = normalize_keyboard_target(args.keyboard, root)
        else:
            target = normalize_keyboard_target(args.keyboard, root)

    # Without --qmk-root, preserve the caller's cwd. QMK can then use either its
    # configured home/default keyboard or its normal directory-aware inference.
    return root or Path.cwd(), target


def extract_json_object(text: str) -> Mapping[str, Any]:
    """Parse JSON, tolerating incidental non-JSON text around one object."""
    stripped = text.strip()
    if not stripped:
        raise GenerationError("qmk info produced no JSON output")

    try:
        result = json.loads(stripped)
        if isinstance(result, Mapping):
            return result
        raise GenerationError("qmk info JSON did not contain a top-level object")
    except json.JSONDecodeError:
        pass

    decoder = json.JSONDecoder()
    candidates: list[Mapping[str, Any]] = []
    for index, character in enumerate(text):
        if character != "{":
            continue
        try:
            value, _end = decoder.raw_decode(text, index)
        except json.JSONDecodeError:
            continue
        if isinstance(value, Mapping):
            candidates.append(value)

    if not candidates:
        preview = stripped[:500]
        raise GenerationError(
            "could not parse JSON from qmk info output; output began with:\n" + preview
        )

    for candidate in reversed(candidates):
        if "rgb_matrix" in candidate or "keyboards" in candidate:
            return candidate
    return candidates[-1]


def unwrap_qmk_info(
    data: Mapping[str, Any],
    keyboard_target: str | None,
) -> tuple[Mapping[str, Any], str | None]:
    """Return one resolved keyboard object and its best-known QMK target."""
    keyboards = data.get("keyboards")
    if isinstance(keyboards, Mapping):
        if keyboard_target is not None:
            exact = keyboards.get(keyboard_target)
            if isinstance(exact, Mapping):
                return exact, keyboard_target
        if len(keyboards) == 1:
            resolved_target, only = next(iter(keyboards.items()))
            if isinstance(only, Mapping):
                return only, str(resolved_target)
        available = ", ".join(str(name) for name in keyboards)
        requested = f" keyboard {keyboard_target!r}" if keyboard_target else " a single keyboard"
        raise GenerationError(
            f"qmk info JSON did not contain{requested}; available: {available}"
        )

    resolved_target = keyboard_target
    folder = data.get("keyboard_folder")
    if isinstance(folder, str) and folder.strip():
        resolved_target = folder.strip().replace("\\", "/").strip("/")

    return data, resolved_target


def load_resolved_qmk_info(
    qmk_cwd: Path,
    keyboard_target: str | None,
    qmk_command: str,
) -> tuple[Mapping[str, Any], str | None]:
    """Ask QMK for its fully resolved target configuration.

    When keyboard_target is None, no -kb option is passed. QMK may then resolve
    the keyboard from user.keyboard, its configured QMK home, or current-directory
    context. --api preserves keyboard_folder in the JSON so output naming can use
    the resolved target.
    """
    command = [qmk_command, "info"]
    if keyboard_target is not None:
        command.extend(["-kb", keyboard_target])
    command.extend(["-f", "json", "--api"])

    env = os.environ.copy()
    env.setdefault("NO_COLOR", "1")
    env.setdefault("CLICOLOR", "0")

    try:
        completed = subprocess.run(
            command,
            cwd=qmk_cwd,
            env=env,
            text=True,
            capture_output=True,
            check=False,
        )
    except FileNotFoundError as exc:
        raise GenerationError(
            f"QMK CLI executable not found: {qmk_command!r}; pass --qmk-command if needed"
        ) from exc

    if completed.returncode != 0:
        details = (completed.stderr or completed.stdout).strip()
        rendered = " ".join(command)
        raise GenerationError(
            f"{rendered} failed with exit status {completed.returncode}"
            + (f":\n{details}" if details else "")
        )

    return unwrap_qmk_info(extract_json_object(completed.stdout), keyboard_target)


def query_qmk_firmware_root(qmk_cwd: Path, qmk_command: str) -> Path | None:
    """Ask QMK for its resolved firmware checkout via QMK_FIRMWARE."""
    command = [qmk_command, "env", "QMK_FIRMWARE"]
    env = os.environ.copy()
    env.setdefault("NO_COLOR", "1")
    env.setdefault("CLICOLOR", "0")

    try:
        completed = subprocess.run(
            command,
            cwd=qmk_cwd,
            env=env,
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError:
        return None

    if completed.returncode != 0:
        return None

    # `qmk env QMK_FIRMWARE` normally prints exactly one path. Taking the last
    # non-empty line also tolerates incidental CLI chatter from older setups.
    lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
    if not lines:
        return None

    candidate = Path(lines[-1].strip('"').strip("'")).expanduser().resolve()
    return candidate if candidate.is_dir() else None


def resolve_qmk_root_for_readme(
    args: argparse.Namespace,
    qmk_cwd: Path,
) -> Path | None:
    """Resolve the checkout used to turn keyboard_folder into a real path."""
    if args.qmk_root is not None:
        return args.qmk_root.expanduser().resolve()
    return query_qmk_firmware_root(qmk_cwd, args.qmk_command)


def find_keyboard_readme(qmk_root: Path, keyboard_target: str) -> Path | None:
    """Find the nearest README from the concrete target up to keyboards/."""
    keyboards_root = (qmk_root / "keyboards").resolve()
    current = (keyboards_root / keyboard_target).resolve()

    try:
        current.relative_to(keyboards_root)
    except ValueError:
        return None

    while True:
        for filename in ("readme.md", "README.md"):
            candidate = current / filename
            if candidate.is_file():
                return candidate
        if current == keyboards_root:
            break
        current = current.parent
    return None


def first_markdown_image_url(markdown: str) -> str | None:
    """Return the URL from the first inline Markdown image."""
    match = MARKDOWN_IMAGE_RE.search(markdown)
    if match is None:
        return None
    value = match.group("angle") or match.group("plain")
    return value.strip() if value and value.strip() else None


def resolve_image_url(
    args: argparse.Namespace,
    qmk_cwd: Path,
    keyboard_target: str,
) -> tuple[str | None, str]:
    """Resolve ImageUrl from CLI override or the nearest keyboard README.

    Failure is intentionally non-fatal. The caller emits an empty ImageUrl()
    function and reports the returned reason as a warning.
    """
    if args.image_url is not None:
        value = args.image_url.strip()
        if value:
            return value, "command line"
        return None, "--image-url was empty"

    qmk_root = resolve_qmk_root_for_readme(args, qmk_cwd)
    if qmk_root is None:
        return None, "could not locate the QMK repository for README image discovery"

    readme = find_keyboard_readme(qmk_root, keyboard_target)
    if readme is None:
        return None, f"no keyboard readme.md found for {keyboard_target!r}"

    try:
        image_url = first_markdown_image_url(readme.read_text(encoding="utf-8"))
    except OSError as exc:
        return None, f"could not read {readme}: {exc}"

    if image_url is None:
        return None, f"no Markdown image found in {readme}"
    return image_url, str(readme)


def template_generator_names(template: str) -> set[str]:
    """Return whole-line generator placeholders used by a template."""
    names: set[str] = set()
    for line in template.splitlines():
        match = PLACEHOLDER_RE.fullmatch(line)
        if match is not None:
            names.add(match.group("name"))
    return names


def collect_unique_layout_labels(info: Mapping[str, Any]) -> dict[tuple[int, int], str]:
    """Collect unambiguous optional key labels from QMK layout metadata."""
    layouts = info.get("layouts")
    if not isinstance(layouts, Mapping):
        return {}

    candidates: dict[tuple[int, int], set[str]] = defaultdict(set)
    for layout_obj in layouts.values():
        if not isinstance(layout_obj, Mapping):
            continue
        entries = layout_obj.get("layout")
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if not isinstance(entry, Mapping):
                continue
            matrix = parse_matrix(entry.get("matrix"), "layout matrix")
            label = entry.get("label")
            if matrix is not None and isinstance(label, str) and label.strip():
                candidates[matrix].add(label.strip())

    return {
        matrix: next(iter(labels))
        for matrix, labels in candidates.items()
        if len(labels) == 1
    }


def load_name_overrides(
    path: Path | None,
) -> tuple[dict[int, str], dict[tuple[int, int], str]]:
    """Read names keyed by LED index ('12') or matrix address ('2,4')."""
    if path is None:
        return {}, {}

    data = json.loads(path.expanduser().read_text(encoding="utf-8"))
    if not isinstance(data, Mapping):
        raise GenerationError(
            "--names-json must contain an object keyed by LED index or 'row,col'"
        )

    by_index: dict[int, str] = {}
    by_matrix: dict[tuple[int, int], str] = {}
    for raw_key, raw_name in data.items():
        if not isinstance(raw_name, str):
            raise GenerationError(f"invalid LED name for {raw_key!r}: {raw_name!r}")
        key = str(raw_key).strip()
        matrix_match = MATRIX_RE.fullmatch(key)
        if matrix_match:
            by_matrix[(int(matrix_match.group(1)), int(matrix_match.group(2)))] = raw_name
            continue
        try:
            index = int(key, 10)
        except ValueError as exc:
            raise GenerationError(
                f"invalid --names-json key {raw_key!r}; expected LED index or 'row,col'"
            ) from exc
        if index < 0:
            raise GenerationError("LED name indices must be non-negative")
        by_index[index] = raw_name

    return by_index, by_matrix


def quantize(value: float, mode: str) -> int:
    epsilon = 1e-9
    if mode == "floor":
        return math.floor(value + epsilon)
    if mode == "ceil":
        return math.ceil(value - epsilon)
    if mode == "round":
        # Avoid Python's bankers rounding; .5 rounds away from zero.
        return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
    raise GenerationError(f"unsupported quantization mode: {mode}")


def parse_center_point(rgb_matrix: Mapping[str, Any]) -> tuple[int, int] | None:
    raw = rgb_matrix.get("center_point")
    if raw is None:
        return None
    if not isinstance(raw, list) or len(raw) != 2:
        raise GenerationError(f"rgb_matrix.center_point must be [x, y], got {raw!r}")
    return (
        quantize(parse_number(raw[0], "rgb_matrix.center_point x"), "round"),
        quantize(parse_number(raw[1], "rgb_matrix.center_point y"), "round"),
    )


def parse_rgb_matrix_layout(
    info: Mapping[str, Any],
) -> tuple[Mapping[str, Any], list[Mapping[str, Any]]]:
    rgb_matrix = info.get("rgb_matrix")
    if not isinstance(rgb_matrix, Mapping):
        raise GenerationError(
            "qmk info contains no rgb_matrix object; the selected target may not use RGB Matrix"
        )

    layout = rgb_matrix.get("layout")
    if not isinstance(layout, list) or not layout:
        raise GenerationError(
            "qmk info contains no non-empty rgb_matrix.layout; verify that QMK can "
            "extract the target's g_led_config"
        )

    normalized: list[Mapping[str, Any]] = []
    for index, entry in enumerate(layout):
        if not isinstance(entry, Mapping):
            raise GenerationError(
                f"rgb_matrix.layout[{index}] must be an object, got {entry!r}"
            )
        normalized.append(entry)
    return rgb_matrix, normalized


def build_context(
    args: argparse.Namespace,
    info: Mapping[str, Any],
    target: str,
    image_url: str | None,
) -> Context:
    if args.position_scale <= 0 or not math.isfinite(args.position_scale):
        raise GenerationError("--position-scale must be a positive finite number")

    rgb_matrix, raw_leds = parse_rgb_matrix_layout(info)

    raw_positions: list[tuple[float, float]] = []
    for index, entry in enumerate(raw_leds):
        if "x" not in entry or "y" not in entry:
            raise GenerationError(
                f"rgb_matrix.layout[{index}] is missing x or y: {entry!r}"
            )
        raw_positions.append(
            (
                parse_number(entry["x"], f"rgb_matrix.layout[{index}].x"),
                parse_number(entry["y"], f"rgb_matrix.layout[{index}].y"),
            )
        )

    min_x = min(position[0] for position in raw_positions) if args.normalize_positions else 0.0
    min_y = min(position[1] for position in raw_positions) if args.normalize_positions else 0.0

    labels = collect_unique_layout_labels(info)
    names_by_index, names_by_matrix = load_name_overrides(args.names_json)

    leds: list[Led] = []
    for index, (entry, raw_position) in enumerate(zip(raw_leds, raw_positions, strict=True)):
        matrix = parse_matrix(
            entry.get("matrix"), f"rgb_matrix.layout[{index}].matrix"
        )
        flags = parse_int(entry.get("flags", 0), f"rgb_matrix.layout[{index}].flags")
        if flags < 0 or flags > 0xFF:
            raise GenerationError(
                f"rgb_matrix.layout[{index}].flags must fit in one byte, got {flags}"
            )

        position = (
            quantize((raw_position[0] - min_x) * args.position_scale, args.quantize),
            quantize((raw_position[1] - min_y) * args.position_scale, args.quantize),
        )
        if position[0] < 0 or position[1] < 0:
            raise GenerationError(
                f"LED {index} produced a negative SignalRGB position {position}"
            )

        name = names_by_index.get(index)
        if name is None and matrix is not None:
            name = names_by_matrix.get(matrix) or labels.get(matrix)
        if name is None:
            name = f"Key {matrix[0]},{matrix[1]}" if matrix is not None else f"LED {index}"

        leds.append(
            Led(
                index=index,
                matrix=matrix,
                name=name,
                position=position,
                flags=flags,
            )
        )

    position_members: dict[tuple[int, int], list[int]] = defaultdict(list)
    for led in leds:
        position_members[led.position].append(led.index)
    collisions = {
        position: indices
        for position, indices in position_members.items()
        if len(indices) > 1
    }
    if collisions:
        rendered = "; ".join(
            f"{position}: {indices}" for position, indices in sorted(collisions.items())
        )
        print(
            "warning: multiple QMK LEDs map to the same SignalRGB coordinate: "
            + rendered,
            file=sys.stderr,
        )

    usb = info.get("usb")
    if not isinstance(usb, Mapping):
        raise GenerationError("resolved QMK metadata has no usb object")

    vendor_value = args.vendor_id if args.vendor_id is not None else usb.get("vid")
    product_value = args.product_id if args.product_id is not None else usb.get("pid")
    if vendor_value is None:
        raise GenerationError("USB vendor ID is absent; pass --vendor-id")
    if product_value is None:
        raise GenerationError("USB product ID is absent; pass --product-id")

    name = args.name or str(info.get("keyboard_name") or target)
    size = (
        max(led.position[0] for led in leds) + 1,
        max(led.position[1] for led in leds) + 1,
    )

    return Context(
        name=name,
        keyboard_target=target,
        version=args.version,
        vendor_id=parse_int(vendor_value, "USB vendor ID"),
        product_id=parse_int(product_value, "USB product ID"),
        publisher=args.publisher,
        image_url=image_url,
        size=size,
        center_point=parse_center_point(rgb_matrix),
        leds=tuple(leds),
    )


def format_const_array(
    variable_name: str,
    values: Sequence[Any],
    formatter: Callable[[Any], str],
    *,
    per_line: int,
) -> str:
    lines = [f"const {variable_name} = ["]
    for start in range(0, len(values), per_line):
        chunk = values[start : start + per_line]
        rendered = ", ".join(formatter(value) for value in chunk)
        comma = "," if start + per_line < len(values) else ""
        lines.append(f"\t{rendered}{comma}")
    lines.append("];\n")
    return "\n".join(lines).rstrip()


@generator("name")
def generate_name(context: Context) -> str:
    return f"export function Name() {{ return {js_string(context.name)}; }}"


@generator("version")
def generate_version(context: Context) -> str:
    return f"export function Version() {{ return {js_string(context.version)}; }}"


@generator("vendorId")
def generate_vendor_id(context: Context) -> str:
    return f"export function VendorId() {{ return 0x{context.vendor_id:04X}; }}"


@generator("productId")
def generate_product_id(context: Context) -> str:
    return f"export function ProductId() {{ return 0x{context.product_id:04X}; }}"


@generator("publisher")
def generate_publisher(context: Context) -> str:
    return f"export function Publisher() {{ return {js_string(context.publisher)}; }}"


@generator("imageUrl")
def generate_image_url(context: Context) -> str:
    if context.image_url is None:
        return "export function ImageUrl() {}"
    return f"export function ImageUrl() {{ return {js_string(context.image_url)}; }}"


@generator("size")
def generate_size(context: Context) -> str:
    return f"export function Size() {{ return [{context.size[0]}, {context.size[1]}]; }}"


@generator("vKeys")
def generate_vkeys(context: Context) -> str:
    return format_const_array(
        "vKeys",
        [led.index for led in context.leds],
        lambda value: str(value),
        per_line=16,
    )


@generator("vKeyNames")
def generate_vkey_names(context: Context) -> str:
    return format_const_array(
        "vKeyNames",
        [led.name for led in context.leds],
        js_string,
        per_line=8,
    )


@generator("vKeyPositions")
def generate_vkey_positions(context: Context) -> str:
    return format_const_array(
        "vKeyPositions",
        [led.position for led in context.leds],
        lambda position: f"[{position[0]}, {position[1]}]",
        per_line=8,
    )


def indent_generated(text: str, indent: str) -> str:
    if not indent:
        return text
    return "\n".join(indent + line if line else line for line in text.splitlines())


def render_template(template: str, context: Context) -> tuple[str, set[str]]:
    used: set[str] = set()
    output: list[str] = []

    for line_number, line in enumerate(template.splitlines(), start=1):
        match = PLACEHOLDER_RE.fullmatch(line)
        if match is None:
            output.append(line)
            continue

        name = match.group("name")
        fn = GENERATORS.get(name)
        if fn is None:
            known = ", ".join(sorted(GENERATORS))
            raise GenerationError(
                f"unknown template generator {name!r} on line {line_number}; "
                f"known generators: {known}"
            )
        used.add(name)
        output.append(indent_generated(fn(context), match.group("indent")))

    rendered = "\n".join(output)
    if template.endswith("\n"):
        rendered += "\n"
    return rendered, used


def infer_output_filename(keyboard_target: str) -> str:
    flattened = keyboard_target.replace("/", "_")
    return f"{flattened}_signalrgb_plugin.js"


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a SignalRGB plugin from rgb_matrix.layout returned by "
            "`qmk info -f json`."
        )
    )
    parser.add_argument(
        "--keyboard",
        "-kb",
        help=(
            "optional QMK build target or path, e.g. "
            "keychron/k11_max/ansi_encoder/rgb; when omitted, qmk info resolves "
            "the keyboard from QMK config or directory context"
        ),
    )
    parser.add_argument(
        "--qmk-root",
        type=Path,
        help=(
            "working QMK repository root; when omitted, preserve cwd and let the "
            "QMK CLI use its configured home/context"
        ),
    )
    parser.add_argument(
        "--qmk-command",
        default="qmk",
        help="QMK CLI executable used for `qmk info` (default: qmk)",
    )
    parser.add_argument(
        "--template",
        type=Path,
        default=default_template_path(),
        help=(
            "JavaScript template; defaults to "
            "<generator_repo>/templates/QMK_Keyboard_SignalRGB_Plugin.js.template"
        ),
    )
    parser.add_argument(
        "--output", "-o",
        type=Path,
        help=(
            "generated plugin path; defaults to the flattened QMK target in cwd, "
            "e.g. keychron_k11_max_ansi_encoder_rgb.js"
        ),
    )

    parser.add_argument("--name", help="override merged QMK keyboard_name")
    parser.add_argument("--version", default="1.0.0", help="plugin version")
    parser.add_argument("--publisher", default="WhirlwindFX", help="plugin publisher")
    parser.add_argument(
        "--image-url",
        help=(
            "override ImageUrl(); otherwise use the first Markdown image in the "
            "nearest keyboard readme.md; failure warns and emits an empty function"
        ),
    )
    parser.add_argument("--vendor-id", help="override merged USB VID, e.g. 0x3434")
    parser.add_argument("--product-id", help="override merged USB PID, e.g. 0x0AB3")

    parser.add_argument(
        "--normalize-positions",
        action="store_true",
        help=(
            "subtract the minimum QMK x/y before emitting positions "
            "(default: preserve raw QMK coordinates)"
        ),
    )
    parser.add_argument(
        "--position-scale",
        type=float,
        default=1.0,
        help="multiply QMK RGB coordinates before quantization (default: 1)",
    )
    parser.add_argument(
        "--quantize",
        choices=("floor", "round", "ceil"),
        default="round",
        help=(
            "integer conversion after scaling (default: round; QMK RGB coordinates "
            "are normally integers)"
        ),
    )
    parser.add_argument(
        "--names-json",
        type=Path,
        help=(
            "optional names keyed by LED index or matrix address, "
            "e.g. {'0':'Esc','1,2':'Q'}"
        ),
    )
    parser.add_argument(
        "--list-generators",
        action="store_true",
        help="print available ###generator### names and exit",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)

    if args.list_generators:
        print("\n".join(sorted(GENERATORS)))
        return 0

    args.template = args.template.expanduser().resolve()
    if not args.template.is_file():
        parser.error(
            "template does not exist or is not a file: "
            f"{args.template} (pass --template to override the default)"
        )

    try:
        template = args.template.read_text(encoding="utf-8")
        requested_generators = template_generator_names(template)

        qmk_cwd, requested_target = resolve_qmk_invocation(args)
        info, resolved_target = load_resolved_qmk_info(
            qmk_cwd, requested_target, args.qmk_command
        )
        if resolved_target is None:
            raise GenerationError(
                "qmk info succeeded but did not report keyboard_folder; pass --keyboard "
                "or use a QMK version supporting `qmk info --api`"
            )
        keyboard_target = resolved_target

        image_url: str | None = None
        image_source = "not requested by template"
        if "imageUrl" in requested_generators or args.image_url is not None:
            image_url, image_source = resolve_image_url(args, qmk_cwd, keyboard_target)
            if image_url is None:
                print(
                    f"warning: no plugin image URL: {image_source}; "
                    "emitting empty ImageUrl()",
                    file=sys.stderr,
                )

        context = build_context(args, info, keyboard_target, image_url)

        output = args.output.expanduser() if args.output else Path.cwd() / infer_output_filename(keyboard_target)
        if not output.is_absolute():
            output = Path.cwd() / output
        output = output.resolve()
        if output.is_dir():
            output = output / infer_output_filename(keyboard_target)

        rendered, used = render_template(template, context)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8", newline="\n")
    except (OSError, json.JSONDecodeError, GenerationError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    center = f", QMK center={list(context.center_point)}" if context.center_point else ""
    print(
        f"generated {output} from `qmk info` target {keyboard_target!r}: "
        f"{len(context.leds)} LEDs, Size={list(context.size)}{center}, "
        f"image={image_source!r}, "
        f"placeholders={','.join(sorted(used)) or '(none)'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
