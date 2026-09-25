"""Strict, versioned settings for the new runtime. No credential is serialized."""

from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path


def _object(value, allowed, name, required=()):
    if not isinstance(value, dict) or set(value) - set(allowed) or set(required) - set(value):
        raise ValueError(f"invalid {name} fields")
    return value


def _number(value, low, high, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError(f"invalid {name}")
    return value


@dataclass(frozen=True)
class RuntimeConfig:
    schema: int
    name: str
    serial: str
    capture_kind: str
    width: int
    height: int
    touch_kind: str
    touch_timeout_ms: float
    max_contacts: int
    max_plans: int
    max_steps: int
    horizon_ms: float
    evidence_max_age_ms: float
    preview_hz: float
    log_dir: str
    endpoint: str | None = None
    token_file: str | None = None
    touch_width: int = 0
    touch_height: int = 0
    touch_rotation_deg: int = 0
    source_rotation: int = 0

    @classmethod
    def load(cls, path: str | Path) -> "RuntimeConfig":
        raw = json.loads(Path(path).read_text(encoding="utf-8"), parse_constant=lambda v: (_ for _ in ()).throw(ValueError(f"invalid JSON number {v}")))
        _object(raw, ("schema", "name", "serial", "capture", "touch", "scheduler", "preview", "log_dir"), "profile",
                ("schema", "name", "serial", "capture", "touch", "scheduler", "preview", "log_dir"))
        capture = _object(raw["capture"], ("kind", "execution", "transport", "image_format", "row_order", "width", "height", "source_rotation", "endpoint", "token_file"), "capture",
                          ("kind", "execution", "transport", "image_format", "row_order", "width", "height", "source_rotation"))
        touch = _object(raw["touch"], ("kind", "timeout_ms", "max_contacts", "width", "height", "rotation_deg"), "touch",
                        ("kind", "timeout_ms", "max_contacts"))
        scheduler = _object(raw["scheduler"], ("max_plans", "max_steps", "horizon_ms", "evidence_max_age_ms"), "scheduler",
                            ("max_plans", "max_steps", "horizon_ms", "evidence_max_age_ms"))
        preview = _object(raw["preview"], ("hz",), "preview", ("hz",))
        if isinstance(raw["schema"], bool) or raw["schema"] != 1 or not isinstance(raw["name"], str) or not raw["name"]:
            raise ValueError("unsupported schema or empty profile name")
        if not isinstance(raw["serial"], str) or not raw["serial"].startswith("emulator-"):
            raise ValueError("explicit emulator serial required")
        if capture["kind"] not in ("emulator-grpc", "fake") or capture["execution"] != "process" or capture["transport"] != "payload" or capture["image_format"] != "rgb888" or capture["row_order"] != "top-down":
            raise ValueError("runtime capture requires process/payload/RGB888/top-down")
        width = _number(capture["width"], 1, 4096, "width")
        height = _number(capture["height"], 1, 4096, "height")
        if int(width) != width or int(height) != height or width * height * 3 > 16 * 1024 * 1024:
            raise ValueError("invalid capture dimensions")
        source_rotation = capture["source_rotation"]
        if isinstance(source_rotation, bool) or not isinstance(source_rotation, int) or source_rotation not in range(4):
            raise ValueError("invalid source rotation")
        endpoint, token_file = capture.get("endpoint"), capture.get("token_file")
        if (bool(endpoint) != bool(token_file) or (capture["kind"] == "fake" and endpoint) or
                (endpoint is not None and not isinstance(endpoint, str)) or
                (token_file is not None and not isinstance(token_file, str))):
            raise ValueError("endpoint and token_file must be paired for gRPC")
        if touch["kind"] not in ("none", "emulator-grpc"):
            raise ValueError("invalid touch backend")
        max_contacts = _number(touch["max_contacts"], 1, 10, "max_contacts")
        max_plans = _number(scheduler["max_plans"], 1, 256, "max_plans")
        max_steps = _number(scheduler["max_steps"], 2, 1024, "max_steps")
        if any(int(v) != v for v in (max_contacts, max_plans, max_steps)):
            raise ValueError("capacities must be integers")
        log_dir = raw["log_dir"]
        if not isinstance(log_dir, str) or not log_dir:
            raise ValueError("log_dir required")
        touch_width = _number(touch.get("width", width), 2, 4096, "touch width")
        touch_height = _number(touch.get("height", height), 2, 4096, "touch height")
        rotation = touch.get("rotation_deg", 0)
        if any(int(v) != v for v in (touch_width, touch_height)) or rotation not in (0, 90, 180, 270) or isinstance(rotation, bool):
            raise ValueError("invalid touch dimensions or rotation")
        return cls(1, raw["name"], raw["serial"], capture["kind"], int(width), int(height),
                   touch["kind"], _number(touch["timeout_ms"], 1, 2000, "touch timeout"),
                   int(max_contacts), int(max_plans), int(max_steps),
                   _number(scheduler["horizon_ms"], 1, 5000, "horizon"),
                   _number(scheduler["evidence_max_age_ms"], 1, 1000, "evidence age"),
                   _number(preview["hz"], 0, 10, "preview Hz"), log_dir, endpoint, token_file,
                   int(touch_width), int(touch_height), rotation, source_rotation)

    def public(self) -> dict:
        result = asdict(self)
        result["token_file"] = "<redacted>" if self.token_file else None
        return result
