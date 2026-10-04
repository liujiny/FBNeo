# SH3 timer differential tests

Run `python3 tests/sh3_stopped_timer/test.py`. It builds with ASan/UBSan and
compares 320,024 timer steps and callback-state traces to the original scalar
implementation, plus 1,200,000 cached reciprocal quotient/remainder cases.
Covers trigger/wrap boundaries and callbacks that reset, rearm, stop or change
the divider. See ../../docs/PS4_V_VALIDATION.md for archived V results.
