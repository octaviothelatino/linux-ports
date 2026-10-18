# Deferred frame-sizing candidate

The owner reported no visible improvement in HTML panel opening latency from
this candidate on 2026-10-15. It was removed from the accepted installation.
It is preserved only to avoid losing work during the laptop handoff.

`candidate.patch` is the original source diff. `bounds-test.c` is the temporary
white-box check that includes Wine's actual composition target implementation.
It is not an official Wine test and retains its original source include path.
Adjust that path if reviewing it on another machine. The patch was not rebased
onto the latest merge and has not been reapplied.

Do not treat the synthetic checks as proof that SOA opening latency improved.
Do not apply this patch automatically during restoration.
