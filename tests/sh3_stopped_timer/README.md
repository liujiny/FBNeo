# SH3 timer differential and source-only checks

`python3 -B tests/sh3_stopped_timer/static_reciprocal.py` uses only Python
integer arithmetic. It does not invoke a compiler. The current source-only
candidate passed 1,650,438 quotient/remainder cases and 100,000 callback-boundary
skip checks. This is not a C++ or full timer callback test.

`python3 tests/sh3_stopped_timer/test.py` **invokes a C++ compiler** and must
remain stopped while the user's no-compilation instruction applies. Once
allowed, it compares 320,024 timer steps and callback state traces to the
original scalar implementation and additionally checks 1,200,000 cached
reciprocal quotient/remainder inputs in the candidate C++ implementation.
The new reciprocal version has not run this test. Existing archived replay
results apply to the earlier c711071c6 timer batching implementation only.

See `docs/PS4_TIMER_BATCHING.md` for proof, scope and remaining validation.
