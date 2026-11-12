# System Validation Report

**Project:** Railway Reservation System (RRS), Team 5  
**Run on:** 12 Nov 2026  
**Code:** `main` at `19a70a6`  
**Scope:** every requirement in SRS v1.1 (FR-001 to FR-024, NFR-001 to NFR-009, SEC-001 to SEC-006), validated as described in the Test Plan  
**Prepared by:** Samhitha S (QA Lead)

## Summary

| Requirements | Validated (pass) | Pass with open defect | Failed |
|---|---|---|---|
| 39 | 38 | 1 | 0 |

Each requirement was validated by automated tests (GoogleTest, run in CI), by system tests through the real ncurses TUI, or both. NFR-006 and NFR-007 were validated by code review. One defect (D-01) was found during system testing and is still open; it does not block any requirement but makes one search result unusable (details below).

Security requirements SEC-001 to SEC-006 also have their own report with more detail: [`security_validation_report.md`](../RRS/04_IMPLEMENTATION_TESTING/security-validation/security_validation_report.md).

## Exit criteria (Test Plan section 5)

| Criterion | Result |
|---|---|
| All high-priority test cases executed | Met: all 266 automated tests and all 9 TUI system tests were run |
| 0 critical defects open | Met: D-01 is medium (no wrong booking or charge is possible) |
| At least 70% unit coverage per module | Met: 87% to 100% line coverage per module |
| NFR-001 and NFR-002 targets met | Met: search p95 46.5 ms (limit 2 s), booking p95 0.003 ms (limit 1 s) |

## Requirement results

### Functional requirements

