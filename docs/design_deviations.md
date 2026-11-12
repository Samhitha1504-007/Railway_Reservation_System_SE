# Design Deviations

**Compares:** the Software Architecture & Design Specification v1.0 (`RRS/03_DESIGN/`) and SRS v1.1 with the code on `main` (12 Nov 2026)  
**Prepared by:** Samhitha S

The Design document froze the module APIs after the Sprint 1 review (section 4.3). The changes below were made during the sprints. None of them changes what a requirement asks for. Each one is listed with the reason for it and the requirement it serves.

## 1. API changes (Design section 4.3)

| Function | Design | Implemented | Why | Req |
|---|---|---|---|---|
| `booking_create` | `(session, route, class, idempotency_key) → booking` | `(session, train_id, class, fare_amount, idempotency_key, out)` | A connecting journey is several tickets on different trains, each with its own seats and waitlist. Booking works on one train, and the facade's `facade_book` books every leg of a route, rolling back the earlier legs if a later one fails. The fare is passed in, so booking does not depend on the search module; the facade always passes `fare_calculate()`'s value (security finding 1, fixed in #77). | FR-018, FR-024, SEC-003 |
| `eco_score` | `(route) → score` | `(route, class, out)` filling `rrs_eco_t {co2_kg, car_co2_kg, score}` | CO2 per passenger depends on the class (Sleeper 8, AC 3-Tier 14, AC 2-Tier 20 g/km). Returning the CO2 and the car comparison as well as the 0-100 score lets the TUI show "33.1 kg CO2" next to the score. | FR-015 |
| `search_rank` | not in the Design | `(routes, class, concession, sort_key, filter, out)` | FR-016 and SRS 5.3.2 ask for sort and filter "without a full re-search". `search_rank` prices, scores, filters and sorts an existing result list. | FR-016 |
| `search_default_opts` | not in the Design | fills `rrs_search_opts_t` (max legs, minimum transfer time, max results) | The `opts` argument of `search_routes` needed documented defaults. | FR-012, FR-013 |
| `search_routes` | requires a passenger session | takes no session | The search engine has no dependency on auth, so it can be tested on its own (NFR-007). The facade checks the session before calling it (`facade_search`). | NFR-006, NFR-007 |
| Auth | `auth_register`, `auth_login` | adds `auth_authorize`, `auth_authorize_for`, `auth_logout`, `auth_profile_get`, `auth_profile_update` | Needed for RBAC on every call (FR-004), logout (FR-006), profiles (FR-005) and audited admin calls (SEC-006). | FR-004 to FR-006, SEC-006 |
| Inventory | `train_create / update / retire` | adds `train_get` and `train_is_bookable` | Booking needs to read a train's capacity and status. | FR-009, FR-018 |
| Facade | one facade in front of every module | `facade_init`, `facade_seed_demo`, `facade_search`, `facade_book`, `facade_my_bookings`, `facade_cancel` | As designed for the passenger flow. The TUI's two admin screens call `train_create` and `train_retire` directly. Those functions do their own session and role checks, so RBAC is still enforced. | SEC-002 |

## 2. Persistence (Design sections 3.1, 3.6)

**Design:** flat files or a lightweight embedded database behind a common interface.  
**Implemented:** not yet. `persistence/store.c` is empty, so everything is kept in memory and resets when the program exits. The only file written is the audit log (`data/audit.log`).  
**Why:** the team put the remaining time into the TUI (Sprint 3). Every module keeps its data behind its own functions, so a store can be added later without changing the module APIs.  
**Effect:** NFR-003 (availability) and SEC-001 / SEC-005 were validated on in-memory data (security finding 5).

## 3. Audit logging (Design section 4.4)

**Design:** every non-OK result from a protected call is written to the audit log.  
**Implemented:** failed logins, rejected or expired sessions, denied admin calls and successful admin actions are logged. Two gaps remain:
- A passenger denied access to someone else's booking is not logged (security finding 3).
- `ADMIN_ACTION` is written when the role check passes, before the action runs (finding 4).

## 4. TUI (Design section 4.5)

| Design | Implemented |
|---|---|
| Header shows the logged-in user and role on every screen | Header shows the app name and the screen title. The menu itself tells passenger and admin apart. |
| Validation messages shown under the field | Shown in the status line at the bottom of the screen |
| Search to booking in at most 6 screens (NFR-004) | 5 screens: Search, Concession, Results, Confirm and pay, Ticket |
| Waitlist position always visible | Shown on the ticket and in My bookings |
| Screens for profile, train editing and result filters | Not built. FR-005, FR-008 and the filter half of FR-016 work and are tested through the API only. |
| — | The program creates a demo admin account at start-up so the admin screens can be shown (security finding 7) |

## 5. Concurrency

The Design asks for race-free waitlist promotion. Booking uses one global mutex around every booking and waitlist operation, including the session check (#70). This is simple and was verified with ThreadSanitizer. A slow real payment gateway would serialise all bookings, but for the mock this is acceptable.

## 6. Schedule

| Plan (SRS / Test Plan) | Actual |
|---|---|
| Four one-week sprints, final validation report on 25 Oct | The course sheet set two sprints. Sprint 1 covered auth, inventory and search; Sprint 2 covered booking, waitlist and security. |
| Sprint 3 not planned | Spill-over from Sprint 2 was finished in an optional Sprint 3 (2-12 Nov), as Part-3 of the course sheet allows: TUI and facade (#77), NFR-003/005/006 (#73, #75, #76) and booking thread safety (#70). |
