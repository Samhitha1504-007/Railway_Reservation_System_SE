# CI/CD Evidence

**Pipeline:** GitHub Actions, workflow [`.github/workflows/ci.yml`](../../../.github/workflows/ci.yml)  
**Runs on:** every push to `main` and every pull request into `main`  
**History captured:** 2026-09-29 to 2026-11-12 (all 73 runs)

## What each run does

| Step | Command | Fails the run when |
|---|---|---|
| Install | `apt-get install build-essential cmake libncurses-dev libssl-dev libgtest-dev cppcheck` | a package is missing |
| Configure and build | `cmake -S $SRC -B build -DCMAKE_BUILD_TYPE=Debug` and `cmake --build build --parallel` | any compile or link error |
| Unit tests | `ctest --test-dir build --output-on-failure --output-junit test-results.xml --no-tests=error` | any of the 266 tests fails, or no tests are found |
| Static analysis | `cppcheck --enable=warning,performance,portability --error-exitcode=1` | cppcheck reports a finding |
| Upload results | `actions/upload-artifact` (`test-results`) | never (always runs, keeps the JUnit XML) |

A newer push to the same branch or PR cancels a run still in progress (`concurrency: cancel-in-progress`), which is why a few runs show *cancelled*.

## Results

| Outcome | Runs |
|---|---|
| success | 64 |
| failure | 3 |
| cancelled | 6 |
| **total** | **73** |

**`main` has never had a failing run (25 passed, 6 cancelled, 0 failed).** The 3 failed runs were all on pull requests, and the pipeline stopped them from merging until they were fixed:

| Run | PR branch | What failed | Fixed by |
|---|---|---|---|
| #35, #36 | `feature/booking-create-pnr` | the first two pushes of the booking PR (#57) | commit `38adc2e` two minutes later; run #37 green |
| #70 | `feature/facade-tui` | cppcheck findings in the new TUI (uninitialised menu pointers, `memset` on a struct with doubles) | commit `b9d6cc6` on #77; run #71 green |

The 6 cancelled runs on `main` were replaced by the next run on `main` a few seconds later when several PRs merged together, and that next run passed.

## Latest run on `main`

Run #73 on `19a70a6` (Merge pull request #77 from Samhitha1504-007/feature/facade-tui): **success**, 2026-11-12 05:25:56 UTC. [Open run](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43161345054)

## All runs