| ID | Requirement | Automated tests | System tests | Result | Notes |
|---|---|---|---|---|---|
| FR-001 | Register with unique email and a complex password | `AuthTest.RegisterValidPassengerSucceeds`<br>`AuthTest.RegisterRejectsWeakPassword`<br>`ValidationTest.PasswordComplexityBoundaries`<br>`ValidationTest.Emails` | ST-TUI-01 | **Pass** |  |
| FR-002 | Reject a duplicate email | `AuthTest.DuplicateEmailIsRejected`<br>`AuthTest.DuplicateCheckIsCaseInsensitive`<br>`AuthTest.FailedRegistrationDoesNotConsumeEmail` | - | **Pass** |  |
| FR-003 | Authenticate and create a role-tagged session | `LoginTest.ValidLoginReturnsPassengerSession`<br>`LoginTest.AdminAccountGetsAdminRole`<br>`LoginTest.WrongPasswordFailsAndLeavesSessionEmpty`<br>`LoginTest.UnknownEmailGivesSameErrorAsWrongPassword` | ST-TUI-02 | **Pass** |  |
| FR-004 | Only admins can manage trains (RBAC) | `RbacTest.PassengerSessionIsForbiddenFromAdminFunction`<br>`RbacTest.EditingTheRoleFieldDoesNotEscalatePrivileges`<br>`InventoryTest.PassengerCannotCreateATrain`<br>`InventoryTest.PassengerCannotEditATrain`<br>`InventoryTest.PassengerCannotRetireATrain` | ST-TUI-06 | **Pass** |  |
| FR-005 | View and update own profile | `ProfileTest.ViewReturnsOwnDetails`<br>`ProfileTest.UpdateNameIsVisibleInTheProfile`<br>`ProfileTest.UpdateOnlyAffectsTheCallersOwnAccount` | - | **Pass** | API only: the TUI has no profile screen yet |
| FR-006 | End the session on idle timeout or logout | `RbacTest.SessionIsExpiredAtTheIdleTimeout`<br>`SessionLifecycleTest.LogoutEndsTheSession`<br>`SessionLifecycleTest.ActivityRestartsTheIdleTimer` | - | **Pass** |  |
| FR-007 | Admin creates a train with halts and capacities | `InventoryTest.AdminCreatesATrainAndCanReadItBack`<br>`InventoryTest.CreatedTrainBecomesSearchable`<br>`InventoryTest.DuplicateTrainIdIsRejectedAndOriginalKept` | ST-TUI-06 | **Pass** |  |
| FR-008 | Admin edits route, timings or capacities | `InventoryTest.AdminEditsCapacitiesAndTheyAreStored`<br>`InventoryTest.EditedRouteReplacesTheOldOneInSearch`<br>`InventoryTest.EditedTimingsShowUpInSearchResults` | - | **Pass** | API only: the TUI has add and retire screens, no edit screen |
| FR-009 | Admin retires a train; history stays viewable | `InventoryTest.RetiredTrainIsNoLongerBookableOrSearchable`<br>`InventoryTest.RetiredTrainRecordIsPreserved` | ST-TUI-08 | **Pass** |  |
| FR-010 | Halts must form a valid connected path | `RouteValidationTest.ConnectedRouteIsAccepted`<br>`RouteValidationTest.RepeatedStationIsRejected`<br>`RouteValidationTest.TimesMustNotGoBackwards`<br>`RouteValidationTest.DistanceMustIncrease`<br>`InventoryTest.InvalidRouteIsRejectedAndNothingIsSaved` | ST-TUI-06 | **Pass** |  |
| FR-011 | Capacity cannot drop below confirmed bookings | `CapacityGuardTest.ReductionBelowConfirmedBookingsIsRejected`<br>`CapacityGuardTest.ReductionDownToConfirmedBookingsIsAllowed`<br>`CapacityGuardTest.ConflictRejectsTheWholeUpdateIncludingRouteChanges` | - | **Pass** |  |
| FR-012 | Return all direct routes | `SearchTest.DirectSearchReturnsAllMatchingTrainsRanked`<br>`SearchTest.TrainGoingTheOtherWayIsNotReturned`<br>`SearchInputTest.UnknownStationIsInvalidInput` | ST-TUI-03, ST-TUI-09 | **Pass (defect D-01 open)** | Defect D-01: results include trains that do not run the chosen class |
| FR-013 | Return connected (multi-leg) journeys | `ConnectionTest.MultiLegRouteViaBIsReturnedWhenNoDirectTrain`<br>`ConnectionTest.FasterConnectionIsListedBeforeASlowDirectTrain`<br>`ConnectionTest.TooShortTransferWaitsForTheNextDaysRun` | - | **Pass** |  |
| FR-014 | Fare by distance, class and concession | `FareTest.DirectSleeperNoConcessionMatchesHandComputedFare`<br>`FareTest.SeniorConcessionDiscountsTheDistanceCharge`<br>`FareTest.ClassChangesTheRate`<br>`FacadeTest.FareComesFromFareCalculateNotFromTheCaller` | ST-TUI-03, ST-TUI-04 | **Pass** |  |
| FR-015 | Eco-score (CO2 per passenger) | `EcoTest.Co2AndScoreForADirectSleeperTrip`<br>`EcoTest.AcClassesEmitMorePerPassenger`<br>`EcoTest.EachChangeOfTrainAddsAPenalty` | ST-TUI-03 | **Pass** |  |
| FR-016 | Sort and filter by fare, time or eco-score | `RankTest.SortByTimeFareAndEco`<br>`RankTest.Filters`<br>`RankTest.TiesKeepTheSearchOrder`<br>`RankIntegrationTest.RanksTheResultOfARealSearch` | ST-TUI-03 | **Pass** | Sort is in the TUI; filters are API only (no filter screen) |
| FR-017 | Search within the NFR-001 time limit | `SearchPerformanceTest.DefaultSearchMeetsNfr001`<br>`SearchPerformanceTest.ThreeTrainSearchMeetsNfr001` | - | **Pass** | p95 9.0 ms (default) and 46.5 ms (3 trains) on 200 stations / 500 trains |
| FR-018 | Booking and unique PNR only after payment succeeds | `BookingTest.CreateBookingSuccessfulPaymentGeneratesConfirmedPNR`<br>`BookingTest.MultipleBookingsGenerateDistinctPNRs` | ST-TUI-04, ST-TUI-07 | **Pass** |  |
| FR-019 | No booking when payment fails | `BookingTest.PaymentFailureRejectsBookingAndCreatesNoPNR`<br>`FacadeTest.FailedPaymentBooksNothing`<br>`PaymentRetryTest.TimeoutBeforeChargeLeavesNoBookingThenRetrySucceedsOnce` | - | **Pass** |  |
| FR-020 | Waitlist when full, first come first served | `BookingTest.EnqueuesWaitlistWhenClassFullAndAutoPromotesOnCancel`<br>`BookingTest.WaitlistEnqueuePreservesFcfsOrder` | ST-TUI-07 | **Pass** |  |
| FR-021 | Cancel a confirmed or waitlisted ticket | `BookingTest.CancelTicketPermissionsAndIdempotency`<br>`BookingTest.CancelWaitlistedTicketRemovesFromQueueAndShiftsPositions` | ST-TUI-05, ST-TUI-07 | **Pass** |  |
| FR-022 | Promote the waitlist head when a seat frees up | `BookingTest.EnqueuesWaitlistWhenClassFullAndAutoPromotesOnCancel`<br>`BookingTest.CancelThenPromoteNeverExceedsCapacity`<br>`ScalabilityTest.WaitlistHoldsTenThousandMinusOneForASingleSeat` | ST-TUI-07 | **Pass** |  |
| FR-023 | Show booking and waitlist status | `BookingTest.PassengerBookingListRetrieval` | ST-TUI-05, ST-TUI-07 | **Pass** |  |
| FR-024 | No duplicate booking on retry | `BookingTest.IdempotentRetryReturnsExistingBookingWithoutDuplicate`<br>`PaymentRetryTest.RetryAfterSuccessfulChargeReturnsSamePnrWithoutSecondCharge`<br>`FacadeTest.RetryWithSameKeyDoesNotDuplicate` | - | **Pass** |  |

