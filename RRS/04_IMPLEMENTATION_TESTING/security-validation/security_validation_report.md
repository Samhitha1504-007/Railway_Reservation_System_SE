# Security Validation Report (SEC-001 – SEC-006)

**Issue:** #36 Security validation pass  
**Run on:** 11 Nov 2026, Ubuntu (WSL / GitHub Actions `ubuntu-latest`), CMake Debug build  
**Scope:** SRS §6.3 security requirements, executed as described in Test Plan §5  
**Result:** all six requirements **pass** after two fixes made in this pass (see *Findings fixed*).

## How to re-run

```bash
cmake -S RRS/04_IMPLEMENTATION_TESTING/source-code -B build
cmake --build build --parallel
./build/rrs_security_validation_tests   # SEC-001..006 end to end (this pass)
./build/rrs_security_tests              # SEC-003/004/006 auth-level tests (#71)
ctest --test-dir build                  # everything, as CI runs it
```

## Results

| ID | Requirement (SRS) | Test Plan method | Tests | Result |
|---|---|---|---|---|
| SEC-001 | Passwords never stored in plaintext; salted hash | Inspect stored users: salted hashes only, salts differ per user | `Sec001PasswordsAreStoredAsSaltedHashesOnly`; `rrs_auth_tests` PasswordTest.* | **Pass**: stored value is `pbkdf2$100000$<salt>$<hash>`; two users with the same password get different hashes |
| SEC-002 | Passenger sessions cannot invoke admin-only functions | Passenger calls admin function → `ERR_FORBIDDEN` | `Sec002PassengerCannotCallAdminFunctions`, `Sec002PassengerCannotReadOrCancelAnotherPassengersBooking`; `rrs_auth_tests` RbacTest.* | **Pass**: `train_create/update/retire` return `ERR_FORBIDDEN` and change nothing; a passenger cannot read or cancel another passenger's booking |
| SEC-003 | Validate and sanitize all user-supplied fields (registration, search, booking) | Fuzz with overlong strings, special characters, negative numbers | `Sec003SearchRejectsMalformedInput`, `Sec003BookingRejectsMalformedInput`, `Sec003AdminTrainInputIsValidated`; `rrs_security_tests` InputFuzz* (registration, login, profile) | **Pass**: every malformed station, date, option, train id, class, fare, idempotency key and train definition returns `ERR_INVALID_INPUT` and leaves state unchanged |
| SEC-004 | Expired or invalid tokens rejected on every protected call | Reuse expired/invalid token → `ERR_SESSION_EXPIRED` | `Sec004ExpiredAndInvalidTokensAreRejectedEverywhere` (inventory + booking); `rrs_security_tests` SessionSecurityTest.* (auth) | **Pass**: expired, forged and logged-out tokens are rejected by all 7 inventory/booking calls and all auth calls |
| SEC-005 | Mock payment details not persisted beyond success/failure | Inspect booking records | `Sec005OnlyAPaymentFlagIsKept`; design review of `rrs_booking_t` | **Pass**: the record holds only `payment_success` and `fare_amount`, with no card or payment fields; a declined payment stores no booking at all |
| SEC-006 | Failed logins and unauthorized attempts logged for audit | 3 failed logins + an admin action → audit entries | `Sec006FailedLoginsAndAdminActionsAreAudited`; `rrs_security_tests` AuditTest.* | **Pass** (after fix 2): 3 × `LOGIN_FAILED`, 1 × `ADMIN_ACTION fn=train_create`, 1 × `ACCESS_DENIED fn=train_create`; no password or token appears in the log |

## Findings fixed in this pass

1. **SEC-006: admin actions were not audited.** Inventory's `train_create`, `train_update` and `train_retire` called `auth_authorize`, which does not log. An admin creating a train added 0 audit entries, and a passenger's denied attempt was not logged either. **Fix:** they now call `auth_authorize_for(session, RRS_ROLE_ADMIN, "<function>")`, as recommended in #71. Before the fix, `Sec006FailedLoginsAndAdminActionsAreAudited` fails.
2. **NFR-008 (found during this pass): the waitlist was capped at 4,096 entries.** Booking #4,097 onward on a full class failed with `ERR_CAPACITY_CONFLICT`, although NFR-008 requires 10,000 bookings. **Fix:** `MAX_WAITLIST_ENTRIES` is now `RRS_MAX_BOOKINGS` (10,000). Covered by `rrs_scalability_tests`.

## Open findings (not blocking this pass)

| # | Finding | Requirement | Owner / where |
|---|---|---|---|
| 1 | The fare is supplied by the caller of `booking_create`; the facade/TUI must pass `fare_calculate()`'s value, never user input | SEC-003 | Facade / TUI (#35) |
| 2 | Booking functions call `auth_authorize` outside the booking lock, a data race under concurrent use of one session (found with ThreadSanitizer) | NFR-009 | #70 (changes requested) |
| 3 | A passenger trying to read or cancel someone else's booking gets `ERR_FORBIDDEN` but is not written to the audit log | SEC-006 | Booking (after #70 lands) |
| 4 | `ADMIN_ACTION` is logged when the role check passes, before the action runs, so it records authorized attempts, not completed actions | SEC-006 | Audit (#71 follow-up) |
| 5 | Nothing is stored on disk yet (`persistence/store.c` is empty), so SEC-001 and SEC-005 were verified on the in-memory records; re-check the saved files once persistence lands | SEC-001, SEC-005 | Persistence |
| 6 | The facade/TUI must call `audit_init("data/audit.log")` and `booking_init()` at startup; until then audit entries stay in memory only | SEC-006 | Facade / TUI (#35) |
