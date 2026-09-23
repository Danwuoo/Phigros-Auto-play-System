"""Monotonic touch scheduling with replace, expiry, cancellation and receipts."""

import heapq
from .clock import Clock
from .contracts import HitIntent, TouchCommand, TouchReceipt
from .input import TouchBackend
from .telemetry import Telemetry


class Scheduler:
    def __init__(self, clock: Clock, backend: TouchBackend, telemetry: Telemetry,
                 max_late_ns: int = 5_000_000, tap_duration_ns: int = 1_000_000):
        self.clock, self.backend, self.telemetry = clock, backend, telemetry
        self.max_late_ns, self.tap_duration_ns = max_late_ns, tap_duration_ns
        self._heap: list[tuple[int, int, TouchCommand]] = []
        self._generations: dict[str, int] = {}
        self._command_generation: dict[int, int] = {}
        self._started: set[str] = set()
        self._finished: set[str] = set()
        self._serial = 0

    def submit(self, intent: HitIntent) -> bool:
        now = self.clock.now_ns()
        if intent.key in self._started or intent.key in self._finished:
            self.telemetry.record("intent_rejected", key=intent.key, reason="duplicate")
            return False
        if intent.predicted_hit_ns < now - self.max_late_ns:
            self.telemetry.record("intent_rejected", key=intent.key, reason="expired")
            return False
        generation = self._generations.get(intent.key, 0) + 1
        self._generations[intent.key] = generation
        contact_id = intent.track_id
        for phase, scheduled_ns in (("down", intent.predicted_hit_ns),
                                    ("up", intent.predicted_hit_ns + self.tap_duration_ns)):
            command = TouchCommand(intent.key, contact_id, phase, intent.x, intent.y,
                                   scheduled_ns, intent.frame_sequence)
            self._command_generation[id(command)] = generation
            self._serial += 1
            heapq.heappush(self._heap, (scheduled_ns, self._serial, command))
        self.telemetry.record("intent_accepted", key=intent.key,
                              frame_sequence=intent.frame_sequence,
                              predicted_hit_ns=intent.predicted_hit_ns,
                              prediction_basis=intent.prediction_basis,
                              confidence=intent.confidence,
                              uncertainty_ns=intent.uncertainty_ns,
                              generation=generation)
        return True

    def _head(self) -> TouchCommand | None:
        while self._heap:
            _, _, command = self._heap[0]
            # Old predictions for the same key are invalidated by the newest generation.
            # The heap entry carries the generation in its serial lookup below.
            if self._valid(command):
                return command
            heapq.heappop(self._heap)
            self._command_generation.pop(id(command), None)
        return None

    def _valid(self, command: TouchCommand) -> bool:
        return (command.key not in self._finished and
                self._command_generation.get(id(command)) == self._generations.get(command.key))

    def next_due_ns(self) -> int | None:
        head = self._head()
        return head.scheduled_ns if head else None

    def run_due(self) -> list[TouchReceipt]:
        receipts = []
        while (head := self._head()) is not None and head.scheduled_ns <= self.clock.now_ns():
            heapq.heappop(self._heap)
            self._command_generation.pop(id(head), None)
            if head.phase == "down" and self.clock.now_ns() - head.scheduled_ns > self.max_late_ns:
                self._finished.add(head.key)
                self.telemetry.record("intent_rejected", key=head.key, reason="late_at_dispatch")
                continue
            receipt = self.backend.inject(head)
            if head.phase == "down" and receipt.success:
                self._started.add(head.key)
            receipts.append(receipt)
            self.telemetry.record("touch_receipt", key=head.key, phase=head.phase,
                                  scheduled_ns=head.scheduled_ns,
                                  injection_start_ns=receipt.injection_start_ns,
                                  injection_return_ns=receipt.injection_return_ns,
                                  frame_sequence=head.source_frame_sequence,
                                  success=receipt.success, reason=receipt.reason)
            if head.phase == "up" or not receipt.success:
                self._finished.add(head.key)
                if not receipt.success:
                    self.backend.release_all()
        return receipts

    def run_next(self) -> list[TouchReceipt]:
        deadline = self.next_due_ns()
        if deadline is None:
            return []
        self.clock.sleep_until_ns(deadline)
        return self.run_due()

    def cancel_all(self, reason: str) -> None:
        self._heap.clear()
        self._command_generation.clear()
        self._generations.clear()
        self._started.clear()
        self._finished.clear()
        self.backend.release_all()
        self.telemetry.record("scheduler_cancelled", reason=reason,
                              monotonic_ns=self.clock.now_ns())
