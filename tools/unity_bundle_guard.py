"""Bounded, read-only preflight for this project's Unity 2022.3.62f2 bundles.

Layout references (independent implementation; not a complete Unity reader):
Inspected the task-local UnityPy 1.25.4 wheel's UnityPy/files/BundleFile.py:
read_fs and UnityPy/enums/BundleFile.py. Upstream source locations:
https://github.com/K0lb3/UnityPy/blob/master/UnityPy/files/BundleFile.py
https://github.com/K0lb3/UnityPy/blob/master/UnityPy/enums/BundleFile.py

Only UnityFS format 6/7/8, the exact observed engine version, and none/LZ4
compression are accepted. Metadata decompression is size-limited before the
LZ4 call. Payload blocks are NOT decompressed or authenticated by this check.
Successful preflight is not proof that downstream Unity decoding is safe or
that catalog/song identity is correct. Run downstream parsing in a bounded
worker as well. No encryption handling, patching, network, or device access.
"""
import struct


class BundleGuardError(ValueError):
    pass


MAX_INFO_BYTES = 4 * 1024 * 1024
MAX_BLOCKS = 65536
MAX_NODES = 65536
DEFAULT_MAX_UNCOMPRESSED_BYTES = 256 * 1024 * 1024


class _Reader:
    def __init__(self, data):
        self.data, self.pos = memoryview(data), 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise BundleGuardError("Truncated UnityFS data")
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        return value

    def integer(self, code):
        return struct.unpack(">" + code, self.take(struct.calcsize(">" + code)))[0]

    def cstring(self, max_bytes=128):
        result = bytearray()
        for _ in range(max_bytes):
            byte = self.integer("B")
            if not byte:
                try:
                    return result.decode("utf-8")
                except UnicodeError as exc:
                    raise BundleGuardError("Invalid UTF-8 UnityFS string") from exc
            result.append(byte)
        raise BundleGuardError("UnityFS string exceeds length budget")


def _aligned(value):
    return (value + 15) & ~15


def _decompress_info(compressed, expected, compression):
    # expected was validated against MAX_INFO_BYTES before this function.
    if compression == 0:
        if len(compressed) != expected:
            raise BundleGuardError("Uncompressed metadata size mismatch")
        return bytes(compressed)
    try:
        import lz4.block
    except ImportError as exc:
        raise BundleGuardError("LZ4 metadata requires the task-local lz4 dependency") from exc
    try:
        result = lz4.block.decompress(bytes(compressed), uncompressed_size=expected)
    except Exception as exc:
        raise BundleGuardError("Invalid or oversized LZ4 metadata") from exc
    if len(result) != expected:
        raise BundleGuardError("Decompressed metadata size mismatch")
    return result


