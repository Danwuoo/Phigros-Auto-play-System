"""Rate-limited host diagnostic window. It never supplies detector input."""

import time


class DiagnosticPreview:
    def __init__(self, hz: float):
        import tkinter as tk
        from PIL import Image, ImageTk
        self.tk, self.Image, self.ImageTk = tk, Image, ImageTk
        self.root = tk.Tk()
        self.root.title("PAS observe diagnostic")
        self.root.protocol("WM_DELETE_WINDOW", self.root.destroy)
        self.image = tk.Label(self.root)
        self.image.pack()
        self.status = tk.Label(self.root, text="Waiting for frame", anchor="w", justify="left")
        self.status.pack(fill="x")
        self.interval_ns = round(1e9 / hz)
        self.next_ns = 0
        self.closed = False
        self._photo = None

    def update(self, frame, *, epoch: int, ui: str, contacts=(), reason="", now_ns=None):
        if self.closed:
            return
        now = now_ns or time.monotonic_ns()
        if now < self.next_ns:
            try:
                self.root.update()
            except self.tk.TclError:
                self.closed = True
            return
        self.next_ns = now + self.interval_ns
        try:
            image = self.Image.frombytes("RGB", (frame.width, frame.height), frame.rgb)
            image.thumbnail((960, 540))
            self._photo = self.ImageTk.PhotoImage(image)
            self.image.configure(image=self._photo)
            residence_ms = (now-frame.capture_complete_ns)/1e6
            self.status.configure(text=(f"frame={frame.sequence} epoch={epoch} UI={ui} "
                                        f"host residence={residence_ms:.1f} ms  "
                                        f"contacts={tuple(contacts)}  {reason}\n"
                                        "source age: unknown | note/line IDs: unavailable | injection: disabled"))
            self.root.update()
        except self.tk.TclError:
            self.closed = True

    def close(self):
        if not self.closed:
            try:
                self.root.destroy()
            except self.tk.TclError:
                pass
        self.closed = True