### Non-functional requirements

| ID | Requirement | Automated tests | System tests | Result | Notes |
|---|---|---|---|---|---|
| NFR-001 | 95% of searches within 2 s | `SearchPerformanceTest.DefaultSearchMeetsNfr001`<br>`SearchPerformanceTest.ThreeTrainSearchMeetsNfr001` | - | **Pass** | p95 9.0 ms / 46.5 ms over 100 random queries; 100/100 found a route |
| NFR-002 | Booking confirmation within 1 s | `BookingTest.NFR002BookingConfirmationTimeUnderNormalLoadMeets1sLimit` | - | **Pass** | p95 0.003 ms, max 0.009 ms |
| NFR-003 | 99% availability | `AvailabilityTest.ValidRequestsSucceedAtLeast99PercentOverASoakRun`<br>`AvailabilityTest.ServiceKeepsRunningAfterBadInputAndFailedPayment`<br>`AvailabilityTest.ServiceRecoversAfterTrainRetireAndRestock` | - | **Pass** | 2,000-request soak run in-process; real uptime needs a deployment to measure |
| NFR-004 | Search to booking in at most 6 screens | - | ST-TUI-03, ST-TUI-04 | **Pass** | 5 screens: Search, Concession, Results, Confirm and pay, Ticket |
| NFR-005 | No duplicate booking on payment-timeout retry | `PaymentRetryTest.TimeoutBeforeChargeLeavesNoBookingThenRetrySucceedsOnce`<br>`PaymentRetryTest.RetryAfterSuccessfulChargeReturnsSamePnrWithoutSecondCharge`<br>`PaymentRetryTest.RepeatedTimeoutsThenSuccessThenRetriesStayAtOneBooking` | - | **Pass** |  |
| NFR-006 | Modular code with tests per module | - | - | **Pass (review)** | Code review: one CMake library and one test binary per module (source-code/MODULES.md) |
| NFR-007 | Engine testable without the UI | - | - | **Pass (review)** | All 266 automated tests call the engine directly; none needs the TUI |
| NFR-008 | 500 trains and 10,000 bookings | `ScalabilityTest.FiveHundredTrainsAreStoredAndSearchable`<br>`ScalabilityTest.TenThousandBookingsAcrossFiveHundredTrains`<br>`ScalabilityTest.WaitlistHoldsTenThousandMinusOneForASingleSeat`<br>`InventoryTest.FiveHundredTrainsAreSupported` | - | **Pass** | 10,000 bookings over 500 trains in 0.36 s |
| NFR-009 | No seat allocated twice, no data races | `BookingTest.NFR009NoDoubleBookingUnder50ConcurrentRequestsForLastSeat`<br>`BookingTest.NFR009SameSessionConcurrentBookingsNoDataRace` | - | **Pass** | 50 threads for 1 seat: exactly 1 confirmed; ThreadSanitizer clean |

