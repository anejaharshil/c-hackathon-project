### Spec Compliance
- ✅ Spec compliant | ❌ Issues found: [registration_normalize does not reject inputs where trimmed non-whitespace content exceeds 10 characters]

### Strengths

- RTO state/district table is comprehensive, includes all required legacy codes (TS, TG, DN, DD, AN)
- Section layout in registration.c follows the mandated order: Header Files → Global Variables → Macros → Function Declarations → Functions
- `registration_is_valid` correctly checks character classes, length, state prefix, and district listing
- `sap_id_is_valid` and `teacher_id_is_valid` implement the required digit-count and range checks
- Makefile properly includes `registration.c` in all build targets (all, test, asan, test-asan)
- Input trimming and uppercasing in `registration_normalize` work correctly for the tested cases
- NULL/empty input handling is correct across all four functions
- No partial valid result written on failure in `registration_normalize` (out[0] = '\0' on failure)

### Issues
#### Critical (Must Fix)
- **file: registration.c, lines 211-222** — `registration_normalize` does not reject inputs where the trimmed non-whitespace content exceeds 10 characters. The loop `for (i = 0; i < 10 && src + i < end; i++)` copies the first 10 characters, and the check `i != 10` only verifies that at least 10 characters exist, not exactly 10. If the input after trimming has 11+ non-whitespace characters (e.g., "UP16FU56745" or "UP16FU5674 extra"), the loop still completes with i=10, the length check passes, and `registration_is_valid` may validate the first 10 characters as if they were a complete registration. This means an 11-character registration like "UP16FU56745" would be incorrectly normalized to "UP16FU5674" and accepted. The fix is to add a check that no non-whitespace characters remain after the 10th position: `if (i != 10 || src + i < end)` should reject such inputs.

### Assessment
**Task quality:** Needs fixes

**Reasoning:** The implementation correctly handles all the explicitly tested scenarios and follows the required code structure. However, the `registration_normalize` function has a critical bug where inputs with more than 10 non-whitespace characters after trimming are not rejected, allowing invalid registrations to be normalized and validated. This violates the contract that registrations are exactly 10 characters. The fix is a one-line change to the length validation in `registration_normalize`.