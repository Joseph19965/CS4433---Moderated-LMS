# Conclusion

The project demonstrates a coherent moderated assessment lifecycle rather than treating marks as immediately public values. The user's original Waterfall planning, conflict resolutions and layered console decision remain the foundation. The completed Phase 3 diagrams clarify responsibilities and data relationships, and the source implements C++17 polymorphic user roles, grade penalties and extensions, mark moderation, audit and SQLite persistence. Phase 8's class-wide extension is integrated into the same effective-deadline computation used by student lists, submissions and penalties, avoiding inconsistent policy paths. Testable code and prepared CI files help show how design artifacts map to evidence.

The local run provides 15 passing test groups and instrumented business-source line coverage above the stated 70% target, with qualifications about line coverage versus branch coverage and unavailable Catch2 execution. The release documentation, class-extension regression and updated risk assessment complete the intended academic workflow in prototype form. The greatest outstanding risks are operational rather than algorithmic: trusted submission time, restricted local database access, real offline usability and capacity testing, hosted Git/CI evidence, and independent acceptance by school stakeholders. These gaps must be resolved or formally accepted before an actual school relies on this prototype for minors' assessment records.

# Appendices

## Appendix A — Traceability matrix

The complete functional and non-functional matrix, including Must/Should priorities, use cases, design operations, tables, source paths, test IDs and honest pass/remaining-work status, is supplied separately as `docs/traceability.csv`. It contains FR-01 through FR-19 (including CR1), and NFR-01 through NFR-08. This CSV can be opened and filtered using a spreadsheet tool without retyping a wide table from the report.

## Appendix B — Complete executable design and SQL

UML sources and PNG previews are in `diagrams/`: component, class, submission state machine, swimlane activity, Submit sequence, HoD Moderate/Release sequence, ERD and deployment diagram. The original WBS, Gantt and use case image are also provided, with the original DOCX source documents in `original_phases/`. The SQLite `CREATE TABLE` definitions and indexes are in `config/schema.sql`, and the actual executable schema is in `Database::schema()` in `src/lms.cpp`.

## Appendix C — Testing, evidence and outstanding acceptance

The CTest-based group suite is `tests/test_core.cpp`; the optional Catch2 v3 suite is `tests/test_catch2.cpp`. The local CTest logs and analyser/coverage measurements are in `evidence/`. The complete test-case and defect summary is given in the Phase 5 section of this report. No hosted CI screenshot, teacher sign-off, 50-session benchmark or public repository link is asserted. To produce them, push this project to a controlled Git repository, run the GitHub Actions workflow and record observed school trial results, including time measurements.

## Appendix D — Updated risk register

| Risk | Original exposure | Phase-gate update | Residual risk / action |
|:--|--:|:--|:--|
| R1 scope creep | 16 | Change managed via CR1 | Medium; lock v1.1 scope |
| R2 moderation complexity | 12 | State machine/tests implemented | Medium; test school exceptions |
| R3 800-user SQLite performance | 8 | Indexed SQL design | High until actual benchmark |
| R4 password storage | 15 | Salted OpenSSL PBKDF2 | Medium; host access and password policy |
| R5 developer unavailability | 15 | Source/docs/tests packaged | Medium; store in Git remote |
| R6 penalty/extension interpretation | 12 | Boundary/precedence tests | Low for tested values |
| R7 untrusted caller-controlled submission clock | New, high | Documented; no server clock yet | High; change API to trusted timestamp |
| R8 50-concurrent-session requirement | New, high | Architecture mismatch disclosed | High; establish capacity architecture |
| R9 database access beyond application | New, high | Host restrictions prescribed | High until secure deployment |

## Appendix E — Release checklist

- [x] C++17 executable built locally.
- [x] SQLite schema and four-role access restrictions implemented.
- [x] Phase 3 component, class, behavioural, ERD and SQL design files supplied.
- [x] CR1 v1.1 class-wide deadline implemented and regression tests added.
- [x] Local built-in automated tests passed (15/15 groups).
- [x] Local Clang static-analyser invocation completed with no emitted diagnostics.
- [x] Local physical-line gcov measurement recorded with limitations disclosed.
- [ ] Catch2 v3 tests executed in a connected runner.
- [x] Local Git commits and annotated v1.0.0/v1.1.0 tags exist.
- [ ] Repository pushed to a host and independent PR review recorded.
- [ ] 800-user and 50-concurrent-session benchmarks completed.
- [ ] Actual school acceptance and accessibility walkthrough completed.
- [ ] Production-grade trusted submission timestamp and database-access hardening completed.
