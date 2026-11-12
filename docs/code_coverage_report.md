# Code Coverage Report

**Run on:** 12 Nov 2026, GCC + gcov, report by gcovr 8.8  
**Code:** `main` at `19a70a6`  
**Tests:** all 12 test binaries (266 tests)  
**Prepared by:** Samhitha S (QA Lead)

Line-by-line results (green and red source lines) are in the HTML report: [`coverage/index.html`](coverage/index.html). Branch coverage is covered in its own report: [Branch Coverage Report](branch_coverage_report.md).

## Summary

| Scope | Lines | Functions | Branches |
|---|---|---|---|
| Engine (all modules below) | **1089/1146 (95.0%)** | **121/122 (99.2%)** | 1111/1291 (86.1%) |

**Exit criterion (Test Plan section 5): at least 70% unit coverage per module. Met: every module is between 87% (Audit) and 100% line coverage.**

## By module

| Module | Lines | Line % | Functions | Meets 70%? |
|---|---|---|---|---|
| Audit | 52/60 | 86.7% | 11/11 | Yes |
| Auth | 263/267 | 98.5% | 35/35 | Yes |
| Booking | 276/304 | 90.8% | 23/24 | Yes |
| Facade | 83/93 | 89.2% | 9/9 | Yes |
| Inventory | 152/158 | 96.2% | 18/18 | Yes |
| Search | 263/264 | 99.6% | 25/25 | Yes |

## By file

| File | Lines | Line % | Functions | Lines not run |
|---|---|---|---|---|
| `src/audit/audit.cpp` | 52/60 | 86.7% | 11/11 | 45-57: names of status codes that are never logged (`audit_status_name` switch) |
| `src/auth/auth.cpp` | 146/149 | 98.0% | 18/18 | 172-173: login when the session store is full (`ERR_IO`); 182: lookup of a missing user id |
| `src/auth/password.cpp` | 43/44 | 97.7% | 5/5 | 26: one line of the hex-encoding helper |
| `src/auth/rbac.cpp` | 16/16 | 100.0% | 3/3 | none |
| `src/auth/session.cpp` | 58/58 | 100.0% | 9/9 | none |
| `src/booking/booking.c` | 225/251 | 89.6% | 17/18 | 73-88: `booking_internal_pnr_ok`, a PNR format checker that nothing calls; the store-full guard; a few defensive checks |
| `src/booking/waitlist.c` | 51/53 | 96.2% | 6/6 | 48, 69: "entry not found" returns |
| `src/facade/facade.cpp` | 83/93 | 89.2% | 9/9 | 16-23: creating the audit log folder (tests pass a path with no folder); seed-data error path |
| `src/inventory/inventory.c` | 152/158 | 96.2% | 18/18 | 215-217, 237-239: roll-back if rebuilding the search graph fails after an edit or retire (cannot happen once the input is validated) |
| `src/search/fare_eco.cpp` | 60/60 | 100.0% | 6/6 | none |
| `src/search/graph.cpp` | 53/53 | 100.0% | 6/6 | none |
| `src/search/search.cpp` | 150/151 | 99.3% | 13/13 | 71: one line of the reverse breadth-first search helper |

## What is not in the numbers

| Code | Lines | Why it is excluded | How it is tested instead |
|---|---|---|---|
| `src/tui/main_tui.c` | 325 | ncurses screens need a terminal; no unit test can drive them | System tests ST-TUI-01 to ST-TUI-09 in the [System Validation Report](system_validation_report.md), and the facade it calls is unit-tested (89%) |
| `src/main.c` | 4 | Program entry point | Started in every TUI system test |
| `src/demo/rrs_demo.cpp` | 134 | Scripted console demo, not product code | Run by hand for the Sprint 1 video |
| `tests/` | | Test code itself | |

Including these files, overall line coverage is 67% (1,089 of 1,609 lines); the gap is entirely the TUI and the demo.

## Uncovered code worth acting on

1. **`booking_internal_pnr_ok` in `booking.c` is never called.** Either `booking_get` and `booking_cancel` should use it to reject a malformed PNR early, or it should be removed.
2. **The booking store-full guard** (`RRS_MAX_BOOKINGS`) is not reached by any test. The scalability test makes exactly 10,000 bookings and stops; one more booking should be tested.
3. **Session store full at login** (`auth.cpp` 172-173) returns `ERR_IO`; no test fills the session store.

## How to reproduce

```bash
cd RRS/04_IMPLEMENTATION_TESTING/source-code
./tools/coverage.sh            # builds with -DRRS_COVERAGE=ON, runs every test, writes the HTML report
```

The script uses `-fprofile-update=atomic` because several tests run many threads; without it gcov counters can go negative.

