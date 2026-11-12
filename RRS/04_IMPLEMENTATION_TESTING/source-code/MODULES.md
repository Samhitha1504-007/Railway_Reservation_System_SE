# Module structure (NFR-006)

Each module is its own CMake library with one public header in `include/`, private sources under `src/<module>/`, and its own gtest executable registered with `ctest`. Modules talk only through the public headers.

| Module | Owner | Library | Public header | Unit tests |
|---|---|---|---|---|
| Auth, RBAC, sessions | Ridhima | `rrs_auth` | `auth.h` | `test_auth.cpp`, `tests/security/` |
| Audit log | Ridhima | `rrs_audit` | `audit.h` | `tests/security/` |
| Search, fare, eco-score | Samhitha | `rrs_search` | `search.h`, `fare_eco.h` | `test_search.cpp`, `test_fare_eco.cpp`, `test_search_perf.cpp` |
| Inventory and schedule | Adhithya | `rrs_inventory` | `inventory.h` | `test_inventory.cpp` |
| Booking and waitlist | Saathvik | `rrs_booking` | `booking.h` | `test_booking.cpp` |

Cross-module tests: `test_payment_retry.cpp` (NFR-005), `test_availability.cpp` (NFR-003), `test_scalability.cpp` (NFR-008), `test_security_validation.cpp` (SEC-001 to SEC-006).

Dependencies point one way: booking -> inventory -> search, and booking/inventory -> auth for role checks. Search and auth depend on no other module. Booking reaches inventory through `train_get` and `train_is_bookable`, and inventory reaches booking only through the `inventory_set_booking_counter` hook, so neither links the other's internals.

Run everything:

```bash
cmake -S . -B build && cmake --build build --parallel && ctest --test-dir build --output-on-failure
```
