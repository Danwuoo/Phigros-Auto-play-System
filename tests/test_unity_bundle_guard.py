"""Synthetic offline UnityFS allocation-bound contracts; no game assets."""
import importlib.util
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("unity_bundle_guard", Path(__file__).parents[1] / "tools/unity_bundle_guard.py")
guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guard)


def fixture(*, version=8, engine="2022.3.62f2", flags=0x240, blocks=None, nodes=None,
            info_compression=0, info_uncompressed_override=None, malformed_info=None):
    blocks = [(8, 8, 0)] if blocks is None else blocks
    nodes = [(0, 8, 0, "CAB-synthetic")] if nodes is None else nodes
    info = (bytes(16) + struct.pack(">i", len(blocks)) +
            b"".join(struct.pack(">IIH", *block) for block in blocks) + struct.pack(">i", len(nodes)) +
            b"".join(struct.pack(">qqI", offset, size, bits) + name.encode() + b"\0" for offset, size, bits, name in nodes))
    if malformed_info is not None:
        info = malformed_info
    uncompressed_size = len(info) if info_uncompressed_override is None else info_uncompressed_override
    if info_compression:
        import lz4.block
        info = lz4.block.compress(info, store_size=False)
    flags = (flags & ~0x3F) | info_compression
    prefix = b"UnityFS\0" + struct.pack(">I", version) + b"5.x.x\0" + engine.encode() + b"\0"
    header = prefix + struct.pack(">qIII", 0, len(info), uncompressed_size, flags)
    if version >= 7:
        header += bytes((-len(header)) % 16)
    data = bytes(sum(block[1] for block in blocks))
    if flags & 0x80:
        payload = header + (bytes((-len(header)) % 16) if flags & 0x200 else b"") + data + info
    else:
        payload = header + info
        if flags & 0x200:
            payload += bytes((-len(payload)) % 16)
        payload += data
    result = bytearray(payload)
    struct.pack_into(">q", result, len(prefix), len(result))
    return bytes(result)


class BundleGuardTests(unittest.TestCase):
    def test_valid_metadata_front_and_end_layouts_and_supported_versions(self):
        for version in (6, 7, 8):
            for flags in (0x40, 0xC0, 0x240, 0x2C0):
                with self.subTest(version=version, flags=flags):
                    result = guard.preflight_unityfs(fixture(version=version, flags=flags))
                    self.assertEqual(result["total_uncompressed_bytes"], 8)
                    self.assertEqual(result["nodes"][0]["path"], "CAB-synthetic")
                    self.assertFalse(result["payload_blocks_decoded"])

    def test_metadata_bomb_rejected_before_decompressor(self):
        payload = fixture(info_uncompressed_override=guard.MAX_INFO_BYTES + 1)
        with patch.object(guard, "_decompress_info") as decoder:
            with self.assertRaises(guard.BundleGuardError):
                guard.preflight_unityfs(payload)
            decoder.assert_not_called()

    def test_individual_and_sum_payload_allocation_budgets(self):
        with self.assertRaises(guard.BundleGuardError):
            guard.preflight_unityfs(fixture(blocks=[(100, 1, 2)], nodes=[]), max_uncompressed_bytes=99)
        with self.assertRaises(guard.BundleGuardError):
            guard.preflight_unityfs(fixture(blocks=[(6, 1, 2), (6, 1, 2)], nodes=[]), max_uncompressed_bytes=10)

    def test_rejects_unknown_compression_encryption_and_engine(self):
        cases = [fixture(flags=0x440), fixture(flags=0x1040), fixture(flags=0x8040),
                 fixture(blocks=[(8, 8, 1)]), fixture(blocks=[(8, 8, 0x402)]),
                 fixture(engine="2022.3.61f1"), fixture(version=9)]
        for payload in cases:
            with self.subTest(header=payload[:70]), self.assertRaises(guard.BundleGuardError):
                guard.preflight_unityfs(payload)
        payload = bytearray(fixture())
        # Header compression mode is the low bits of the final header integer.
        offset = payload.index(b"2022.3.62f2\0") + len(b"2022.3.62f2\0") + 16
        struct.pack_into(">I", payload, offset, 0x241)
        with self.assertRaises(guard.BundleGuardError):
            guard.preflight_unityfs(payload)

    def test_block_and_node_count_limits(self):
        for info in (bytes(16) + struct.pack(">i", guard.MAX_BLOCKS + 1),
                     bytes(16) + struct.pack(">iIIHi", 1, 8, 8, 0, guard.MAX_NODES + 1)):
            with self.assertRaises(guard.BundleGuardError):
                guard.preflight_unityfs(fixture(malformed_info=info))

    def test_node_ranges_and_duplicate_names(self):
        for nodes in [[(-1, 1, 0, "bad")], [(0, 9, 0, "bad")],
                      [(0, 1, 0, "same"), (1, 1, 0, "same")]]:
            with self.subTest(nodes=nodes), self.assertRaises(guard.BundleGuardError):
                guard.preflight_unityfs(fixture(nodes=nodes))

    def test_truncation_declared_length_and_payload_region_mismatch(self):
        payload = fixture()
        for bad in (payload[:20], payload[:-1], payload + b"extra"):
            with self.assertRaises(guard.BundleGuardError):
                guard.preflight_unityfs(bad)
        bad = bytearray(payload + b"extra")
        size_offset = bad.index(b"2022.3.62f2\0") + len(b"2022.3.62f2\0")
        struct.pack_into(">q", bad, size_offset, len(bad))
        with self.assertRaises(guard.BundleGuardError):
            guard.preflight_unityfs(bad)

    def test_lz4_metadata_is_bounded_and_validated(self):
        try:
            import lz4.block
        except ImportError:
            self.skipTest("task-local lz4 required for compressed metadata test")
        for compression in (2, 3):
            payload = fixture(info_compression=compression)
            self.assertEqual(guard.preflight_unityfs(payload)["metadata_compression"], compression)
        with self.assertRaises(guard.BundleGuardError):
            guard.preflight_unityfs(fixture(info_compression=2, info_uncompressed_override=1))


if __name__ == "__main__":
    unittest.main()
