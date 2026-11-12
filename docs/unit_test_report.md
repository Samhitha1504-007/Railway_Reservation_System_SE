# Unit Test Report

**Project:** Railway Reservation System (RRS), Team 5  
**Run on:** 12 Nov 2026, Ubuntu, GCC, CMake Debug build, GoogleTest  
**Code:** `main` at `19a70a6` (after PRs #70 to #77)  
**Prepared by:** Samhitha S (QA Lead)

## Summary

| Test binaries | Test cases | Passed | Failed | Skipped | Pass rate |
|---|---|---|---|---|---|
| 12 | 266 | 266 | 0 | 0 | 100% |

Every binary is registered with CTest and runs in CI (GitHub Actions) on each push and pull request to `main`; a failing test turns the pull request red. The JUnit XML for each binary is in [`test-results/`](test-results/).

## Results by module

| Test binary | Module | Owner | Level | Tests | Passed | Failed | Time (s) |
|---|---|---|---|---|---|---|---|
| `rrs_auth_tests` | Auth & RBAC | Ridhima Jain | Unit | 68 | 68 | 0 | 6.13 |
| `rrs_security_tests` | Auth & RBAC / Audit (security) | Ridhima Jain | Security | 43 | 43 | 0 | 7.96 |
| `rrs_inventory_tests` | Inventory & Schedule | S S Adhithya Sriram | Unit | 55 | 55 | 0 | 6.09 |
| `rrs_search_tests` | Search engine | Samhitha S | Unit | 29 | 29 | 0 | 0.00 |
| `rrs_fare_eco_tests` | Fare, eco-score & ranking | Samhitha S | Unit | 22 | 22 | 0 | 0.00 |
| `rrs_search_perf_tests` | Search performance (NFR-001) | Samhitha S | Performance | 2 | 2 | 0 | 10.95 |
| `rrs_booking_tests` | Booking & waitlist | Saathvik Shenoy Padubidri | Unit | 20 | 20 | 0 | 6.40 |
| `rrs_payment_retry_tests` | Payment retry (NFR-005) | S S Adhithya Sriram | Integration | 6 | 6 | 0 | 1.48 |
| `rrs_availability_tests` | Availability soak (NFR-003) | S S Adhithya Sriram | Soak | 3 | 3 | 0 | 0.40 |
| `rrs_scalability_tests` | Scalability (NFR-008) | Samhitha S | Load | 3 | 3 | 0 | 5.12 |
| `rrs_security_validation_tests` | Security validation SEC-001..006 | Samhitha S | System / security | 9 | 9 | 0 | 1.83 |
| `rrs_facade_tests` | Facade (TUI back end) | S S Adhithya Sriram | Integration | 6 | 6 | 0 | 0.82 |
| **Total** | | | | **266** | **266** | **0** | |

Times are from the coverage-instrumented build, which runs slower than a normal build. The performance limits (NFR-001, NFR-002) are checked inside the tests on every run.

## Measured values

| What | Requirement | Measured | Limit |
|---|---|---|---|
| Search, 200 stations / 500 trains, 100 random queries | NFR-001 / FR-017 | p95 9.0 ms (default), 46.5 ms (3 trains) | p95 within 2 s |
| Booking confirmation | NFR-002 | p95 0.003 ms, max 0.009 ms | 1 s |
| 10,000 bookings over 500 trains | NFR-008 | 0.36 s; 7,500 confirmed, 2,500 waitlisted | supported without redesign |
| 50 threads booking the last seat | NFR-009 | exactly 1 confirmed, 49 waitlisted; ThreadSanitizer clean | no double booking |
| 2,000-request soak run | NFR-003 | at least 99% of valid requests succeed | 99% |

## Other checks

- **Static analysis:** cppcheck (`warning, performance, portability`) runs in CI with `--error-exitcode=1`; it is clean on `main`.
- **Thread safety:** the booking module was run under ThreadSanitizer with 8 threads sharing one session and with 50 concurrent sessions: 0 warnings (the race found earlier was fixed in #70).
- **Memory and undefined behaviour:** all 12 binaries were also run under AddressSanitizer (with leak detection) and UndefinedBehaviorSanitizer on 12 Nov: all tests pass, 0 findings.
- **Coverage:** see the [Code Coverage Report](code_coverage_report.md) and the [Branch Coverage Report](branch_coverage_report.md).

## Defects

| ID | Found by | Description | Severity | Status |
|---|---|---|---|---|
| D-01 | System test ST-TUI-09 | Search offers journeys on trains that have no seats in the chosen class (e.g. 22691 in Sleeper). Choosing one fails at payment with "Invalid input". Nothing is booked or charged. | Medium | Open: search or the facade should skip a train whose capacity in the chosen class is 0 |
| D-02 | Security pass (#36) | Inventory admin calls were not audited | Medium | Fixed in #72 |
| D-03 | Security pass (#36) | Waitlist capped at 4,096 entries (NFR-008 needs 10,000) | Medium | Fixed in #72 |
| D-04 | Review of #70 | Data race on a shared session in booking (ThreadSanitizer) | High | Fixed in #70 |
| D-05 | Review of #77 | Fare passed in by the caller of `booking_create` | Medium | Fixed in #77 (facade computes it) |

## All test cases

### `rrs_auth_tests`: Auth & RBAC (68 tests, all passed)

Source: `tests/test_auth.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `PasswordTest.HashIsNotPlaintextAndVerifies` | Pass |
| 2 | `PasswordTest.WrongPasswordFailsVerify` | Pass |
| 3 | `PasswordTest.SamePasswordGetsDifferentSalts` | Pass |
| 4 | `PasswordTest.MalformedEncodedStringIsRejected` | Pass |
| 5 | `ValidationTest.Emails` | Pass |
| 6 | `ValidationTest.PasswordComplexityBoundaries` | Pass |
| 7 | `ValidationTest.Names` | Pass |
| 8 | `AuthTest.RegisterValidPassengerSucceeds` | Pass |
| 9 | `AuthTest.RegisterRejectsNullInputs` | Pass |
| 10 | `AuthTest.RegisterRejectsBadEmail` | Pass |
| 11 | `AuthTest.RegisterRejectsWeakPassword` | Pass |
| 12 | `AuthTest.RegisterRejectsEmptyName` | Pass |
| 13 | `AuthTest.StoredRecordHoldsHashNotPlaintext` | Pass |
| 14 | `AuthTest.TwoUsersSamePasswordHaveDifferentHashes` | Pass |
| 15 | `AuthTest.DuplicateEmailIsRejected` | Pass |
| 16 | `AuthTest.DuplicateCheckIsCaseInsensitive` | Pass |
| 17 | `AuthTest.FailedRegistrationDoesNotConsumeEmail` | Pass |
| 18 | `LoginTest.ValidLoginReturnsPassengerSession` | Pass |
| 19 | `LoginTest.SessionExpiryIsInTheFutureByTheTtl` | Pass |
| 20 | `LoginTest.WrongPasswordFailsAndLeavesSessionEmpty` | Pass |
| 21 | `LoginTest.UnknownEmailGivesSameErrorAsWrongPassword` | Pass |
| 22 | `LoginTest.EmailMatchIsCaseInsensitive` | Pass |
| 23 | `LoginTest.PasswordMatchIsCaseSensitive` | Pass |
| 24 | `LoginTest.NullArgumentsAreRejected` | Pass |
| 25 | `LoginTest.EmptyCredentialsFailAuthentication` | Pass |
| 26 | `LoginTest.TwoLoginsGiveDifferentTokensForTheSameUser` | Pass |
| 27 | `LoginTest.DifferentUsersGetDifferentUserIds` | Pass |
| 28 | `LoginTest.AdminAccountGetsAdminRole` | Pass |
| 29 | `LoginTest.SelfRegistrationNeverProducesAnAdmin` | Pass |
| 30 | `RbacTest.AdminSessionMayCallAdminFunction` | Pass |
| 31 | `RbacTest.PassengerSessionIsForbiddenFromAdminFunction` | Pass |
| 32 | `RbacTest.PassengerSessionMayCallPassengerFunction` | Pass |
| 33 | `RbacTest.AdminSessionDoesNotSatisfyPassengerOnlyCheck` | Pass |
| 34 | `RbacTest.NullSessionIsRejected` | Pass |
| 35 | `RbacTest.MadeUpTokenIsRejected` | Pass |
| 36 | `RbacTest.EmptyTokenIsRejected` | Pass |
| 37 | `RbacTest.TokenWithoutTerminatorIsRejected` | Pass |
| 38 | `RbacTest.EditingTheRoleFieldDoesNotEscalatePrivileges` | Pass |
| 39 | `RbacTest.EditingTheUserIdFieldInvalidatesTheSession` | Pass |
| 40 | `RbacTest.SessionIsValidJustBeforeTheIdleTimeout` | Pass |
| 41 | `RbacTest.SessionIsExpiredAtTheIdleTimeout` | Pass |
| 42 | `RbacTest.ExpiredSessionIsReportedAsExpiredNotForbidden` | Pass |
| 43 | `RbacTest.SessionsDoNotSurviveAStoreReset` | Pass |
| 44 | `SessionLifecycleTest.LogoutEndsTheSession` | Pass |
| 45 | `SessionLifecycleTest.LoggingOutTwiceFailsTheSecondTime` | Pass |
| 46 | `SessionLifecycleTest.LogoutWithNullSessionIsRejected` | Pass |
| 47 | `SessionLifecycleTest.LogoutOnlyEndsThatSession` | Pass |
| 48 | `SessionLifecycleTest.LogoutWithEditedUserIdIsRejectedAndSessionSurvives` | Pass |
| 49 | `SessionLifecycleTest.LogoutWithMadeUpTokenIsRejectedAndSessionSurvives` | Pass |
| 50 | `SessionLifecycleTest.LogoutAfterTheIdleTimeoutIsRejected` | Pass |
| 51 | `SessionLifecycleTest.ExpiredSessionIsRemovedFromTheStore` | Pass |
| 52 | `SessionLifecycleTest.ActivityRestartsTheIdleTimer` | Pass |
| 53 | `SessionLifecycleTest.RejectedCallsDoNotRestartTheIdleTimer` | Pass |
| 54 | `ProfileTest.ViewReturnsOwnDetails` | Pass |
| 55 | `ProfileTest.ViewNeverShowsAnotherUsersDetails` | Pass |
| 56 | `ProfileTest.ViewRejectsBadSessionsAndZeroesTheOutput` | Pass |
| 57 | `ProfileTest.EditedUserIdInTheSessionCopyDoesNotShowSomeoneElsesProfile` | Pass |
| 58 | `ProfileTest.AdminCanViewOwnProfile` | Pass |
| 59 | `ProfileTest.UpdateNameIsVisibleInTheProfile` | Pass |
| 60 | `ProfileTest.UpdateEmailMovesLoginToTheNewAddress` | Pass |
| 61 | `ProfileTest.UpdateKeepsThePasswordAndRole` | Pass |
| 62 | `ProfileTest.UpdateToAnotherUsersEmailIsRejectedAndNothingChanges` | Pass |
| 63 | `ProfileTest.UpdateToOwnEmailInADifferentCaseIsAllowed` | Pass |
| 64 | `ProfileTest.UpdateRejectsInvalidValuesAndNothingChanges` | Pass |
| 65 | `ProfileTest.UpdateWithNothingToChangeIsRejected` | Pass |
| 66 | `ProfileTest.UpdateOnlyAffectsTheCallersOwnAccount` | Pass |
| 67 | `ProfileTest.ExpiredOrLoggedOutSessionsCannotUpdate` | Pass |
| 68 | `ProfileTest.IdleExpiredSessionCannotViewOrUpdate` | Pass |

### `rrs_security_tests`: Auth & RBAC / Audit (security) (43 tests, all passed)

Source: `tests/security/`

| # | Test | Result |
|---|---|---|
| 1 | `AuditTest.ThreeFailedLoginsAndAnAdminActionAreAllLogged` | Pass |
| 2 | `AuditTest.FailedLoginEntryHasTimestampUserFunctionAndStatus` | Pass |
| 3 | `AuditTest.SuccessfulLoginIsNotLogged` | Pass |
| 4 | `AuditTest.ForbiddenAttemptIsLoggedWithTheServersUserIdAndFunction` | Pass |
| 5 | `AuditTest.ForgedUserIdInTheCallersCopyIsNeverWrittenToTheLog` | Pass |
| 6 | `AuditTest.PlainAuthorizeLogsRejectedSessionsButNotWrongRoles` | Pass |
| 7 | `AuditTest.PassengerSuccessIsNotLoggedAdminSuccessIs` | Pass |
| 8 | `AuditTest.DeadSessionsOnProfileAndLogoutAreLoggedWithTheirFunctionNames` | Pass |
| 9 | `AuditTest.NoPasswordOrTokenEverAppearsInTheLog` | Pass |
| 10 | `AuditTest.NewlinesAndFakeFieldsInAnAttemptedEmailCannotForgeLogLines` | Pass |
| 11 | `AuditTest.AVeryLongEmailIsCutInTheLog` | Pass |
| 12 | `AuditTest.OnlyTheMostRecentEntriesAreKeptInMemory` | Pass |
| 13 | `AuditTest.EventsAreAppendedToTheLogFile` | Pass |
| 14 | `AuditTest.ReopeningTheFileKeepsEarlierLines` | Pass |
| 15 | `AuditTest.AnUnwritableLogFileReportsErrIoButNeverBreaksTheCaller` | Pass |
| 16 | `AuditTest.NullFieldsAreLoggedAsDashes` | Pass |
| 17 | `InputFuzzTest.OverLongFieldsAreRejectedByRegister` | Pass |
| 18 | `InputFuzzTest.OverLongFieldsFailCleanlyAtLogin` | Pass |
| 19 | `InputFuzzTest.OverLongFieldsAreRejectedByProfileUpdateAndChangeNothing` | Pass |
| 20 | `InputFuzzTest.LengthBoundariesAreExact` | Pass |
| 21 | `InputFuzzTest.EmailsWithSpecialCharactersAreRejected` | Pass |
| 22 | `InputFuzzTest.OrdinaryEmailsStillWork` | Pass |
| 23 | `InputFuzzTest.NamesWithControlCharactersAreRejected` | Pass |
| 24 | `InputFuzzTest.NamesWithPunctuationAndSqlOrHtmlAreStoredAsPlainText` | Pass |
| 25 | `InputFuzzTest.EmptyAndBlankFieldsAreRejected` | Pass |
| 26 | `InputFuzzTest.NegativeAndHugeNumbersAsTextNeverCrashOrSlipThrough` | Pass |
| 27 | `InputFuzzTest.RandomBytesNeverCrashRegisterAndAreOnlyAcceptedWhenValid` | Pass |
| 28 | `InputFuzzTest.RandomEmailLikeStringsAreAcceptedOnlyWhenTheValidatorSaysSo` | Pass |
| 29 | `InputFuzzTest.RandomBytesNeverCrashLoginAndNeverSucceed` | Pass |
| 30 | `InputFuzzTest.RandomBytesNeverCrashProfileUpdateAndRejectedValuesChangeNothing` | Pass |
| 31 | `InputFuzzTest.AuditLinesStayOnOneLineOfPlainAsciiUnderFuzz` | Pass |
| 32 | `SessionSecurityTest.ControlLiveSessionIsAcceptedByTheCallsThatShouldAcceptIt` | Pass |
| 33 | `SessionSecurityTest.ExpiredTokenIsRejectedByEveryProtectedCall` | Pass |
| 34 | `SessionSecurityTest.ExpiredTokenStaysRejectedWhenReused` | Pass |
| 35 | `SessionSecurityTest.ExpiredTokenIsNotRevivedWhenTheUserLogsInAgain` | Pass |
| 36 | `SessionSecurityTest.ExpiredSessionIsReportedAsExpiredNeverForbidden` | Pass |
| 37 | `SessionSecurityTest.MadeUpTokenIsRejectedByEveryProtectedCall` | Pass |
| 38 | `SessionSecurityTest.EmptyAndUnterminatedTokensAreRejectedByEveryProtectedCall` | Pass |
| 39 | `SessionSecurityTest.NullSessionIsRejectedByEveryProtectedCall` | Pass |
| 40 | `SessionSecurityTest.TokenIsDeadAfterLogoutForEveryProtectedCall` | Pass |
| 41 | `SessionSecurityTest.TokenWithEditedUserIdIsRejectedByEveryProtectedCall` | Pass |
| 42 | `SessionSecurityTest.OneUsersTokenCannotActAsAnotherUser` | Pass |
| 43 | `SessionSecurityTest.ARejectedCallLeavesTheRealSessionWorking` | Pass |

### `rrs_inventory_tests`: Inventory & Schedule (55 tests, all passed)

Source: `tests/test_inventory.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `RouteValidationTest.ConnectedRouteIsAccepted` | Pass |
| 2 | `RouteValidationTest.TwoHaltsIsTheSmallestRoute` | Pass |
| 3 | `RouteValidationTest.NullAndBadCountsAreRejected` | Pass |
| 4 | `RouteValidationTest.MaxHaltsIsAccepted` | Pass |
| 5 | `RouteValidationTest.BadStationCodesAreRejected` | Pass |
| 6 | `RouteValidationTest.RepeatedStationIsRejected` | Pass |
| 7 | `RouteValidationTest.FirstHaltMustStartAtZeroKmWithoutDwell` | Pass |
| 8 | `RouteValidationTest.LastHaltMustNotDwell` | Pass |
| 9 | `RouteValidationTest.TimesMustNotGoBackwards` | Pass |
| 10 | `RouteValidationTest.DistanceMustIncrease` | Pass |
| 11 | `RouteValidationTest.OvernightRunningIsAccepted` | Pass |
| 12 | `RouteValidationTest.TimesAndDistancesHaveUpperBounds` | Pass |
| 13 | `InventoryTest.AdminCreatesATrainAndCanReadItBack` | Pass |
| 14 | `InventoryTest.CreatedTrainBecomesSearchable` | Pass |
| 15 | `InventoryTest.ClassWithZeroCapacityIsAllowedWhileAnotherHasSeats` | Pass |
| 16 | `InventoryTest.PassengerCannotCreateATrain` | Pass |
| 17 | `InventoryTest.MissingForgedOrLoggedOutSessionCannotCreate` | Pass |
| 18 | `InventoryTest.DuplicateTrainIdIsRejectedAndOriginalKept` | Pass |
| 19 | `InventoryTest.BadTrainIdsAreRejected` | Pass |
| 20 | `InventoryTest.InvalidRouteIsRejectedAndNothingIsSaved` | Pass |
| 21 | `InventoryTest.BadCapacitiesAreRejected` | Pass |
| 22 | `InventoryTest.CallerCannotChooseTheInitialStatus` | Pass |
| 23 | `InventoryTest.StoredTrainIsACopyOfTheCallersStruct` | Pass |
| 24 | `InventoryTest.FiveHundredTrainsAreSupported` | Pass |
| 25 | `InventoryTest.GetNeedsALiveSessionButNotAdmin` | Pass |
| 26 | `InventoryTest.GetUnknownTrainOrNullArgumentsFail` | Pass |
| 27 | `InventoryTest.AdminEditsCapacitiesAndTheyAreStored` | Pass |
| 28 | `InventoryTest.EditedRouteReplacesTheOldOneInSearch` | Pass |
| 29 | `InventoryTest.EditedTimingsShowUpInSearchResults` | Pass |
| 30 | `InventoryTest.EditingOneTrainLeavesOthersUntouched` | Pass |
| 31 | `InventoryTest.PassengerCannotEditATrain` | Pass |
| 32 | `InventoryTest.MissingOrForgedSessionCannotEdit` | Pass |
| 33 | `InventoryTest.EditingAnUnknownTrainFails` | Pass |
| 34 | `InventoryTest.InvalidEditIsRejectedAndOldScheduleKept` | Pass |
| 35 | `InventoryTest.EditCannotChangeStatus` | Pass |
| 36 | `InventoryTest.RetiredTrainIsNoLongerBookableOrSearchable` | Pass |
| 37 | `InventoryTest.RetiredTrainRecordIsPreserved` | Pass |
| 38 | `InventoryTest.RetiringOneTrainLeavesOthersBookable` | Pass |
| 39 | `InventoryTest.RetireIsRepeatableAndIdUnusable` | Pass |
| 40 | `InventoryTest.RetiredTrainCannotBeEdited` | Pass |
| 41 | `InventoryTest.PassengerCannotRetireATrain` | Pass |
| 42 | `InventoryTest.MissingOrForgedSessionCannotRetire` | Pass |
| 43 | `InventoryTest.RetireUnknownTrainOrNullIdFails` | Pass |
| 44 | `InventoryTest.UnknownOrNullTrainIsNotBookable` | Pass |
| 45 | `CapacityGuardTest.ReductionBelowConfirmedBookingsIsRejected` | Pass |
| 46 | `CapacityGuardTest.ReductionDownToConfirmedBookingsIsAllowed` | Pass |
| 47 | `CapacityGuardTest.EveryClassIsChecked` | Pass |
| 48 | `CapacityGuardTest.ConflictRejectsTheWholeUpdateIncludingRouteChanges` | Pass |
| 49 | `CapacityGuardTest.IncreasingCapacityIsAlwaysAllowed` | Pass |
| 50 | `CapacityGuardTest.DroppingAClassToZeroNeedsNoConfirmedBookingsInIt` | Pass |
| 51 | `CapacityGuardTest.RouteEditsAreAllowedWhileBookingsExist` | Pass |
| 52 | `CapacityGuardTest.OnlyThisTrainsBookingsCount` | Pass |
| 53 | `CapacityGuardTest.WithoutACounterNothingIsConfirmed` | Pass |
| 54 | `CapacityGuardTest.GuardRunsAfterSessionAndInputChecks` | Pass |
| 55 | `CapacityGuardTest.RetireIsNotBlockedByConfirmedBookings` | Pass |

### `rrs_search_tests`: Search engine (29 tests, all passed)

Source: `tests/test_search.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `SearchTest.AddValidTrainSucceeds` | Pass |
| 2 | `SearchTest.DuplicateTrainIdIsRejected` | Pass |
| 3 | `SearchTest.InvalidTrainsAreRejected` | Pass |
| 4 | `SearchTest.TimesAndDistancesHaveUpperBounds` | Pass |
| 5 | `SearchTest.HugeTimeCannotOverflowTheSearch` | Pass |
| 6 | `SearchTest.FirstAndLastHaltMustNotDwell` | Pass |
| 7 | `SearchTest.StationCodeWithoutTerminatorIsRejected` | Pass |
| 8 | `SearchTest.RejectedTrainIsNotLoaded` | Pass |
| 9 | `SearchTest.DirectSearchReturnsAllMatchingTrainsRanked` | Pass |
| 10 | `SearchTest.DirectLegUsesIntermediateHaltsAndDistance` | Pass |
| 11 | `SearchTest.TrainGoingTheOtherWayIsNotReturned` | Pass |
| 12 | `SearchTest.OvernightTrainIsReportedOnTheTravelDate` | Pass |
| 13 | `SearchTest.MaxResultsTruncatesToTheBest` | Pass |
| 14 | `SearchTest.DisconnectedStationsGiveNoRoute` | Pass |
| 15 | `SearchTest.SameTrainsConnectingAtTwoStationsAreListedOnce` | Pass |
| 16 | `SearchTest.JourneyNeverUsesTheSameTrainTwice` | Pass |
| 17 | `SearchInputTest.UnknownStationIsInvalidInput` | Pass |
| 18 | `SearchInputTest.NullArgumentsAreRejected` | Pass |
| 19 | `SearchInputTest.SameOriginAndDestinationIsRejected` | Pass |
| 20 | `SearchInputTest.MalformedStationCodesAreRejected` | Pass |
| 21 | `SearchInputTest.Dates` | Pass |
| 22 | `SearchInputTest.OutOfRangeOptionsAreRejected` | Pass |
| 23 | `ConnectionTest.MultiLegRouteViaBIsReturnedWhenNoDirectTrain` | Pass |
| 24 | `ConnectionTest.MaxLegsOneMeansDirectOnly` | Pass |
| 25 | `ConnectionTest.TooShortTransferWaitsForTheNextDaysRun` | Pass |
| 26 | `ConnectionTest.ConnectionIsDroppedWhenADirectTrainIsFaster` | Pass |
| 27 | `ConnectionTest.FasterConnectionIsListedBeforeASlowDirectTrain` | Pass |
| 28 | `ConnectionTest.ThreeLegJourneyNeedsMaxLegsThree` | Pass |
| 29 | `ConnectionTest.ChangeAtExactlyTheMinimumTransferTimeIsAllowed` | Pass |

### `rrs_fare_eco_tests`: Fare, eco-score & ranking (22 tests, all passed)

Source: `tests/test_fare_eco.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `FareTest.DirectSleeperNoConcessionMatchesHandComputedFare` | Pass |
| 2 | `FareTest.SeniorConcessionDiscountsTheDistanceCharge` | Pass |
| 3 | `FareTest.OtherConcessions` | Pass |
| 4 | `FareTest.ClassChangesTheRate` | Pass |
| 5 | `FareTest.EachLegOfAConnectionIsASeparateTicket` | Pass |
| 6 | `FareTest.RoundedToTheNearestRupee` | Pass |
| 7 | `FareTest.LongerTripCostsMore` | Pass |
| 8 | `FareTest.InvalidInputIsRejected` | Pass |
| 9 | `EcoTest.Co2AndScoreForADirectSleeperTrip` | Pass |
| 10 | `EcoTest.AcClassesEmitMorePerPassenger` | Pass |
| 11 | `EcoTest.EachChangeOfTrainAddsAPenalty` | Pass |
| 12 | `EcoTest.InvalidInputIsRejected` | Pass |
| 13 | `RankTest.SortByTimeFareAndEco` | Pass |
| 14 | `RankTest.EachOptionCarriesItsFareAndEcoScore` | Pass |
| 15 | `RankTest.ConcessionIsReflectedInRankedFares` | Pass |
| 16 | `RankTest.Filters` | Pass |
| 17 | `RankTest.FilterThatMatchesNothingGivesAnEmptyResult` | Pass |
| 18 | `RankTest.TiesKeepTheSearchOrder` | Pass |
| 19 | `RankTest.RankingDoesNotChangeTheInput` | Pass |
| 20 | `RankTest.FilterValuesOutsideTheirRangeAreRejected` | Pass |
| 21 | `RankTest.InvalidInputIsRejected` | Pass |
| 22 | `RankIntegrationTest.RanksTheResultOfARealSearch` | Pass |

### `rrs_search_perf_tests`: Search performance (NFR-001) (2 tests, all passed)

Source: `tests/test_search_perf.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `SearchPerformanceTest.DefaultSearchMeetsNfr001` | Pass |
| 2 | `SearchPerformanceTest.ThreeTrainSearchMeetsNfr001` | Pass |

### `rrs_booking_tests`: Booking & waitlist (20 tests, all passed)

Source: `tests/test_booking.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `BookingTest.CreateBookingSuccessfulPaymentGeneratesConfirmedPNR` | Pass |
| 2 | `BookingTest.MultipleBookingsGenerateDistinctPNRs` | Pass |
| 3 | `BookingTest.PaymentFailureRejectsBookingAndCreatesNoPNR` | Pass |
| 4 | `BookingTest.PaymentGatewayCallbackCanControlOutcome` | Pass |
| 5 | `BookingTest.RejectsInvalidInputs` | Pass |
| 6 | `BookingTest.RejectsNonBookableOrRetiredTrain` | Pass |
| 7 | `BookingTest.RejectsAdminSessionCallingPassengerBooking` | Pass |
| 8 | `BookingTest.SEC005NoSensitivePaymentDataInBookingRecord` | Pass |
| 9 | `BookingTest.IdempotentRetryReturnsExistingBookingWithoutDuplicate` | Pass |
| 10 | `BookingTest.EnqueuesWaitlistWhenClassFullAndAutoPromotesOnCancel` | Pass |
| 11 | `BookingTest.RejectsIdempotencyKeyExceedingMaxLength` | Pass |
| 12 | `BookingTest.WaitlistPromoteDoesNotOverAllocateWhenCapacityFull` | Pass |
| 13 | `BookingTest.WaitlistEnqueuePreservesFcfsOrder` | Pass |
| 14 | `BookingTest.CancelWaitlistedTicketRemovesFromQueueAndShiftsPositions` | Pass |
| 15 | `BookingTest.CancelTicketPermissionsAndIdempotency` | Pass |
| 16 | `BookingTest.PassengerBookingListRetrieval` | Pass |
| 17 | `BookingTest.CancelThenPromoteNeverExceedsCapacity` | Pass |
| 18 | `BookingTest.NFR009NoDoubleBookingUnder50ConcurrentRequestsForLastSeat` | Pass |
| 19 | `BookingTest.NFR009SameSessionConcurrentBookingsNoDataRace` | Pass |
| 20 | `BookingTest.NFR002BookingConfirmationTimeUnderNormalLoadMeets1sLimit` | Pass |

### `rrs_payment_retry_tests`: Payment retry (NFR-005) (6 tests, all passed)

Source: `tests/test_payment_retry.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `PaymentRetryTest.TimeoutBeforeChargeLeavesNoBookingThenRetrySucceedsOnce` | Pass |
| 2 | `PaymentRetryTest.RetryAfterSuccessfulChargeReturnsSamePnrWithoutSecondCharge` | Pass |
| 3 | `PaymentRetryTest.RepeatedTimeoutsThenSuccessThenRetriesStayAtOneBooking` | Pass |
| 4 | `PaymentRetryTest.RetryOfWaitlistedBookingDoesNotDuplicateWaitlistEntry` | Pass |
| 5 | `PaymentRetryTest.SameKeyFromDifferentPassengersDoesNotCollide` | Pass |
| 6 | `PaymentRetryTest.RetryAfterCancelDoesNotCreateNewActiveBooking` | Pass |

### `rrs_availability_tests`: Availability soak (NFR-003) (3 tests, all passed)

Source: `tests/test_availability.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `AvailabilityTest.ValidRequestsSucceedAtLeast99PercentOverASoakRun` | Pass |
| 2 | `AvailabilityTest.ServiceKeepsRunningAfterBadInputAndFailedPayment` | Pass |
| 3 | `AvailabilityTest.ServiceRecoversAfterTrainRetireAndRestock` | Pass |

### `rrs_scalability_tests`: Scalability (NFR-008) (3 tests, all passed)

Source: `tests/test_scalability.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `ScalabilityTest.FiveHundredTrainsAreStoredAndSearchable` | Pass |
| 2 | `ScalabilityTest.TenThousandBookingsAcrossFiveHundredTrains` | Pass |
| 3 | `ScalabilityTest.WaitlistHoldsTenThousandMinusOneForASingleSeat` | Pass |

### `rrs_security_validation_tests`: Security validation SEC-001..006 (9 tests, all passed)

Source: `tests/test_security_validation.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `SecurityValidation.Sec001PasswordsAreStoredAsSaltedHashesOnly` | Pass |
| 2 | `SecurityValidation.Sec002PassengerCannotCallAdminFunctions` | Pass |
| 3 | `SecurityValidation.Sec002PassengerCannotReadOrCancelAnotherPassengersBooking` | Pass |
| 4 | `SecurityValidation.Sec003SearchRejectsMalformedInput` | Pass |
| 5 | `SecurityValidation.Sec003BookingRejectsMalformedInput` | Pass |
| 6 | `SecurityValidation.Sec003AdminTrainInputIsValidated` | Pass |
| 7 | `SecurityValidation.Sec004ExpiredAndInvalidTokensAreRejectedEverywhere` | Pass |
| 8 | `SecurityValidation.Sec005OnlyAPaymentFlagIsKept` | Pass |
| 9 | `SecurityValidation.Sec006FailedLoginsAndAdminActionsAreAudited` | Pass |

### `rrs_facade_tests`: Facade (TUI back end) (6 tests, all passed)

Source: `tests/test_facade.cpp`

| # | Test | Result |
|---|---|---|
| 1 | `FacadeTest.SearchBookAndCancelEndToEnd` | Pass |
| 2 | `FacadeTest.FareComesFromFareCalculateNotFromTheCaller` | Pass |
| 3 | `FacadeTest.RetryWithSameKeyDoesNotDuplicate` | Pass |
| 4 | `FacadeTest.MultiLegBookingIsRolledBackWhenALaterLegFails` | Pass |
| 5 | `FacadeTest.FailedPaymentBooksNothing` | Pass |
| 6 | `FacadeTest.RejectsMissingOrBadSessionAndInput` | Pass |