### Security requirements

| ID | Requirement | Automated tests | System tests | Result | Notes |
|---|---|---|---|---|---|
| SEC-001 | Salted password hashes only | `SecurityValidation.Sec001PasswordsAreStoredAsSaltedHashesOnly`<br>`PasswordTest.SamePasswordGetsDifferentSalts`<br>`AuthTest.StoredRecordHoldsHashNotPlaintext` | - | **Pass** |  |
| SEC-002 | Passenger cannot call admin functions | `SecurityValidation.Sec002PassengerCannotCallAdminFunctions`<br>`SecurityValidation.Sec002PassengerCannotReadOrCancelAnotherPassengersBooking` | - | **Pass** |  |
| SEC-003 | Validate all user input | `SecurityValidation.Sec003SearchRejectsMalformedInput`<br>`SecurityValidation.Sec003BookingRejectsMalformedInput`<br>`SecurityValidation.Sec003AdminTrainInputIsValidated`<br>`InputFuzzTest.RandomBytesNeverCrashRegisterAndAreOnlyAcceptedWhenValid` | - | **Pass** |  |
| SEC-004 | Expired or invalid tokens rejected everywhere | `SecurityValidation.Sec004ExpiredAndInvalidTokensAreRejectedEverywhere`<br>`SessionSecurityTest.ExpiredTokenIsRejectedByEveryProtectedCall`<br>`SessionSecurityTest.MadeUpTokenIsRejectedByEveryProtectedCall` | - | **Pass** |  |
| SEC-005 | No payment details stored | `SecurityValidation.Sec005OnlyAPaymentFlagIsKept`<br>`BookingTest.SEC005NoSensitivePaymentDataInBookingRecord` | - | **Pass** |  |
| SEC-006 | Failed logins and admin actions audited | `SecurityValidation.Sec006FailedLoginsAndAdminActionsAreAudited`<br>`AuditTest.ThreeFailedLoginsAndAnAdminActionAreAllLogged`<br>`AuditTest.NoPasswordOrTokenEverAppearsInTheLog` | ST-TUI-02, ST-TUI-06 | **Pass** |  |

## System tests through the TUI

These were run on the real program (`./build/rrs_app`) in a terminal, driven by a script so every step is repeatable. Screenshots of every step are in [`RRS/05_FINAL_REPORT/screenshots/`](../RRS/05_FINAL_REPORT/screenshots/), and the same run is the Sprint 2/3 demo video ([`sprint2_tui_demo.mp4`](../RRS/04_IMPLEMENTATION_TESTING/demo-videos/sprint2_tui_demo.mp4)).

