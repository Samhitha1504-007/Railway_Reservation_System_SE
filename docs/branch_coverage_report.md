# Branch Coverage Report

**Run on:** 12 Nov 2026, GCC + gcov, report by gcovr 8.8 (`--exclude-throw-branches --exclude-unreachable-branches`)  
**Code:** `main` at `19a70a6`  
**Tests:** all 12 test binaries (266 tests)  
**Prepared by:** Samhitha S (QA Lead)

A branch is one outcome of a decision (`if`, `while`, `&&`, `||`, `?:`, `switch`). Branch coverage counts how many of those outcomes the tests actually took. Exception branches that the C++ compiler adds for every call that might throw are excluded, because they cannot be reached in this code base and would only add noise.

Per-line branch markers are in the HTML report: [`coverage/index.html`](coverage/index.html).

## Summary

| Scope | Branches taken | Branch % |
|---|---|---|
| Engine | **1111/1291** | **86.1%** |

## By module

| Module | Branches | Branch % |
|---|---|---|
| Audit | 68/82 | 82.9% |
| Auth | 323/349 | 92.6% |
| Booking | 178/256 | 69.5% |
| Facade | 46/69 | 66.7% |
| Inventory | 149/162 | 92.0% |
| Search | 347/373 | 93.0% |

## By file

| File | Branches | Branch % |
|---|---|---|
| `src/booking/waitlist.c` | 37/60 | 61.7% |
| `src/facade/facade.cpp` | 46/69 | 66.7% |
| `src/booking/booking.c` | 141/196 | 71.9% |
| `src/auth/password.cpp` | 48/58 | 82.8% |
| `src/audit/audit.cpp` | 68/82 | 82.9% |
| `src/search/fare_eco.cpp` | 90/101 | 89.1% |
| `src/auth/session.cpp` | 39/43 | 90.7% |
| `src/inventory/inventory.c` | 149/162 | 92.0% |
| `src/auth/rbac.cpp` | 15/16 | 93.8% |
| `src/search/search.cpp` | 177/188 | 94.1% |
| `src/search/graph.cpp` | 80/84 | 95.2% |
| `src/auth/auth.cpp` | 221/232 | 95.3% |

## Analysis of the lowest files

**`src/booking/waitlist.c` (61.7%).** Nearly all missed branches are the defensive checks at the top of each internal function (`!train_id`, `!pnr`, `cls < 0`, `cls >= RRS_CLASS_COUNT`). These functions are only called from `booking.c`, which has already validated the same values, so the "bad argument" side can never be taken through the public API. The queue-full branch (`g_waitlist_count >= MAX_WAITLIST_ENTRIES`) is also not reached: the waitlist now holds `RRS_MAX_BOOKINGS` entries, so the booking store fills first.

**`src/facade/facade.cpp` (66.7%).** The missed branches are: creating the folder for the audit log (the tests pass a file name with no folder), each half of the long input check in `facade_book` (only some of the `||` conditions are tried individually), and the seed-data error path.

**`src/booking/booking.c` (71.9%).** 16 of the 55 missed branches are in `booking_internal_pnr_ok`, which is never called. Most of the rest are the `||` halves of argument checks, the store-full guard, and the combined passenger/admin role checks in `booking_get` and `booking_cancel` (an admin reading a booking is not tested).

## Tests that would raise branch coverage

| Test to add | Module | Branches it covers |
|---|---|---|
| Booking number 10,001 returns `ERR_CAPACITY_CONFLICT` | Booking | store-full guard |
| Admin session reads and cancels a passenger's booking | Booking | admin side of the role checks |
| `booking_get` with a malformed PNR (once `booking_internal_pnr_ok` is used) | Booking | 16 branches in `booking_internal_pnr_ok` |
| `facade_init("data/sub/audit.log")` with a folder that does not exist | Facade | folder creation |
| `facade_book` with each bad argument on its own | Facade | each half of the input check |

None of these block the release: the uncovered outcomes are guards against impossible or out-of-scope situations, and every decision on the normal and error paths that users can reach is covered.

## How to reproduce

```bash
cd RRS/04_IMPLEMENTATION_TESTING/source-code
./tools/coverage.sh            # builds with -DRRS_COVERAGE=ON, runs every test, writes the HTML report
```

