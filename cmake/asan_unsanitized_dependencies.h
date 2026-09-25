#pragma once

// vcpkg's Abseil and gRPC binaries are built without ASan. Abseil's Cord
// changes its inline lifetime and poison behavior when this compiler macro is
// visible, so matching headers to those binaries avoids an ODR mismatch.
// /fsanitize=address still instruments the project translation units.
#ifdef __SANITIZE_ADDRESS__
#undef __SANITIZE_ADDRESS__
#endif
