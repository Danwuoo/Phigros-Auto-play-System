"""Offline diagnostic decoder for the optional capture test page."""

from .contracts import Frame


def read_fixture_counter(frame: Frame, offset_x: int = 0,
                         offset_y: int = 0, scale: float = 1.0,
                         scale_y: float | None = None) -> int | None:
    scale_y = scale if scale_y is None else scale_y
    if scale <= 0 or scale_y <= 0:
        raise ValueError("fixture scales must be positive")
    def pixel(x: int, y: int) -> tuple[int, int, int] | None:
        x = round(x * scale) + offset_x
        y = round(y * scale_y) + offset_y
        if x < 0 or y < 0 or x >= frame.width or y >= frame.height:
            return None
        position = (y * frame.width + x) * 3
        return tuple(frame.rgb[position:position + 3])

    def red(value: tuple[int, int, int] | None) -> bool:
        return value is not None and value[0] > 180 and value[1] < 90 and value[2] < 90

    counter = 0
    for row in range(2):
        y = 96 + row * 26
        if not red(pixel(16, y)) or not red(pixel(206, y)):
            return None
        for bit in range(12):
            value = pixel(36 + bit * 14, y)
            if value is None:
                return None
            if all(channel > 180 for channel in value):
                counter |= 1 << (row * 12 + bit)
            elif not all(channel < 90 for channel in value):
                return None
    return counter
