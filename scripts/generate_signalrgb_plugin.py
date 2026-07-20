#!/usr/bin/env python3
"""Generate SignalRGB QMK plugin sections from QMK info.json + VIA JSON.

The JavaScript body remains in a .js.template file. By default, the template is
resolved as ../templates/QMK_Keyboard_SignalRGB_Plugin.js.template relative to
this script, which is expected to live in <repo_root>/scripts/. Any line
consisting only of

    ###generatorName###

is replaced by the corresponding generated section. Leading indentation on the
placeholder line is applied to every emitted line.

Built-in generators:
    name, version, vendorId, productId, publisher, size,
    vKeys, vKeyNames, vKeyPositions

The default assumption is that RGB LED indices follow the selected QMK layout
order, so vKeys is [0, 1, ..., N-1]. Use --led-map when the firmware's
RGB-matrix LED order differs.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterable, Mapping, Sequence


PLACEHOLDER_RE = re.compile(r"^(?P<indent>[ \t]*)###(?P<name>[A-Za-z_][A-Za-z0-9_]*)###[ \t]*$")
MATRIX_RE = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*$")
ENCODER_META_RE = re.compile(r"^e\d+$", re.IGNORECASE)


class GenerationError(RuntimeError):
    """A user-facing generation or validation error."""


@dataclass(frozen=True)
class Key:
    matrix: tuple[int, int]
    x: float
    y: float
    w: float
    h: float
    via_label: str | None


@dataclass(frozen=True)
class GeneratedKey:
    matrix: tuple[int, int]
    led_index: int
    name: str
    position: tuple[int, int]


@dataclass(frozen=True)
class Context:
    name: str
    keyboard_name: str
    version: str
    vendor_id: int
    product_id: int
    publisher: str
    size: tuple[int, int]
    keys: tuple[GeneratedKey, ...]
    layout_name: str


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
    raise GenerationError(f"invalid {field_name}: expected integer or numeric string")


def js_string(value: str) -> str:
    # JSON string syntax is valid JavaScript string syntax and handles escaping.
    return json.dumps(value, ensure_ascii=False)


def find_qmk_root(path: Path) -> Path | None:
    """Find the nearest ancestor that looks like a QMK repository root."""
    resolved = path.expanduser().resolve()
    if resolved.is_file():
        resolved = resolved.parent

    for candidate in (resolved, *resolved.parents):
        keyboards = candidate / "keyboards"
        if not keyboards.is_dir():
            continue
        # A real QMK tree normally has at least one of these. Requiring one
        # avoids treating an arbitrary directory named `keyboards` as QMK.
        if any((candidate / marker).exists() for marker in ("quantum", "builddefs", "platforms", ".git")):
            return candidate
    return None


def normalize_keyboard_target(value: str) -> str:
    raw = value.strip().replace("\\", "/").strip("/")
    if raw.startswith("keyboards/"):
        raw = raw[len("keyboards/") :]
    parts = [part for part in raw.split("/") if part and part != "."]
    for stop in ("keymaps", "via_json"):
        if stop in parts:
            parts = parts[: parts.index(stop)]
    if not parts or any(part == ".." for part in parts):
        raise GenerationError(f"invalid QMK keyboard target: {value!r}")
    return "/".join(parts)


def infer_keyboard_target(path: Path, qmk_root: Path) -> str | None:
    """Infer `vendor/board[/revision]` from a path inside keyboards/."""
    resolved = path.expanduser().resolve()
    try:
        relative = resolved.relative_to((qmk_root / "keyboards").resolve())
    except ValueError:
        return None

    parts = list(relative.parts)
    if not parts:
        return None

    if resolved.is_file():
        parts = parts[:-1]

    for stop in ("keymaps", "via_json"):
        if stop in parts:
            parts = parts[: parts.index(stop)]
            break

    # For a cwd somewhere below a keyboard, choose the closest ancestor that
    # owns info.json. This also handles revisions with their own info.json.
    if resolved.is_dir() and "keymaps" not in relative.parts and "via_json" not in relative.parts:
        cursor = resolved
        keyboard_root = (qmk_root / "keyboards").resolve()
        while cursor != keyboard_root and keyboard_root in cursor.parents:
            if (cursor / "info.json").is_file():
                return cursor.relative_to(keyboard_root).as_posix()
            cursor = cursor.parent

    return "/".join(parts) if parts else None


def discover_via_json(
    keyboard_dir: Path,
    *,
    requested_layout: str | None,
) -> Path:
    via_dir = keyboard_dir / "via_json"
    candidates = sorted(via_dir.glob("*.json")) if via_dir.is_dir() else []
    if not candidates:
        raise GenerationError(
            f"no VIA JSON found under {via_dir}; pass --via explicitly"
        )
    if len(candidates) == 1:
        return candidates[0]

    if requested_layout:
        lowered = requested_layout.lower()
        flavors = [token for token in ("ansi", "iso", "jis") if token in lowered]
        if flavors:
            matches = [
                path for path in candidates
                if all(token in path.stem.lower() for token in flavors)
            ]
            if len(matches) == 1:
                return matches[0]

    rendered = "\n  ".join(str(path) for path in candidates)
    raise GenerationError(
        "multiple VIA JSON files match this keyboard; pass --via explicitly:\n  " + rendered
    )


def resolve_inputs(args: argparse.Namespace) -> tuple[Path, Path, str | None, Path | None]:
    """Resolve info/VIA files and the QMK keyboard target from CLI context."""
    explicit_info = args.info.expanduser().resolve() if args.info else None
    explicit_via = args.via.expanduser().resolve() if args.via else None
    explicit_root = args.qmk_root.expanduser().resolve() if args.qmk_root else None

    root_candidates = [path for path in (explicit_info, explicit_via, Path.cwd()) if path is not None]
    qmk_root = explicit_root
    if qmk_root is None:
        roots = [root for path in root_candidates if (root := find_qmk_root(path)) is not None]
        if roots:
            qmk_root = roots[0]
            if any(root != qmk_root for root in roots[1:]):
                raise GenerationError("input paths appear to belong to different QMK repositories")

    target_candidates: list[tuple[str, str]] = []
    if args.keyboard:
        target_candidates.append(("--keyboard", normalize_keyboard_target(args.keyboard)))
    if qmk_root is not None:
        for label, path in (("--info", explicit_info), ("--via", explicit_via), ("cwd", Path.cwd())):
            if path is None:
                continue
            target = infer_keyboard_target(path, qmk_root)
            if target:
                target_candidates.append((label, target))

    keyboard_target = target_candidates[0][1] if target_candidates else None
    disagreements = [(label, target) for label, target in target_candidates if target != keyboard_target]
    if disagreements:
        details = ", ".join(f"{label}={target}" for label, target in target_candidates)
        raise GenerationError(f"conflicting inferred QMK keyboard targets: {details}")

    if (explicit_info is None or explicit_via is None) and (qmk_root is None or keyboard_target is None):
        raise GenerationError(
            "cannot infer missing input paths; pass --info and --via, or run inside a QMK "
            "repository and provide --keyboard (or a path under keyboards/)"
        )

    keyboard_dir = qmk_root / "keyboards" / keyboard_target if qmk_root and keyboard_target else None
    info = explicit_info or (keyboard_dir / "info.json" if keyboard_dir else None)
    if info is None or not info.is_file():
        raise GenerationError(f"QMK info.json not found: {info}")

    via = explicit_via or discover_via_json(keyboard_dir, requested_layout=args.layout)  # type: ignore[arg-type]
    if not via.is_file():
        raise GenerationError(f"VIA JSON not found: {via}")

    return info, via, keyboard_target, qmk_root


def filename_token(value: str) -> str:
    token = re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_")
    return token or "QMK_Keyboard"


def infer_output_path(context: Context) -> Path:
    flavor = next(
        (token.upper() for token in ("ansi", "iso", "jis") if token in context.layout_name.lower()),
        None,
    )
    pieces = [filename_token(context.keyboard_name), "QMK"]
    if flavor:
        pieces.append(flavor)
    pieces.append("Keyboard")
    return Path.cwd() / ("_".join(pieces) + ".js")


def format_number(value: float) -> str:
    if not math.isfinite(value):
        raise GenerationError(f"non-finite coordinate: {value!r}")
    if value.is_integer():
        return str(int(value))
    return format(value, ".12g")


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


def extract_via_matrix_and_labels(via: Mapping[str, Any]) -> tuple[set[tuple[int, int]], dict[tuple[int, int], str]]:
    try:
        rows = via["layouts"]["keymap"]
    except (KeyError, TypeError) as exc:
        raise GenerationError("VIA JSON is missing layouts.keymap") from exc

    if not isinstance(rows, list):
        raise GenerationError("VIA layouts.keymap must be an array")

    matrices: set[tuple[int, int]] = set()
    labels: dict[tuple[int, int], str] = {}

    for row in rows:
        if not isinstance(row, list):
            raise GenerationError("each VIA keymap row must be an array")
        for item in row:
            if not isinstance(item, str):
                continue

            fields = item.split("\n")
            match = MATRIX_RE.fullmatch(fields[0])
            if match is None:
                continue

            matrix = (int(match.group(1)), int(match.group(2)))
            matrices.add(matrix)

            # VIA uses later legend slots for labels and metadata. Keep the
            # first useful human label, but ignore encoder metadata such as e0.
            for field in fields[1:]:
                candidate = field.strip()
                if not candidate or ENCODER_META_RE.fullmatch(candidate):
                    continue
                if MATRIX_RE.fullmatch(candidate):
                    continue
                labels.setdefault(matrix, candidate)
                break

    if not matrices:
        raise GenerationError("no matrix labels such as '0,0' were found in VIA layouts.keymap")

    return matrices, labels


def info_layout_matrices(layout_obj: Mapping[str, Any]) -> set[tuple[int, int]]:
    entries = layout_obj.get("layout")
    if not isinstance(entries, list):
        return set()

    result: set[tuple[int, int]] = set()
    for entry in entries:
        if not isinstance(entry, Mapping):
            continue
        matrix = entry.get("matrix")
        if isinstance(matrix, list) and len(matrix) == 2:
            result.add((int(matrix[0]), int(matrix[1])))
    return result


def choose_layout(
    info: Mapping[str, Any],
    requested_name: str | None,
    via_matrices: set[tuple[int, int]],
    via_name: str,
) -> tuple[str, Mapping[str, Any]]:
    layouts = info.get("layouts")
    if not isinstance(layouts, Mapping) or not layouts:
        raise GenerationError("QMK info.json contains no layouts")

    if requested_name is not None:
        selected = layouts.get(requested_name)
        if not isinstance(selected, Mapping):
            available = ", ".join(sorted(str(name) for name in layouts))
            raise GenerationError(f"unknown layout {requested_name!r}; available layouts: {available}")
        return requested_name, selected

    via_name_lower = via_name.lower()
    scored: list[tuple[tuple[int, int, int, int, int], str, Mapping[str, Any]]] = []
    for name, layout_obj in layouts.items():
        if not isinstance(layout_obj, Mapping):
            continue
        matrices = info_layout_matrices(layout_obj)
        layout_name_lower = str(name).lower()
        overlap = len(matrices & via_matrices)
        missing_from_info = len(via_matrices - matrices)
        extra_in_info = len(matrices - via_matrices)
        exact_count = int(len(matrices) == len(via_matrices))
        # ANSI/ISO/JIS in a layout name is a useful discriminator when one
        # layout is a matrix superset of another.
        flavor_match = int(
            any(
                token in via_name_lower and token in layout_name_lower
                for token in ("ansi", "iso", "jis")
            )
        )
        score = (flavor_match, exact_count, overlap, -missing_from_info, -extra_in_info)
        scored.append((score, str(name), layout_obj))

    if not scored:
        raise GenerationError("QMK info.json has no usable layout entries")

    scored.sort(key=lambda item: (item[0], item[1]), reverse=True)
    best_score, best_name, best_layout = scored[0]

    tied = [item for item in scored if item[0] == best_score]
    if len(tied) > 1:
        names = ", ".join(sorted(item[1] for item in tied))
        raise GenerationError(
            "could not uniquely infer the QMK layout from the VIA matrix; "
            f"equally good matches: {names}. Pass --layout explicitly."
        )

    return best_name, best_layout


def parse_keys(
    layout_obj: Mapping[str, Any],
    via_labels: Mapping[tuple[int, int], str],
) -> list[Key]:
    entries = layout_obj.get("layout")
    if not isinstance(entries, list):
        raise GenerationError("selected QMK layout has no layout array")

    keys: list[Key] = []
    seen: set[tuple[int, int]] = set()

    for index, entry in enumerate(entries):
        if not isinstance(entry, Mapping):
            raise GenerationError(f"layout entry {index} is not an object")

        matrix = entry.get("matrix")
        if not isinstance(matrix, list) or len(matrix) != 2:
            raise GenerationError(f"layout entry {index} has no valid matrix [row, col]")
        matrix_tuple = (int(matrix[0]), int(matrix[1]))
        if matrix_tuple in seen:
            raise GenerationError(
                f"selected layout contains duplicate matrix position {matrix_tuple}; "
                "select a different layout or fix info.json"
            )
        seen.add(matrix_tuple)

        try:
            x = float(entry.get("x", 0.0))
            y = float(entry.get("y", 0.0))
            w = float(entry.get("w", 1.0))
            h = float(entry.get("h", 1.0))
        except (TypeError, ValueError) as exc:
            raise GenerationError(f"layout entry {index} has invalid geometry") from exc

        if w <= 0 or h <= 0:
            raise GenerationError(f"layout entry {index} has non-positive width or height")

        keys.append(Key(matrix_tuple, x, y, w, h, via_labels.get(matrix_tuple)))

    if not keys:
        raise GenerationError("selected layout contains no keys")

    return keys


def load_matrix_name_map(path: Path | None) -> dict[tuple[int, int], str]:
    if path is None:
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, Mapping):
        raise GenerationError("--names-json must contain an object mapping 'row,col' to a name")

    result: dict[tuple[int, int], str] = {}
    for raw_matrix, raw_name in data.items():
        match = MATRIX_RE.fullmatch(str(raw_matrix))
        if match is None or not isinstance(raw_name, str):
            raise GenerationError(f"invalid name mapping: {raw_matrix!r}: {raw_name!r}")
        result[(int(match.group(1)), int(match.group(2)))] = raw_name
    return result


def load_led_indices(path: Path | None, keys: Sequence[Key], led_offset: int) -> list[int]:
    if path is None:
        return [led_offset + index for index in range(len(keys))]

    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, list):
        indices = [parse_int(value, "LED index") for value in data]
        if len(indices) != len(keys):
            raise GenerationError(
                f"--led-map array has {len(indices)} entries, but the selected layout has {len(keys)} keys"
            )
        return indices

    if isinstance(data, Mapping):
        result: list[int] = []
        for key in keys:
            matrix_name = f"{key.matrix[0]},{key.matrix[1]}"
            if matrix_name not in data:
                raise GenerationError(f"--led-map is missing matrix {matrix_name!r}")
            result.append(parse_int(data[matrix_name], f"LED index for {matrix_name}"))
        return result

    raise GenerationError("--led-map must be either an array or an object keyed by 'row,col'")


def quantize(value: float, mode: str) -> int:
    # A tiny epsilon prevents floating-point representations such as
    # 2.9999999999999996 from falling into the previous cell with floor().
    epsilon = 1e-9
    if mode == "floor":
        return math.floor(value + epsilon)
    if mode == "ceil":
        return math.ceil(value - epsilon)
    if mode == "round":
        # Avoid Python's bankers-rounding; .5 always rounds away from zero.
        return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
    raise GenerationError(f"unsupported quantization mode: {mode}")


def make_positions(
    keys: Sequence[Key],
    *,
    origin: str,
    scale: float,
    quantization: str,
) -> list[tuple[int, int]]:
    if scale <= 0 or not math.isfinite(scale):
        raise GenerationError("--position-scale must be a positive finite number")

    raw: list[tuple[float, float]] = []
    for key in keys:
        if origin == "top-left":
            raw.append((key.x, key.y))
        elif origin == "center":
            raw.append((key.x + key.w / 2.0, key.y + key.h / 2.0))
        else:
            raise GenerationError(f"unsupported position origin: {origin}")

    min_x = min(point[0] for point in raw)
    min_y = min(point[1] for point in raw)

    return [
        (
            quantize((x - min_x) * scale, quantization),
            quantize((y - min_y) * scale, quantization),
        )
        for x, y in raw
    ]


def warn_position_collisions(keys: Sequence[Key], positions: Sequence[tuple[int, int]]) -> None:
    at_position: dict[tuple[int, int], list[tuple[int, int]]] = defaultdict(list)
    for key, position in zip(keys, positions, strict=True):
        at_position[position].append(key.matrix)

    collisions = {position: matrices for position, matrices in at_position.items() if len(matrices) > 1}
    if not collisions:
        return

    rendered = "; ".join(
        f"{position}: {', '.join(f'{row},{col}' for row, col in matrices)}"
        for position, matrices in sorted(collisions.items())
    )
    print(
        "warning: multiple LEDs quantized to the same SignalRGB coordinate: " + rendered,
        file=sys.stderr,
    )


def build_context(args: argparse.Namespace) -> Context:
    info = json.loads(args.info.read_text(encoding="utf-8"))
    via = json.loads(args.via.read_text(encoding="utf-8"))
    if not isinstance(info, Mapping) or not isinstance(via, Mapping):
        raise GenerationError("both input JSON files must contain top-level objects")

    via_matrices, via_labels = extract_via_matrix_and_labels(via)
    layout_name, layout_obj = choose_layout(
        info, args.layout, via_matrices, str(via.get("name", ""))
    )
    keys = parse_keys(layout_obj, via_labels)

    info_matrices = {key.matrix for key in keys}
    missing_from_info = sorted(via_matrices - info_matrices)
    missing_from_via = sorted(info_matrices - via_matrices)
    if missing_from_info:
        print(
            "warning: VIA contains matrices absent from selected QMK layout: "
            + ", ".join(f"{row},{col}" for row, col in missing_from_info),
            file=sys.stderr,
        )
    if missing_from_via:
        print(
            "warning: selected QMK layout contains matrices absent from VIA: "
            + ", ".join(f"{row},{col}" for row, col in missing_from_via),
            file=sys.stderr,
        )

    name_overrides = load_matrix_name_map(args.names_json)
    led_indices = load_led_indices(args.led_map, keys, args.led_offset)
    positions = make_positions(
        keys,
        origin=args.position_origin,
        scale=args.position_scale,
        quantization=args.quantize,
    )
    warn_position_collisions(keys, positions)

    generated_keys = tuple(
        GeneratedKey(
            matrix=key.matrix,
            led_index=led_index,
            name=name_overrides.get(key.matrix)
            or key.via_label
            or f"Key {key.matrix[0]},{key.matrix[1]}",
            position=position,
        )
        for key, led_index, position in zip(keys, led_indices, positions, strict=True)
    )

    if len({key.led_index for key in generated_keys}) != len(generated_keys):
        raise GenerationError("vKeys LED indices are not unique")
    if min(key.led_index for key in generated_keys) < 0:
        raise GenerationError("vKeys LED indices must be non-negative")

    size = (
        max(key.position[0] for key in generated_keys) + 1,
        max(key.position[1] for key in generated_keys) + 1,
    )

    usb = info.get("usb") if isinstance(info.get("usb"), Mapping) else {}
    name = args.name or str(via.get("name") or info.get("keyboard_name") or layout_name)
    vendor_id = parse_int(
        args.vendor_id if args.vendor_id is not None else via.get("vendorId", usb.get("vid")),
        "vendor ID",
    )
    product_value = args.product_id if args.product_id is not None else via.get("productId", usb.get("pid"))
    if product_value is None:
        raise GenerationError("product ID not found; pass --product-id")
    product_id = parse_int(product_value, "product ID")

    keyboard_name = str(info.get("keyboard_name") or name)

    return Context(
        name=name,
        keyboard_name=keyboard_name,
        version=args.version,
        vendor_id=vendor_id,
        product_id=product_id,
        publisher=args.publisher,
        size=size,
        keys=generated_keys,
        layout_name=layout_name,
    )


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


@generator("size")
def generate_size(context: Context) -> str:
    return f"export function Size() {{ return [{context.size[0]}, {context.size[1]}]; }}"


@generator("vKeys")
def generate_vkeys(context: Context) -> str:
    return format_const_array(
        "vKeys",
        [key.led_index for key in context.keys],
        lambda value: str(value),
        per_line=16,
    )


@generator("vKeyNames")
def generate_vkey_names(context: Context) -> str:
    return format_const_array(
        "vKeyNames",
        [key.name for key in context.keys],
        js_string,
        per_line=8,
    )


@generator("vKeyPositions")
def generate_vkey_positions(context: Context) -> str:
    return format_const_array(
        "vKeyPositions",
        [key.position for key in context.keys],
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
                f"unknown template generator {name!r} on line {line_number}; known generators: {known}"
            )
        used.add(name)
        output.append(indent_generated(fn(context), match.group("indent")))

    rendered = "\n".join(output)
    if template.endswith("\n"):
        rendered += "\n"
    return rendered, used


def default_template_path() -> Path:
    """Return <repo_root>/templates/... for a script in <repo_root>/scripts/."""
    return (
        Path(__file__).resolve().parent.parent
        / "templates"
        / "QMK_Keyboard_SignalRGB_Plugin.js.template"
    )


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate a SignalRGB QMK plugin from a line-placeholder JavaScript template."
    )
    parser.add_argument(
        "--info",
        type=Path,
        help="QMK info.json; inferred from --via, --keyboard, or cwd when omitted",
    )
    parser.add_argument(
        "--via",
        type=Path,
        help="VIA keyboard-definition JSON; discovered under via_json/ when unambiguous",
    )
    parser.add_argument(
        "--template",
        type=Path,
        default=default_template_path(),
        help=(
            "JavaScript .js.template file; defaults to "
            "<repo_root>/templates/QMK_Keyboard_SignalRGB_Plugin.js.template "
            "relative to this script"
        ),
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="generated plugin path; defaults to <Keyboard>_QMK_<Layout>_Keyboard.js in cwd",
    )
    parser.add_argument(
        "--keyboard",
        help="QMK keyboard target such as keychron/k11_max",
    )
    parser.add_argument(
        "--qmk-root",
        type=Path,
        help="QMK repository root; normally inferred from cwd or an input path",
    )
    parser.add_argument("--layout", help="QMK layout name; inferred from the VIA matrix when omitted")

    parser.add_argument("--name", help="override the generated plugin/device name")
    parser.add_argument("--version", default="1.0.0", help="plugin version used by ###version###")
    parser.add_argument("--publisher", default="WhirlwindFX", help="publisher used by ###publisher###")
    parser.add_argument("--vendor-id", help="override USB vendor ID, e.g. 0x3434")
    parser.add_argument("--product-id", help="override USB product ID, e.g. 0x0AB3")

    parser.add_argument(
        "--position-origin",
        choices=("top-left", "center"),
        default="top-left",
        help="which point of each QMK key rectangle becomes the LED coordinate (default: top-left)",
    )
    parser.add_argument(
        "--position-scale",
        type=float,
        default=1.0,
        help="multiply normalized QMK x/y coordinates before integer quantization (default: 1)",
    )
    parser.add_argument(
        "--quantize",
        choices=("floor", "round", "ceil"),
        default="floor",
        help="convert QMK coordinates to the integer grid required by device.color() (default: floor)",
    )

    parser.add_argument(
        "--led-map",
        type=Path,
        help="optional JSON array or {'row,col': ledIndex} object overriding sequential vKeys",
    )
    parser.add_argument(
        "--led-offset",
        type=int,
        default=0,
        help="starting LED index when --led-map is omitted (default: 0)",
    )
    parser.add_argument(
        "--names-json",
        type=Path,
        help="optional {'row,col': 'SignalRGB key name'} JSON object",
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
        info, via, keyboard_target, qmk_root = resolve_inputs(args)
        args.info = info
        args.via = via
        context = build_context(args)
        output = args.output.expanduser() if args.output else infer_output_path(context)
        if not output.is_absolute():
            output = Path.cwd() / output
        output = output.resolve()

        template = args.template.read_text(encoding="utf-8")
        rendered, used = render_template(template, context)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8", newline="\n")
    except (OSError, json.JSONDecodeError, GenerationError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    inferred = f", keyboard={keyboard_target}" if keyboard_target else ""
    print(
        f"generated {output} using layout {context.layout_name!r}{inferred}: "
        f"{len(context.keys)} LEDs, Size={list(context.size)}, "
        f"placeholders={','.join(sorted(used)) or '(none)'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
