PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS department (
 department_id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE CHECK(length(trim(name))>0)
);
CREATE TABLE IF NOT EXISTS users (
 user_id INTEGER PRIMARY KEY, username TEXT NOT NULL UNIQUE COLLATE NOCASE,
 password_hash TEXT NOT NULL, role TEXT NOT NULL CHECK(role IN ('Student','Lecturer','HoD','Administrator')),
 dept_id INTEGER REFERENCES department(department_id) ON DELETE RESTRICT,
 active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);
CREATE TABLE IF NOT EXISTS course (
 course_id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, title TEXT NOT NULL,
 dept_id INTEGER NOT NULL REFERENCES department(department_id) ON DELETE RESTRICT
);
CREATE TABLE IF NOT EXISTS course_lecturer (
 course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,
 lecturer_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,
 PRIMARY KEY(course_id,lecturer_id)
);
CREATE TABLE IF NOT EXISTS enrolment (
 course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,
 student_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,
 PRIMARY KEY(course_id,student_id)
);
CREATE TABLE IF NOT EXISTS assignment (
 assignment_id INTEGER PRIMARY KEY,course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,
 title TEXT NOT NULL,deadline INTEGER NOT NULL,max_mark REAL NOT NULL CHECK(max_mark>0),
 created_by INTEGER NOT NULL REFERENCES users(user_id)
);
CREATE TABLE IF NOT EXISTS extension (
 extension_id INTEGER PRIMARY KEY,assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id) ON DELETE CASCADE,
 student_id INTEGER NOT NULL REFERENCES users(user_id),new_deadline INTEGER NOT NULL,
 reason TEXT NOT NULL,granted_by INTEGER NOT NULL REFERENCES users(user_id),
 UNIQUE(assignment_id,student_id)
);
CREATE TABLE IF NOT EXISTS class_extension (
 assignment_id INTEGER PRIMARY KEY REFERENCES assignment(assignment_id) ON DELETE CASCADE,
 new_deadline INTEGER NOT NULL,reason TEXT NOT NULL,
 granted_by INTEGER NOT NULL REFERENCES users(user_id),updated_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS submission (
 submission_id INTEGER PRIMARY KEY,assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id) ON DELETE CASCADE,
 student_id INTEGER NOT NULL REFERENCES users(user_id),content TEXT NOT NULL,
 submitted_at INTEGER NOT NULL,status TEXT NOT NULL CHECK(status IN ('Submitted','Late','Marked','Moderated','Released','Rejected')),
 UNIQUE(assignment_id,student_id)
);
CREATE TABLE IF NOT EXISTS mark (
 mark_id INTEGER PRIMARY KEY,submission_id INTEGER NOT NULL UNIQUE REFERENCES submission(submission_id) ON DELETE CASCADE,
 raw_mark REAL NOT NULL CHECK(raw_mark>=0),final_mark REAL NOT NULL CHECK(final_mark>=0),
 feedback TEXT NOT NULL,marked_by INTEGER NOT NULL REFERENCES users(user_id),
 moderated_by INTEGER REFERENCES users(user_id)
);
CREATE TABLE IF NOT EXISTS audit_event (
 audit_id INTEGER PRIMARY KEY,mark_id INTEGER NOT NULL REFERENCES mark(mark_id) ON DELETE RESTRICT,
 actor_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,
 changed_at INTEGER NOT NULL,old_value REAL,new_value REAL NOT NULL,
 reason TEXT NOT NULL CHECK(length(trim(reason))>0)
);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);
CREATE INDEX IF NOT EXISTS idx_course_department ON course(dept_id);
CREATE INDEX IF NOT EXISTS idx_assignment_course ON assignment(course_id);
CREATE INDEX IF NOT EXISTS idx_submission_student ON submission(student_id);
CREATE INDEX IF NOT EXISTS idx_submission_assignment_status ON submission(assignment_id,status);
CREATE INDEX IF NOT EXISTS idx_audit_mark ON audit_event(mark_id);
