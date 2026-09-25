"""Fixture-scoped capability evidence. A report never arms gameplay by itself."""

from dataclasses import asdict, dataclass
import json
from pathlib import Path


@dataclass(frozen=True)
class CapabilityReport:
    schema: int
    serial: str
    capture_size: tuple[int, int]
    touch_size: tuple[int, int]
    rotation_deg: int
    backend: str
    fixture_apk_sha256: str | None
    verified_actions: tuple[str, ...]
    sample_count: dict[str, int]
    failures: dict[str, int]
    evidence_dir: str
    gameplay_enabled: bool = False

    def matches(self, config) -> bool:
        return (self.schema == 1 and self.serial == config.serial and
                self.capture_size == (config.width, config.height) and
                self.touch_size == (config.touch_width, config.touch_height) and
                self.rotation_deg == config.touch_rotation_deg and
                self.backend == config.touch_kind and not self.gameplay_enabled)

    def save(self, path: str | Path):
        Path(path).write_text(json.dumps(asdict(self),indent=2),encoding="utf-8")

    @classmethod
    def from_touch_summary(cls, summary):
        environment=summary["environment"]
        kinds=summary["by_kind"]
        return cls(1,environment["serial"],tuple(environment["display"]),
                   tuple(environment["touch_display"]),environment["rotation_deg"],
                   "emulator-grpc",environment.get("fixture_apk_sha256"),
                   tuple(k for k,v in summary["fixture_verified"].items() if v),
                   {k:v["n"] for k,v in kinds.items()},
                   {k:v["rpc_failures"]+v["visible_failures"] for k,v in kinds.items()},
                   summary["evidence_dir"])