def preflight_unityfs(payload, max_uncompressed_bytes=DEFAULT_MAX_UNCOMPRESSED_BYTES):
    """Validate declared allocation sizes and return JSON-compatible metadata.

    Does not decompress payload blocks. ``max_uncompressed_bytes`` bounds each
    block, the sum of blocks, and each node's byte range. Unknown flags and
    unsupported engines/formats/compression modes fail explicitly.
    """
    if (isinstance(max_uncompressed_bytes, bool) or not isinstance(max_uncompressed_bytes, int)
            or max_uncompressed_bytes <= 0):
        raise BundleGuardError("Invalid uncompressed byte budget")
    if not isinstance(payload, (bytes, bytearray, memoryview)):
        raise TypeError("UnityFS input must be bytes-like")
    reader = _Reader(payload)
    if reader.cstring(16) != "UnityFS":
        raise BundleGuardError("Only UnityFS bundles are supported")
    version = reader.integer("I")
    if version not in (6, 7, 8):
        raise BundleGuardError("Unsupported UnityFS format version")
    player_version, engine_version = reader.cstring(), reader.cstring()
    if engine_version != "2022.3.62f2":
        raise BundleGuardError("Unsupported Unity engine version")
    declared_size = reader.integer("q")
    compressed_info_size, uncompressed_info_size, flags = (reader.integer("I") for _ in range(3))
    if declared_size != len(payload):
        raise BundleGuardError("UnityFS declared file size differs from input size")
    # Modern ArchiveFlags: compression 0x3f, combined 0x40, end 0x80,
    # padding 0x200. Encryption uses 0x400/0x1000 and is always refused.
    if flags & ~0x2FF or not (flags & 0x40):
        raise BundleGuardError("Unsupported or encrypted UnityFS archive flags")
    compression = flags & 0x3F
    if compression not in (0, 2, 3):
        raise BundleGuardError("Unsupported UnityFS metadata compression")
    if (not 0 < compressed_info_size <= MAX_INFO_BYTES or
            not 0 < uncompressed_info_size <= MAX_INFO_BYTES):
        raise BundleGuardError("UnityFS metadata exceeds byte budget")
    header_end = _aligned(reader.pos) if version >= 7 else reader.pos
    if header_end > len(payload):
        raise BundleGuardError("Truncated aligned UnityFS header")
    at_end = bool(flags & 0x80)
    info_offset = len(payload) - compressed_info_size if at_end else header_end
    if info_offset < header_end or info_offset + compressed_info_size > len(payload):
        raise BundleGuardError("UnityFS metadata overlaps header or exceeds input")
    data_offset = header_end if at_end else info_offset + compressed_info_size
    if flags & 0x200:
        data_offset = _aligned(data_offset)
    data_end = info_offset if at_end else len(payload)
    if data_offset > data_end:
        raise BundleGuardError("UnityFS metadata overlaps payload")
    info = _Reader(_decompress_info(memoryview(payload)[info_offset:info_offset + compressed_info_size],
                                    uncompressed_info_size, compression))
    info_hash = bytes(info.take(16)).hex()  # Preserved, not treated as authenticated.
    block_count = info.integer("i")
    if not 1 <= block_count <= MAX_BLOCKS:
        raise BundleGuardError("UnityFS block count exceeds budget")
    blocks, total_uncompressed, total_compressed = [], 0, 0
    for index in range(block_count):
        uncompressed, compressed, block_flags = info.integer("I"), info.integer("I"), info.integer("H")
        if block_flags & ~0x7F or (block_flags & 0x3F) not in (0, 2, 3):
            raise BundleGuardError("Unsupported or encrypted UnityFS block flags")
        if (uncompressed > max_uncompressed_bytes or compressed > data_end - data_offset or
                (uncompressed and not compressed)):
            raise BundleGuardError("UnityFS block exceeds declared size budget")
        if (block_flags & 0x3F) == 0 and compressed != uncompressed:
            raise BundleGuardError("Uncompressed payload block size mismatch")
        total_uncompressed += uncompressed
        total_compressed += compressed
        if total_uncompressed > max_uncompressed_bytes:
            raise BundleGuardError("UnityFS total uncompressed size exceeds byte budget")
        blocks.append(dict(index=index, compressed_bytes=compressed, uncompressed_bytes=uncompressed, flags=block_flags))
    if total_compressed != data_end - data_offset:
        raise BundleGuardError("UnityFS compressed blocks do not match payload region")
    node_count = info.integer("i")
    if not 0 <= node_count <= MAX_NODES:
        raise BundleGuardError("UnityFS node count exceeds budget")
    nodes, names = [], set()
    for index in range(node_count):
        offset, size, node_flags = info.integer("q"), info.integer("q"), info.integer("I")
        name = info.cstring(4096)
        if offset < 0 or size < 0 or offset + size > total_uncompressed:
            raise BundleGuardError("UnityFS node range exceeds uncompressed data")
        if not name or name in names:
            raise BundleGuardError("Empty or duplicate UnityFS node path")
        names.add(name)
        nodes.append(dict(index=index, path=name, offset=offset, bytes=size, flags=node_flags))
    if info.pos != len(info.data):
        raise BundleGuardError("Trailing UnityFS metadata is unsupported")
    return dict(signature="UnityFS", format_version=version, player_version=player_version,
                engine_version=engine_version, bytes=len(payload), archive_flags=flags,
                metadata_compression=compression, metadata_offset=info_offset,
                metadata_compressed_bytes=compressed_info_size, metadata_uncompressed_bytes=uncompressed_info_size,
                metadata_hash_unverified=info_hash, data_offset=data_offset,
                compressed_payload_bytes=total_compressed, total_uncompressed_bytes=total_uncompressed,
                block_count=block_count, node_count=node_count, blocks=blocks, nodes=nodes,
                payload_blocks_decoded=False)


__all__ = ["BundleGuardError", "preflight_unityfs"]
