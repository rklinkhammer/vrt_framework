# P07 interrupted implementation drafts

These `.hpp.txt` files are incomplete, unverified work preserved outside the compiled tree. They are not public APIs or accepted architectural decisions and are not expected to compile. In particular, the temporary engine cancellation method references an unimplemented AckRecord member; the retention prototype references an unadded error enum.

Implementation stopped because the selected protocol/architecture does not close cancellation-attempt identity for multiple different subsets using the original Message ID. A successful cancellation acknowledgement can omit selectors, so a delayed previous acknowledgement cannot always be attributed to a subsequent attempt. No repeated-attempt policy has been chosen here.

The compiled engine.hpp and backend.hpp were restored byte-for-byte to the independently verified P06 manifest. No P07 cancellation, retention, or controller implementation is released.
