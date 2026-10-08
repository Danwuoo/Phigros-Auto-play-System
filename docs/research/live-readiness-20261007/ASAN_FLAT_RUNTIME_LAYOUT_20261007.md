# Fresh flat ASan test layout

The journal08 proposal to supply PATH through the direct specification is rejected by the unchanged guard's environment allowlist before native launch. The guard permits only three telemetry settings with value 1. Journal08 remains failed; no guard exception or inherited-PATH workaround is used.

Journal09 instead creates a fresh flat test directory from the frozen journal07 executable and DLL bytes. All copies must match their bound originals, and duplicate filenames must have identical SHA/size. It includes the complete ASan test executables, both sanitizer DLLs, GTest DLLs and strict compatibility executable. Original compiled build directories, source and receipts remain unchanged. The fresh copy layout, recipe, complete original inventories, original build receipts and source manifest are inputs to every new native test. Every test runs as the direct owned native root under the unchanged environment allowlist. No PATH or sanitizer option is changed.

The copy is budgeted before creation under the existing 4 GiB journal-candidate and 32 GiB accumulated ceilings. Passing cost prerequisites preserves all original-session failures/skips and does not qualify or enable live execution.