| ID | Test | Steps | Data | Expected result | Requirements | Result |
|---|---|---|---|---|---|---|
| ST-TUI-01 | Register a passenger from the TUI | Welcome > 2 Register; enter name, email, password | Riya Sharma / riya@example.com / Passw0rdOK | Password masked; 'Account created, you can log in now' | FR-001 | **Pass** |
| ST-TUI-02 | Wrong password is rejected, then login works | 1 Login with wrong password; then with the right one | wrongpass1, then Passw0rdOK | 'Wrong email or password' then passenger main menu; LOGIN_FAILED in data/audit.log | FR-003, SEC-006 | **Pass** |
| ST-TUI-03 | Search, sort and price a journey | 1 Search and book; SBC to NDLS, today; AC 3-Tier; Cheapest first; no concession | SBC, NDLS, AC3 | Results list both trains ranked by fare with eco-score: 22691 Rs 3824 (88), 12627 Rs 3880 (88) | FR-012, FR-014, FR-015, FR-016 | **Pass** |
| ST-TUI-04 | Book and pay | Select 22691; confirm; type y | y | Ticket screen: PNR PNR100000001, CONFIRMED, Rs 3824 (= 40 + 2365 km x 1.60) | FR-014, FR-018, NFR-004 | **Pass** |
| ST-TUI-05 | View status and cancel | 2 My bookings; Enter on the ticket; y | PNR100000001 | Shows CONFIRMED; after cancelling shows CANCELLED | FR-021, FR-023 | **Pass** |
| ST-TUI-06 | Admin adds a train | Login as admin; 1 Add a train | 12951, MAS to LKO, 1900 km, 1 seat per class | 'Train added'; ADMIN_ACTION fn=train_create in audit log; train is searchable | FR-004, FR-007, FR-010, SEC-006 | **Pass** |
| ST-TUI-07 | Waitlist and automatic promotion | Riya books it; Arjun books it; Riya cancels; Arjun opens My bookings | Two passengers | Riya CONFIRMED; Arjun WAITLISTED position 1; after cancel Arjun CONFIRMED | FR-020, FR-021, FR-022, FR-023 | **Pass** |
| ST-TUI-08 | Admin retires a train | 2 Retire a train; then search SBC to NDLS as a passenger | 12627 | 'Train retired'; 12627 no longer in results | FR-009 | **Pass** |
| ST-TUI-09 | Journey on a train without that class (defect D-01) | Search SBC to NDLS in Sleeper; pick 22691; pay | Sleeper | Expected: 22691 not offered in Sleeper. Actual: offered, then 'Invalid input' on pay (nothing booked) | FR-012, FR-016 | **Fail (D-01)** |

Audit log written during the run (`data/audit.log`, SEC-006):

```
event=ADMIN_ACTION user=<admin id> fn=train_create status=RRS_OK      (x3, seed data)
event=LOGIN_FAILED user=riya@example.com fn=auth_login status=ERR_AUTH_FAILED
event=ADMIN_ACTION user=<admin id> fn=train_create status=RRS_OK      (train 12951)
event=ADMIN_ACTION user=<admin id> fn=train_retire status=RRS_OK      (train 12627)
```

## Defect D-01 (open)

**Search offers journeys on a train that has no seats in the chosen class.**

- **Steps:** log in as a passenger, search SBC to NDLS, choose Sleeper, select train 22691, pay.
- **Expected:** 22691 has 0 Sleeper seats, so it should not be offered in Sleeper.
- **Actual:** it is listed (Rs 1439). Paying shows "Invalid input, please check the fields" and nothing is booked or charged. The same happens for connections that use 12004 (also 0 Sleeper seats).
- **Cause:** `search_routes` does not look at capacity, and `booking_create` (booking.c line 242) rejects a class with 0 seats as invalid input.
- **Fix:** in `facade_search`, drop options where any leg's train has 0 capacity in the chosen class (using `train_get`), and give the error a clearer message. Add a facade test for it.
- **Severity:** medium. **Owner:** facade/TUI.

## Known limitations (not defects)

- **No persistence yet:** `persistence/store.c` is empty, so users, trains and bookings reset when the program exits. The audit log is the only file written.
- **Demo admin account:** the program creates `admin@rrs.in` at start-up with a fixed password so the admin screens can be shown. A real deployment must not do this.
- **API-only features:** profile view/update (FR-005), editing a train (FR-008) and result filters (FR-016) work and are tested through the API, but the TUI has no screen for them yet.
- **NFR-003** was checked with an in-process soak run; actual 99% uptime can only be measured on a deployed system.

