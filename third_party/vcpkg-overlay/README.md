# PAS gRPC Windows read patch

Base port: microsoft/vcpkg `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb`,
`ports/grpc` (cached tree `dd34d98b6fa959331f1be77c79592e6b8569859f`).
All seven upstream vcpkg patches and dependency features are retained.
gRPC stays pinned to 1.81.1; this local overlay is port revision 2.

`00017-pas-windows-read-chunk.patch` adds one per-endpoint integer argument:
`pas.grpc.windows_read_chunk_bytes`. Supported opt-in values are 65536 and
262144. Absent or unsupported values use upstream's 8192-byte default.
PAS validates its public KiB option before creating a channel. Connect snapshots
the integer into ConnectionState before returning, then passes it to the endpoint
after the asynchronous connect completes. Listener endpoints keep the default;
the patch never reads their retained EndpointConfig reference. The initial
revision 1 failed this lifetime regression and is superseded.

This changes the minimum receive slice allocation, not a frame queue or HTTP/2
flow-control window. Spare slices are donated as before. The aggregate usable
length passed to WSARecv stays below two chunks; retained backing allocations
and gRPC's message and transport buffers are separate. This is not a process
memory bound. Partial reads, callbacks and cancellation retain the
upstream implementation. Endpoint layout changes require rebuilding gRPC as a
unit; do not mix the previous research object overrides with this library.

The installed `grpc/pas_windows_read_config.h` exposes a revision symbol and
the bounded size-selection function. PAS links the revision symbol, preventing
a stock library from silently accepting a no-op channel argument. A trace under
`event_engine_endpoint` reports the selected size without logging payload data.

License: upstream gRPC Apache-2.0, and the upstream vcpkg port MIT license.
The project carries those notices under `docs/third_party/`.