| Run | Event | Branch | Commit | Result | Started (UTC) | Title |
|---|---|---|---|---|---|---|
| [#73](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43161345054) | push | `main` | `19a70a6` | success | 2026-11-12 05:25:56 | Merge pull request #77 from Samhitha1504-007/feature/facade-tui |
| [#72](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43161336546) | push | `main` | `5f2dd93` | cancelled | 2026-11-12 05:25:51 | Merge pull request #75 from Samhitha1504-007/test/nfr003-availability |
| [#71](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43153517304) | pull_request | `feature/facade-tui` | `b9d6cc6` | success | 2026-11-12 04:14:49 | tui: facade and ncurses front end for search, book and cancel (#35, #3 |
| [#70](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43152702787) | pull_request | `feature/facade-tui` | `6412c8a` | failure | 2026-11-12 04:07:28 | tui: facade and ncurses front end for search, book and cancel (#35, #3 |
| [#69](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43150537621) | pull_request | `test/nfr003-availability` | `8b14aaf` | success | 2026-11-12 03:47:33 | test: availability soak run (NFR-003, #38) |
| [#68](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43149033085) | push | `main` | `3159597` | success | 2026-11-12 03:33:43 | Merge pull request #76 from Samhitha1504-007/docs/nfr006-module-struc… |
| [#67](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43149026981) | push | `main` | `c073e4e` | cancelled | 2026-11-12 03:33:40 | Merge pull request #74 from Samhitha1504-007/demo/e2e-booking-flow |
| [#66](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43149019193) | push | `main` | `62cfd9f` | cancelled | 2026-11-12 03:33:36 | Merge pull request #73 from Samhitha1504-007/test/nfr005-payment-time… |
| [#65](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43149013285) | push | `main` | `9e21691` | cancelled | 2026-11-12 03:33:33 | Merge pull request #70 from Samhitha1504-007/feature/booking-waitlist… |
| [#64](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43147853463) | pull_request | `docs/nfr006-module-structure` | `8f94154` | success | 2026-11-12 03:23:14 | docs: module structure and unit-test map (NFR-006, #41) |
| [#63](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43147818119) | pull_request | `test/nfr003-availability` | `97fb0ec` | success | 2026-11-12 03:22:55 | test: availability soak run (NFR-003, #38) |
| [#62](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43147629060) | pull_request | `demo/e2e-booking-flow` | `10a0827` | success | 2026-11-12 03:21:14 | demo: end-to-end walkthrough from search to booking to cancel (#35) |
| [#61](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43147361890) | pull_request | `test/nfr005-payment-timeout-retry` | `25f0b9b` | success | 2026-11-12 03:18:55 | test: payment-timeout retry never duplicates a booking (NFR-005, #40) |
| [#60](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43019639631) | pull_request | `feature/booking-waitlist-cancel` | `a0ee968` | success | 2026-11-11 12:21:33 | booking: thread safety, concurrency guard, and performance tests (NFR- |
| [#59](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43001973809) | pull_request | `feature/booking-waitlist-cancel` | `85efc92` | success | 2026-11-11 10:14:44 | booking: thread safety, concurrency guard, and performance tests (NFR- |
| [#58](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/43001865640) | push | `main` | `3fdb9db` | success | 2026-11-11 10:13:58 | Merge pull request #72 from Samhitha1504-007/samhitha/nfr008-security… |
| [#57](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/42973948914) | pull_request | `samhitha/nfr008-security-validation` | `4d3d34f` | success | 2026-11-11 06:39:23 | NFR-008 scalability tests and SEC-001–SEC-006 security validation pass |
| [#56](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/42184539987) | push | `main` | `f78b489` | success | 2026-11-06 07:59:54 | Merge pull request #71 from Samhitha1504-007/feature/auth-sprint2-sec… |
| [#55](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/42171472413) | pull_request | `feature/auth-sprint2-security` | `ff2753c` | success | 2026-11-06 05:57:44 | Feature/auth sprint2 security |
| [#54](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41857141488) | pull_request | `feature/booking-waitlist-cancel` | `63a1aeb` | success | 2026-11-04 15:22:58 | booking: thread safety, concurrency guard, and performance tests (NFR- |
| [#53](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41591816066) | push | `main` | `afd6016` | success | 2026-11-03 06:44:40 | Merge pull request #58 from Samhitha1504-007/feature/booking-waitlist… |
| [#52](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41591805081) | push | `main` | `fa3c798` | cancelled | 2026-11-03 06:44:34 | Merge pull request #57 from Samhitha1504-007/feature/booking-create-pn |
| [#51](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41591034523) | push | `main` | `34b487b` | success | 2026-11-03 06:37:16 | Merge pull request #62 from Samhitha1504-007/samhitha/fare-eco-sort |
| [#50](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41588979033) | pull_request | `feature/infra-cmake-build` | `7622bf1` | success | 2026-11-03 06:17:45 | docs: document the CMake build in the README (#17) |
| [#49](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41588966015) | pull_request | `feature/infra-ci-pipeline` | `111b022` | success | 2026-11-03 06:17:36 | ci: always build and test, cancel superseded runs, keep test results … |
| [#48](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41583679032) | pull_request | `samhitha/fare-eco-sort` | `db3827f` | success | 2026-11-03 05:23:54 | search: fare, eco-score, sort/filter and NFR-001 performance test (FR- |
| [#47](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41319874179) | pull_request | `feature/booking-waitlist-cancel` | `cf12eec` | success | 2026-11-01 15:10:35 | booking: waitlist enqueue and ticket cancellation (FR-020, FR-021) |
| [#46](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41319578411) | pull_request | `feature/booking-create-pnr` | `2c71f1f` | success | 2026-11-01 15:07:14 | booking: create booking and PNR with payment validation (FR-018, FR-01 |
| [#45](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41318813297) | pull_request | `samhitha/fare-eco-sort` | `2eba7a4` | success | 2026-11-01 14:58:23 | search: fare, eco-score, sort/filter and NFR-001 performance test (FR- |
| [#44](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41317844285) | push | `main` | `65836e2` | success | 2026-11-01 14:46:29 | Merge pull request #61 from Samhitha1504-007/docs/sprint1-video |
| [#43](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41317760242) | pull_request | `docs/sprint1-video` | `81e570c` | success | 2026-11-01 14:45:29 | docs: add Sprint 1 demo video |
| [#42](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41315415623) | push | `main` | `89245f6` | success | 2026-11-01 14:17:02 | Merge pull request #60 from Samhitha1504-007/samhitha/demo-step-by-ste |
| [#41](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41315330005) | pull_request | `samhitha/demo-step-by-step` | `dda8afa` | success | 2026-11-01 14:16:01 | demo: step-by-step rrs_demo (pause and explain each step) |
| [#40](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41314405624) | push | `main` | `fbb38c5` | success | 2026-11-01 14:05:17 | Merge pull request #59 from Samhitha1504-007/samhitha/console-demo |
| [#39](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41314322553) | pull_request | `samhitha/console-demo` | `5237f0c` | success | 2026-11-01 14:04:18 | demo: console walkthrough of Sprint 1 features (rrs_demo) |
| [#38](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41307623569) | pull_request | `feature/booking-waitlist-cancel` | `f137f11` | success | 2026-11-01 12:40:38 | booking: waitlist enqueue and ticket cancellation (FR-020, FR-021) |
| [#37](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41303685344) | pull_request | `feature/booking-create-pnr` | `38adc2e` | success | 2026-11-01 11:50:41 | booking: create booking and PNR with payment validation (FR-018, FR-01 |
| [#36](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41303525928) | pull_request | `feature/booking-create-pnr` | `85035af` | failure | 2026-11-01 11:48:34 | booking: create booking and PNR with payment validation (FR-018, FR-01 |
| [#35](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41303399584) | pull_request | `feature/booking-create-pnr` | `1c19d6e` | failure | 2026-11-01 11:46:54 | booking: create booking and PNR with payment validation (FR-018, FR-01 |
| [#34](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272995021) | push | `main` | `0cc565b` | success | 2026-11-01 04:24:45 | Merge pull request #56 from Samhitha1504-007/feature/infra-cmake-build |
| [#33](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272991407) | push | `main` | `ad775e5` | cancelled | 2026-11-01 04:24:42 | Merge pull request #55 from Samhitha1504-007/feature/infra-ci-pipeline |
| [#32](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272948611) | push | `main` | `818f2aa` | success | 2026-11-01 04:24:05 | Merge pull request #54 from Samhitha1504-007/feature/inventory-capaci… |
| [#31](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272931492) | push | `main` | `76e0785` | success | 2026-11-01 04:23:49 | Merge pull request #53 from Samhitha1504-007/feature/inventory-retire… |
| [#30](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272922685) | push | `main` | `54d786e` | success | 2026-11-01 04:23:41 | Merge pull request #52 from Samhitha1504-007/feature/inventory-edit-t… |
| [#29](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272914658) | push | `main` | `68cacfb` | success | 2026-11-01 04:23:33 | Merge pull request #51 from Samhitha1504-007/feature/inventory-create… |
| [#28](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41272905144) | push | `main` | `8e2d06b` | success | 2026-11-01 04:23:23 | Merge pull request #50 from Samhitha1504-007/feature/inventory-route-… |
| [#27](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177410662) | pull_request | `feature/infra-cmake-build` | `b3af889` | success | 2026-10-31 06:38:01 | docs: document the CMake build |
| [#26](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177404080) | pull_request | `feature/infra-ci-pipeline` | `b63cbf4` | success | 2026-10-31 06:37:55 | ci: harden build-and-test workflow |
| [#25](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177396514) | pull_request | `feature/inventory-capacity-guard` | `e9ce510` | success | 2026-10-31 06:37:48 | inventory: capacity reduction guard (FR-011) |
| [#24](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177389391) | pull_request | `feature/inventory-retire-train` | `208e7d4` | success | 2026-10-31 06:37:41 | inventory: admin retire train (FR-009) |
| [#23](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177383241) | pull_request | `feature/inventory-edit-train` | `5645b11` | success | 2026-10-31 06:37:35 | inventory: admin edit train (FR-008) |
| [#22](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177375921) | pull_request | `feature/inventory-create-train` | `686df4a` | success | 2026-10-31 06:37:28 | inventory: admin create train (FR-007) |
| [#21](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41177367588) | pull_request | `feature/inventory-route-validation` | `852b53e` | success | 2026-10-31 06:37:20 | inventory: route connectivity validation (FR-010) |
| [#20](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41163822944) | push | `main` | `2c9d175` | success | 2026-10-31 03:18:47 | Merge pull request #49 from Samhitha1504-007/feature/auth-profile-ses… |
| [#19](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41163756056) | pull_request | `feature/auth-profile-session` | `0d57bab` | success | 2026-10-31 03:17:52 | Feature/auth profile session |
| [#18](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/41163615227) | push | `main` | `15ee465` | success | 2026-10-31 03:15:57 | Merge pull request #48 from Samhitha1504-007/samhitha/search-route-se… |
| [#17](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40430746487) | pull_request | `samhitha/search-route-search` | `b127109` | success | 2026-10-27 01:08:06 | search: direct and multi-leg route search (FR-012, FR-013) |
| [#16](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40382090363) | pull_request | `feature/auth-profile-session` | `75d5fc9` | success | 2026-10-26 18:55:39 | Feature/auth profile session |
| [#15](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40194839338) | pull_request | `samhitha/search-route-search` | `7525cf5` | success | 2026-10-25 12:49:38 | search: direct and multi-leg route search (FR-012, FR-013) |
| [#14](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40194089517) | push | `main` | `89ec2f2` | success | 2026-10-25 12:39:39 | Merge pull request #47 from Samhitha1504-007/claude/github-board-prio… |
| [#13](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40194009688) | pull_request | `claude/github-board-priority-column-9h621z` | `a57e682` | success | 2026-10-25 12:38:35 | ci: pass include dir to cppcheck; add contributing guide and board Pri |
| [#12](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40193517730) | pull_request | `claude/github-board-priority-column-9h621z` | `7b49511` | success | 2026-10-25 12:31:53 | ci: pass include dir to cppcheck; add contributing guide and board Pri |
| [#11](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40193381551) | push | `main` | `85053a9` | success | 2026-10-25 12:30:11 | Merge pull request #46 from Samhitha1504-007/feature/auth-rbac |
| [#10](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40193325198) | pull_request | `feature/auth-rbac` | `0e10bf3` | success | 2026-10-25 12:29:27 | Feature/auth rbac |
| [#9](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40193276905) | push | `main` | `45fa3bd` | success | 2026-10-25 12:28:45 | Merge pull request #45 from Samhitha1504-007/feature/auth-login |
| [#8](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40193095630) | pull_request | `feature/auth-login` | `d8bba85` | success | 2026-10-25 12:26:17 | auth: add login and session creation (FR-003) |
| [#7](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40192591795) | push | `main` | `7eb9b9b` | success | 2026-10-25 12:19:25 | Merge pull request #44 from Samhitha1504-007/feature/auth-registration |
| [#6](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40167284547) | pull_request | `feature/auth-registration` | `9083760` | success | 2026-10-25 05:55:39 | Feature/auth registration |
| [#5](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/40080076764) | pull_request | `feature/auth-registration` | `ac4e487` | success | 2026-10-24 08:57:56 | Feature/auth registration |
| [#4](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/38438654729) | push | `main` | `61c720d` | success | 2026-10-13 17:26:23 | ci: add GitHub Actions workflow and PR template |
| [#3](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/36826007922) | push | `main` | `eff53ee` | success | 2026-10-01 06:40:14 | Added SRS, SAD  and STP |
| [#2](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/36614730266) | push | `main` | `be804c0` | success | 2026-09-29 18:48:48 | Fix formatting in README.md for technology stack section |
| [#1](https://github.com/Samhitha1504-007/Railway_Reservation_System_SE/actions/runs/36528100428) | push | `main` | `d7af134` | success | 2026-09-29 05:50:47 | File directory structure created |
