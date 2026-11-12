# Test Reports (Deliverables Part-3)

Reports for the Railway Reservation System, Team 5. All numbers come from one run on `main` on 12 Nov 2026, after PRs #70 to #77 were merged.

| Deliverable | File | Headline |
|---|---|---|
| Unit Test Report | [unit_test_report.md](unit_test_report.md) | 266 tests in 12 binaries, all passed |
| System Validation Report | [system_validation_report.md](system_validation_report.md) | 39 requirements validated; 9 TUI system tests (8 pass, 1 defect found: D-01) |
| Code Coverage Report | [code_coverage_report.md](code_coverage_report.md) | Engine: 95.0% lines, 99.2% functions; every module at 87% or more |
| Branch Coverage Report | [branch_coverage_report.md](branch_coverage_report.md) | Engine: 86.1% branches |
| Coverage, line by line (HTML) | [coverage/index.html](coverage/index.html) | Open in a browser |
| Raw test results (JUnit XML) | [test-results/](test-results/) | One file per test binary |
| Design deviations | [design_deviations.md](design_deviations.md) | API, persistence, audit and TUI changes compared with the Design document |

Related files elsewhere in the repository:

| What | Where |
|---|---|
| Test Case Document (updated test cases, traceability matrix, every automated test) | [`RRS/04_IMPLEMENTATION_TESTING/test-cases.xlsx`](../RRS/04_IMPLEMENTATION_TESTING/test-cases.xlsx) |
| Security validation (SEC-001 to SEC-006) | [`RRS/04_IMPLEMENTATION_TESTING/security-validation/`](../RRS/04_IMPLEMENTATION_TESTING/security-validation/security_validation_report.md) |
| CI/CD evidence (all 73 pipeline runs) | [`RRS/04_IMPLEMENTATION_TESTING/CI-CD-evidence/`](../RRS/04_IMPLEMENTATION_TESTING/CI-CD-evidence/ci_run_history.md) |
| TUI screenshots (system tests) | [`RRS/05_FINAL_REPORT/screenshots/`](../RRS/05_FINAL_REPORT/screenshots/) |
| Sprint 2/3 demo video (TUI walkthrough) | [`RRS/04_IMPLEMENTATION_TESTING/demo-videos/sprint2_tui_demo.mp4`](../RRS/04_IMPLEMENTATION_TESTING/demo-videos/sprint2_tui_demo.mp4) |

## How to re-run

```bash
cd RRS/04_IMPLEMENTATION_TESTING/source-code
cmake -S . -B build && cmake --build build --parallel
ctest --test-dir build --output-on-failure      # all tests, as CI runs them
./tools/coverage.sh                             # coverage build + docs/coverage (needs: pip install gcovr)
./build/rrs_app                                 # the TUI used for the system tests
```
