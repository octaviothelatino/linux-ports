# Bounded merge verification

The final three runs each completed 116 composition checks with three known color
failures, no X11 `BadWindow`, and no cleanup timeout. Three earlier repetitions
after the same corrections also completed. The final JSON records elapsed times
and exit status. Exit status 3 represents the retained test failures, not success.

The shared-resource log reports zero unexpected failures and one expected
unimplemented NT shared-handle case. It is not a full graphics test suite.

The debugger log predates the cleanup correction. It identifies the duplicate
native-child destruction that motivated the correction, not a remaining fatal
error in the final runs. Absolute source and temporary-prefix paths refer to the
old laptop. These files contain synthetic test diagnostics, not game chat.

None of these records establishes manual SOA acceptance of the merged build.
