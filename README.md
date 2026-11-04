# CS4433 Moderated Learning Management System (Console)

Aligned with the submitted Phase 1 (management plan), Phase 2 (SRS), and Phase 3 (layered architecture), with a **completed design** and source for Phases 4–8. v1.1 implements CR1 (class-wide deadline extensions).

## Local build

Requirements: a C++17 compiler, CMake 3.16+, SQLite3 development headers, OpenSSL 1.1+ or 3 development headers. Supported reference build: Linux; Windows via MSYS2/MinGW or Visual Studio with vcpkg for SQLite3/OpenSSL.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/lms school.db --init-admin
./build/lms school.db
```

Enter UTC timestamps in `YYYY-MM-DD HH:MM:SS` format. Accounts are generated locally; there are **no default credentials**. Passwords use OpenSSL PBKDF2-HMAC-SHA256 (16-byte random salt, 310000 iterations, 32-byte output). Password entry is hidden in the interactive Windows/POSIX terminal; it is deliberately disabled for non-interactive stdin. Use the local console in a private session.

## Commands
- Administrator: `add-dept`, `rename-dept`, `del-dept`, `add-user`, `edit-user`, `deactivate-user`, `add-course`, `edit-course`, `del-course`, `assign-lecturer`, `enrol`.
- Lecturer: `add-assignment`, `extend-student`, `extend-class` (CR1), `submissions`, `mark`.
- Head of Department: `review`, `moderate`, `reject`, `release`.
- Student: `assignments`, `submit`, `results`.

Type `help` to see commands; `logout` to switch users; `exit` at login to leave. Numeric IDs appear when records are created. School technician should keep the database on a protected local drive and back it up with SQLite's online backup API or by shutting down the application before copying the database. It must **not** be placed on a shared network filesystem for simultaneous access; run the console on the one host, with separate accounts. Multiple independent users on a LAN would require a separate network service layer, not provided here.

## Testing
`tests/test_core.cpp` is a local CTest-based executable (15 test groups). `tests/test_catch2.cpp` is a separate Catch2 v3 suite (5 test cases) built when Catch2 is installed or fetched. For networked CI, pass `-DLMS_FETCH_CATCH2=ON`. Do not confuse the two: Catch2 tests are **not executed** in a local environment without Catch2 installed. Run coverage using a GCC coverage build and `gcov` or `gcovr`.

## Design files and documentation
- `diagrams/` – editable PlantUML sources and viewable diagram PNGs.
- `config/schema.sql` – SQLite v1.1 schema (v1.0 excludes the `class_extension` table).
- `docs/` – completed Phase 3, V-model test plan, release notes, manuals, CR1 and traceability.
- `evidence/` – local test runs, static analysis, coverage and code metrics.
- Local Git history has annotated `v1.0.0` (original workflow, 14 test groups) and `v1.1.0` (CR1, 15 test groups) tags.
- `.github/workflows/ci.yml` – GitHub Actions workflow (not a claim that CI was executed on GitHub).

## Limitations
This is a console proof-of-concept. There is no password reset UI, file upload, TLS-backed LAN server, automated 50-client capacity certification or public GitHub repository. Timestamp clock correctness and host file permissions are technician responsibilities. Release artifacts must be pushed to your hosted Git repository and reviewed before school deployment.
# CS4433---Moderated-LMS
