# Independent isolated extension contract check

PASS for the tested draft contract. No live headers or build registration changed. The independent source and test hashes are in `manifest.sha256`.

`contract.cpp` constructs its own literal Extension Context packet and checks exact full class matching, opaque unknown handling, every truncated prefix, malformed payload callback barriers, budget rejection before callback and callback budget exhaustion. Validation performs no dispatch/admission; denied admission has no semantic callback, each successful repeated dispatch invokes admission again, and another registry cannot dispatch the capability. These are controlled trusted callbacks, not a sandbox or full transaction implementation.

Independent review identified inconsistent validator inputs: receive validation supplied full wire, while encode validation supplied empty wire. The producer repaired encode to serialize and checked-decode the envelope before invoking the same validator. The independent regression reparses the validator's full wire and compares payload address/offset and SID; the same validator now succeeds on both encode and receive, and encoded bytes match the literal exactly.

Commands executed successfully on local macOS:

```
clang++ -std=c++23 -O2 -UNDEBUG -fno-exceptions -fno-rtti -Idrafts/P14-extensions/include -Iinclude drafts/P14-extensions/verification/contract.cpp -o /tmp/p14-extension-independent
/tmp/p14-extension-independent
clang++ -std=c++23 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-exceptions -fno-rtti -Idrafts/P14-extensions/include -Iinclude drafts/P14-extensions/verification/contract.cpp -o /tmp/p14-extension-independent-asan
/tmp/p14-extension-independent-asan
```

This is isolated draft evidence, not live integration, all-family conformance, a vendor class/peer qualification, allocation instrumentation or proof of callback behavior beyond the declared trusted contract. Producer tests supply additional coverage separately.
