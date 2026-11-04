#include "lms/lms.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>
#include <set>
#include <regex>
namespace lms
{
    namespace
    {
        constexpr int HASH_ITERATIONS=310000;
        std::string hex(const unsigned char* p,size_t n)
        {
            static const char* digits="0123456789abcdef";
            std::string out;
            out.reserve(n*2);
            for(size_t i=0;i<n;++i)
            {
                out+=digits[p[i]>>4];
                out+=digits[p[i]&15];
            }
            return out;
        }
        std::vector<unsigned char> unhex(const std::string& h)
        {
            if(h.size()%2)throw Error("Invalid stored hash");
            std::vector<unsigned char> out;
            for(size_t i=0;i<h.size();i+=2)
            {
                unsigned x;
                std::istringstream s(h.substr(i,2));
                if(!(s>>std::hex>>x))throw Error("Invalid stored hash");
                out.push_back(static_cast<unsigned char>(x));
            }
            return out;
        }
        std::string hashPassword(const std::string& password)
        {
            if(password.size()<10)throw Error("Password needs at least 10 characters");
            unsigned char salt[16],key[32];
            if(RAND_bytes(salt,sizeof salt)!=1||PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),salt,sizeof salt,HASH_ITERATIONS,EVP_sha256(),sizeof key,key)!=1)throw Error("Password hashing failed");
            return "pbkdf2-sha256$"+std::to_string(HASH_ITERATIONS)+"$"+hex(salt,sizeof salt)+"$"+hex(key,sizeof key);
        }
        bool verifyPassword(const std::string& password,const std::string& stored)
        {
            try
            {
                std::istringstream s(stored);
                std::string alg,rounds,sa,ka;
                std::getline(s,alg,'$');
                std::getline(s,rounds,'$');
                std::getline(s,sa,'$');
                std::getline(s,ka,'$');
                if(alg!="pbkdf2-sha256")return false;
                int n=std::stoi(rounds);
                if(n<100000||n>2000000)return false;
                auto salt=unhex(sa),expected=unhex(ka);
                if(salt.size()!=16||expected.size()!=32)return false;
                unsigned char computed[32];
                if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),salt.data(),static_cast<int>(salt.size()),n,EVP_sha256(),sizeof computed,computed)!=1)return false;
                return CRYPTO_memcmp(computed,expected.data(),sizeof computed)==0;
            }
            catch(...)
            {
                return false;
            }
        }
        void nonempty(const std::string& s,const std::string& field)
        {
            if(s.empty()||s.find_first_not_of(" \t\r\n")==std::string::npos)throw Error(field+" must not be empty");
        }
        void validRole(const std::string& role)
        {
            if(role!="Student"&&role!="Lecturer"&&role!="HoD"&&role!="Administrator")throw Error("Unknown role");
        }
        long long nowEpoch()
        {
            return static_cast<long long>(std::time(nullptr));
        }
        void affected(sqlite3* db,const std::string& label)
        {
            if(sqlite3_changes(db)!=1)throw Error(label+": record not found");
        }
        class Transaction
        {
            Database& db_;
            bool completed_=false;
            public:explicit Transaction(Database& db):db_(db)
            {
                db_.exec("BEGIN IMMEDIATE");
            }
            void commit()
            {
                db_.exec("COMMIT");
                completed_=true;
            }
            ~Transaction()
            {
                if(!completed_)
                {
                    try
                    {
                        db_.exec("ROLLBACK");
                    }
                    catch(...)
                    {
                    }
                }
            }
        }
        ;
    }
    // namespace
    Statement::Statement(sqlite3* db,const std::string& sql)
    {
        if(sqlite3_prepare_v2(db,sql.c_str(),-1,&st_,nullptr)!=SQLITE_OK)throw Error(sqlite3_errmsg(db));
    }
    Statement::~Statement()
    {
        sqlite3_finalize(st_);
    }
    void Statement::bind(int i,int v)
    {
        if(sqlite3_bind_int(st_,i,v)!=SQLITE_OK)throw Error("SQL bind error");
    }
    void Statement::bind(int i,long long v)
    {
        if(sqlite3_bind_int64(st_,i,v)!=SQLITE_OK)throw Error("SQL bind error");
    }
    void Statement::bind(int i,double v)
    {
        if(!std::isfinite(v)||sqlite3_bind_double(st_,i,v)!=SQLITE_OK)throw Error("Invalid numeric value");
    }
    void Statement::bind(int i,const std::string& v)
    {
        if(sqlite3_bind_text(st_,i,v.c_str(),static_cast<int>(v.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw Error("SQL bind error");
    }
    void Statement::null(int i)
    {
        if(sqlite3_bind_null(st_,i)!=SQLITE_OK)throw Error("SQL bind error");
    }
    bool Statement::next()
    {
        int rc=sqlite3_step(st_);
        if(rc==SQLITE_ROW)return true;
        if(rc==SQLITE_DONE)return false;
        throw Error(sqlite3_errmsg(sqlite3_db_handle(st_)));
    }
    void Statement::execute()
    {
        if(next())throw Error("Unexpected result rows");
    }
    int Statement::integer(int i)const
    {
        return sqlite3_column_int(st_,i);
    }
    long long Statement::integer64(int i)const
    {
        return sqlite3_column_int64(st_,i);
    }
    double Statement::real(int i)const
    {
        return sqlite3_column_double(st_,i);
    }
    std::string Statement::str(int i)const
    {
        auto* p=sqlite3_column_text(st_,i);
        return p?reinterpret_cast<const char*>(p):"";
    }
    bool Statement::isNull(int i)const
    {
        return sqlite3_column_type(st_,i)==SQLITE_NULL;
    }
    Database::Database(const std::string& path)
    {
        if(sqlite3_open_v2(path.c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr)!=SQLITE_OK)
        {
            std::string message=db_?sqlite3_errmsg(db_):"DB open failed";
            if(db_)sqlite3_close(db_);
            db_=nullptr;
            throw Error(message);
        }
        sqlite3_busy_timeout(db_,5000);
        exec("PRAGMA foreign_keys=ON");
        if(path!=":memory:")exec("PRAGMA journal_mode=WAL");
        schema();
    }
    Database::~Database()
    {
        if(db_)sqlite3_close(db_);
    }
    void Database::exec(const std::string& sql)
    {
        char* err=nullptr;
        int rc=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&err);
        if(rc!=SQLITE_OK)
        {
            std::string msg=err?err:"SQL error";
            sqlite3_free(err);
            throw Error(msg);
        }
    }
    long long Database::lastId()const
    {
        return sqlite3_last_insert_rowid(db_);
    }
    void Database::schema()
    {
        // Version 1.1 compatible schema. See config/schema.sql for deployment SQL and constraints.
        exec(R"SQL(
CREATE TABLE IF NOT EXISTS department(department_id INTEGER PRIMARY KEY,name TEXT NOT NULL UNIQUE CHECK(length(trim(name))>0));
CREATE TABLE IF NOT EXISTS users(user_id INTEGER PRIMARY KEY,username TEXT NOT NULL UNIQUE COLLATE NOCASE,password_hash TEXT NOT NULL,role TEXT NOT NULL CHECK(role IN ('Student','Lecturer','HoD','Administrator')),dept_id INTEGER REFERENCES department(department_id) ON DELETE RESTRICT,active INTEGER NOT NULL DEFAULT 1 CHECK(active IN(0,1)));
CREATE TABLE IF NOT EXISTS course(course_id INTEGER PRIMARY KEY,code TEXT NOT NULL UNIQUE,title TEXT NOT NULL,dept_id INTEGER NOT NULL REFERENCES department(department_id) ON DELETE RESTRICT);
CREATE TABLE IF NOT EXISTS course_lecturer(course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,lecturer_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,PRIMARY KEY(course_id,lecturer_id));
CREATE TABLE IF NOT EXISTS enrolment(course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,student_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,PRIMARY KEY(course_id,student_id));
CREATE TABLE IF NOT EXISTS assignment(assignment_id INTEGER PRIMARY KEY,course_id INTEGER NOT NULL REFERENCES course(course_id) ON DELETE CASCADE,title TEXT NOT NULL,deadline INTEGER NOT NULL,max_mark REAL NOT NULL CHECK(max_mark>0),created_by INTEGER NOT NULL REFERENCES users(user_id));
CREATE TABLE IF NOT EXISTS extension(extension_id INTEGER PRIMARY KEY,assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id) ON DELETE CASCADE,student_id INTEGER NOT NULL REFERENCES users(user_id),new_deadline INTEGER NOT NULL,reason TEXT NOT NULL,granted_by INTEGER NOT NULL REFERENCES users(user_id),UNIQUE(assignment_id,student_id));
CREATE TABLE IF NOT EXISTS class_extension(assignment_id INTEGER PRIMARY KEY REFERENCES assignment(assignment_id) ON DELETE CASCADE,new_deadline INTEGER NOT NULL,reason TEXT NOT NULL,granted_by INTEGER NOT NULL REFERENCES users(user_id),updated_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS submission(submission_id INTEGER PRIMARY KEY,assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id) ON DELETE CASCADE,student_id INTEGER NOT NULL REFERENCES users(user_id),content TEXT NOT NULL,submitted_at INTEGER NOT NULL,status TEXT NOT NULL CHECK(status IN ('Submitted','Late','Marked','Moderated','Released','Rejected')),UNIQUE(assignment_id,student_id));
CREATE TABLE IF NOT EXISTS mark(mark_id INTEGER PRIMARY KEY,submission_id INTEGER NOT NULL UNIQUE REFERENCES submission(submission_id) ON DELETE CASCADE,raw_mark REAL NOT NULL CHECK(raw_mark>=0),final_mark REAL NOT NULL CHECK(final_mark>=0),feedback TEXT NOT NULL,marked_by INTEGER NOT NULL REFERENCES users(user_id),moderated_by INTEGER REFERENCES users(user_id));
CREATE TABLE IF NOT EXISTS audit_event(audit_id INTEGER PRIMARY KEY,mark_id INTEGER NOT NULL REFERENCES mark(mark_id) ON DELETE RESTRICT,actor_id INTEGER NOT NULL REFERENCES users(user_id),changed_at INTEGER NOT NULL,old_value REAL,new_value REAL NOT NULL,reason TEXT NOT NULL CHECK(length(trim(reason))>0));
CREATE INDEX IF NOT EXISTS idx_course_department ON course(dept_id);
CREATE INDEX IF NOT EXISTS idx_assignment_course ON assignment(course_id);
CREATE INDEX IF NOT EXISTS idx_submission_student ON submission(student_id);
CREATE INDEX IF NOT EXISTS idx_submission_assignment_status ON submission(assignment_id,status);
CREATE INDEX IF NOT EXISTS idx_audit_mark ON audit_event(mark_id);
)SQL");
    }
    std::unique_ptr<User> UserFactory::create(int id,const std::string& name,const std::string& role)
    {
        if(role=="Student")return std::make_unique<Student>(id,name);
        if(role=="Lecturer")return std::make_unique<Lecturer>(id,name);
        if(role=="HoD")return std::make_unique<HoD>(id,name);
        if(role=="Administrator")return std::make_unique<Administrator>(id,name);
        throw Error("Unknown user type");
    }
    double StandardLatePenalty::finalMark(double raw,double maximum,long long secondsLate)const
    {
        if(!std::isfinite(raw)||!std::isfinite(maximum)||maximum<=0||raw<0||raw>maximum)throw Error("Mark outside valid range");
        if(secondsLate<=0)return raw;
        long long days=(secondsLate+86399)/86400;
        if(days>3)return 0;
        return std::max(0.0,raw-maximum*0.10*days);
    }
    void AuditService::onMarkChange(const MarkChange& e)
    {
        Statement s(db_.handle(),"INSERT INTO audit_event(mark_id,actor_id,changed_at,old_value,new_value,reason) VALUES(?,?,?,?,?,?)");
        s.bind(1,e.markId);
        s.bind(2,e.actor);
        s.bind(3,e.when);
        if(e.oldValue)s.bind(4,*e.oldValue);
        else s.null(4);
        s.bind(5,e.newValue);
        s.bind(6,e.reason);
        s.execute();
    }
    void NotificationService::onMarkChange(const MarkChange& e)
    {
        messages_.push_back("Mark "+std::to_string(e.markId)+" changed (internal event; not student-visible)");
    }
    std::optional<SubmissionView> SubmissionRepository::get(int assignment,int student)const
    {
        Statement s(db_.handle(),"SELECT submission_id,assignment_id,student_id,content,status,submitted_at FROM submission WHERE assignment_id=? AND student_id=?");
        s.bind(1,assignment);
        s.bind(2,student);
        if(!s.next())return std::nullopt;
        return SubmissionView
        {
            s.integer(0),s.integer(1),s.integer(2),s.str(3),s.str(4),s.integer64(5)
        }
        ;
    }
    void SubmissionRepository::insert(int assignment,int student,const std::string& content,long long time,const std::string& status)
    {
        Statement s(db_.handle(),"INSERT INTO submission(assignment_id,student_id,content,submitted_at,status) VALUES(?,?,?,?,?)");
        s.bind(1,assignment);
        s.bind(2,student);
        s.bind(3,content);
        s.bind(4,time);
        s.bind(5,status);
        s.execute();
    }
    void SubmissionRepository::replace(int id,const std::string& content,long long time,const std::string& status)
    {
        Statement s(db_.handle(),"UPDATE submission SET content=?,submitted_at=?,status=? WHERE submission_id=?");
        s.bind(1,content);
        s.bind(2,time);
        s.bind(3,status);
        s.bind(4,id);
        s.execute();
        affected(db_.handle(),"Update submission");
    }
    LmsService::LmsService(const std::string& path):db_(path),audit_(db_),submissions_(db_)
    {
    }
    void LmsService::require(int user,const std::string& role)const
    {
        Statement s(db_.handle(),"SELECT role,active FROM users WHERE user_id=?");
        s.bind(1,user);
        if(!s.next()||s.integer(1)!=1||s.str(0)!=role)throw Error("Permission denied: "+role+" required");
    }
    std::unique_ptr<User> LmsService::user(int id)const
    {
        Statement s(db_.handle(),"SELECT username,role FROM users WHERE user_id=? AND active=1");
        s.bind(1,id);
        if(!s.next())throw Error("Inactive/unknown user");
        return UserFactory::create(id,s.str(0),s.str(1));
    }
    int LmsService::bootstrapAdmin(const std::string& username,const std::string& password)
    {
        nonempty(username,"Username");
        Statement q(db_.handle(),"SELECT COUNT(*) FROM users");
        q.next();
        if(q.integer(0)!=0)throw Error("Bootstrap only works with an empty users table");
        Statement s(db_.handle(),"INSERT INTO users(username,password_hash,role) VALUES(?,?,'Administrator')");
        s.bind(1,username);
        s.bind(2,hashPassword(password));
        s.execute();
        return static_cast<int>(db_.lastId());
    }
    int LmsService::login(const std::string& username,const std::string& password)const
    {
        Statement s(db_.handle(),"SELECT user_id,password_hash FROM users WHERE username=? AND active=1");
        s.bind(1,username);
        if(!s.next())throw Error("Invalid credentials");
        int id=s.integer(0);
        if(!verifyPassword(password,s.str(1)))throw Error("Invalid credentials");
        return id;
    }
    int LmsService::addUser(int admin,const std::string& name,const std::string& password,const std::string& role,int department)
    {
        require(admin,"Administrator");
        nonempty(name,"Username");
        validRole(role);
        if(role=="HoD"&&department<=0)throw Error("HoD needs a department");
        Statement s(db_.handle(),"INSERT INTO users(username,password_hash,role,dept_id) VALUES(?,?,?,?)");
        s.bind(1,name);
        s.bind(2,hashPassword(password));
        s.bind(3,role);
        if(department)s.bind(4,department);
        else s.null(4);
        s.execute();
        return static_cast<int>(db_.lastId());
    }
    void LmsService::updateUser(int admin,int target,const std::string& name,const std::string& role,int dept)
    {
        require(admin,"Administrator");
        nonempty(name,"Username");
        validRole(role);
        if(role=="HoD"&&dept==0)throw Error("HoD needs department");
        Statement s(db_.handle(),"UPDATE users SET username=?,role=?,dept_id=? WHERE user_id=? AND active=1");
        s.bind(1,name);
        s.bind(2,role);
        if(dept)s.bind(3,dept);
        else s.null(3);
        s.bind(4,target);
        s.execute();
        affected(db_.handle(),"Update user");
    }
    void LmsService::deactivateUser(int admin,int target)
    {
        require(admin,"Administrator");
        if(admin==target)throw Error("Cannot deactivate own administrator account");
        Statement s(db_.handle(),"UPDATE users SET active=0 WHERE user_id=? AND active=1");
        s.bind(1,target);
        s.execute();
        affected(db_.handle(),"Deactivate user");
    }
    int LmsService::addDepartment(int admin,const std::string& name)
    {
        require(admin,"Administrator");
        nonempty(name,"Department name");
        Statement s(db_.handle(),"INSERT INTO department(name) VALUES(?)");
        s.bind(1,name);
        s.execute();
        return static_cast<int>(db_.lastId());
    }
    void LmsService::renameDepartment(int admin,int id,const std::string& name)
    {
        require(admin,"Administrator");
        nonempty(name,"Department");
        Statement s(db_.handle(),"UPDATE department SET name=? WHERE department_id=?");
        s.bind(1,name);
        s.bind(2,id);
        s.execute();
        affected(db_.handle(),"Rename department");
    }
    void LmsService::deleteDepartment(int admin,int id)
    {
        require(admin,"Administrator");
        Statement s(db_.handle(),"DELETE FROM department WHERE department_id=?");
        s.bind(1,id);
        s.execute();
        affected(db_.handle(),"Delete department");
    }
    int LmsService::addCourse(int admin,int dept,const std::string& code,const std::string& title)
    {
        require(admin,"Administrator");
        nonempty(code,"Course code");
        nonempty(title,"Course title");
        Statement s(db_.handle(),"INSERT INTO course(code,title,dept_id) VALUES(?,?,?)");
        s.bind(1,code);
        s.bind(2,title);
        s.bind(3,dept);
        s.execute();
        return static_cast<int>(db_.lastId());
    }
    void LmsService::editCourse(int admin,int id,const std::string& code,const std::string& title)
    {
        require(admin,"Administrator");
        nonempty(code,"Course code");
        nonempty(title,"Course title");
        Statement s(db_.handle(),"UPDATE course SET code=?,title=? WHERE course_id=?");
        s.bind(1,code);
        s.bind(2,title);
        s.bind(3,id);
        s.execute();
        affected(db_.handle(),"Edit course");
    }
    void LmsService::deleteCourse(int admin,int id)
    {
        require(admin,"Administrator");
        Statement s(db_.handle(),"DELETE FROM course WHERE course_id=?");
        s.bind(1,id);
        s.execute();
        affected(db_.handle(),"Delete course");
    }
    void LmsService::assignLecturer(int admin,int course,int lecturer)
    {
        require(admin,"Administrator");
        require(lecturer,"Lecturer");
        Statement s(db_.handle(),"INSERT OR IGNORE INTO course_lecturer(course_id,lecturer_id) VALUES(?,?)");
        s.bind(1,course);
        s.bind(2,lecturer);
        s.execute();
    }
    void LmsService::enrol(int admin,int course,int student)
    {
        require(admin,"Administrator");
        require(student,"Student");
        Statement s(db_.handle(),"INSERT OR IGNORE INTO enrolment(course_id,student_id) VALUES(?,?)");
        s.bind(1,course);
        s.bind(2,student);
        s.execute();
    }
    void LmsService::requireCourseLecturer(int lecturer,int course)const
    {
        require(lecturer,"Lecturer");
        Statement q(db_.handle(),"SELECT 1 FROM course_lecturer WHERE course_id=? AND lecturer_id=?");
        q.bind(1,course);
        q.bind(2,lecturer);
        if(!q.next())throw Error("Lecturer not assigned to course");
    }
    void LmsService::requireDepartmentHoD(int hod,int course)const
    {
        require(hod,"HoD");
        Statement q(db_.handle(),"SELECT 1 FROM users u JOIN course c ON c.dept_id=u.dept_id WHERE u.user_id=? AND c.course_id=?");
        q.bind(1,hod);
        q.bind(2,course);
        if(!q.next())throw Error("HoD cannot access another department");
    }
    int LmsService::addAssignment(int lecturer,int course,const std::string& title,long long deadline,double max)
    {
        requireCourseLecturer(lecturer,course);
        nonempty(title,"Assignment title");
        if(deadline<=0||max<=0||!std::isfinite(max))throw Error("Invalid deadline or maximum mark");
        Statement s(db_.handle(),"INSERT INTO assignment(course_id,title,deadline,max_mark,created_by) VALUES(?,?,?,?,?)");
        s.bind(1,course);
        s.bind(2,title);
        s.bind(3,deadline);
        s.bind(4,max);
        s.bind(5,lecturer);
        s.execute();
        return static_cast<int>(db_.lastId());
    }
    int LmsService::courseForAssignment(int assignment)const
    {
        Statement s(db_.handle(),"SELECT course_id FROM assignment WHERE assignment_id=?");
        s.bind(1,assignment);
        if(!s.next())throw Error("Assignment not found");
        return s.integer(0);
    }
    long long LmsService::assignmentDeadline(int assignment)const
    {
        Statement s(db_.handle(),"SELECT deadline FROM assignment WHERE assignment_id=?");
        s.bind(1,assignment);
        if(!s.next())throw Error("Assignment not found");
        return s.integer64(0);
    }
    double LmsService::maximumForAssignment(int assignment)const
    {
        Statement s(db_.handle(),"SELECT max_mark FROM assignment WHERE assignment_id=?");
        s.bind(1,assignment);
        if(!s.next())throw Error("Assignment not found");
        return s.real(0);
    }
    void LmsService::individualExtension(int lecturer,int assignment,int student,long long deadline,const std::string& reason)
    {
        requireCourseLecturer(lecturer,courseForAssignment(assignment));
        require(student,"Student");
        nonempty(reason,"Reason");
        if(deadline<=assignmentDeadline(assignment))throw Error("Extension must exceed original deadline");
        Statement enrolment(db_.handle(),"SELECT 1 FROM enrolment WHERE course_id=? AND student_id=?");
        enrolment.bind(1,courseForAssignment(assignment));
        enrolment.bind(2,student);
        if(!enrolment.next())throw Error("Student not enrolled");
        Statement s(db_.handle(),"INSERT INTO extension(assignment_id,student_id,new_deadline,reason,granted_by) VALUES(?,?,?,?,?) ON CONFLICT(assignment_id,student_id) DO UPDATE SET new_deadline=excluded.new_deadline,reason=excluded.reason,granted_by=excluded.granted_by");
        s.bind(1,assignment);
        s.bind(2,student);
        s.bind(3,deadline);
        s.bind(4,reason);
        s.bind(5,lecturer);
        s.execute();
    }
    void LmsService::classExtension(int lecturer,int assignment,long long deadline,const std::string& reason)
    {
        requireCourseLecturer(lecturer,courseForAssignment(assignment));
        nonempty(reason,"Reason");
        if(deadline<=assignmentDeadline(assignment))throw Error("Class extension must exceed original deadline");
        Statement s(db_.handle(),"INSERT INTO class_extension(assignment_id,new_deadline,reason,granted_by,updated_at) VALUES(?,?,?,?,?) ON CONFLICT(assignment_id) DO UPDATE SET new_deadline=excluded.new_deadline,reason=excluded.reason,granted_by=excluded.granted_by,updated_at=excluded.updated_at");
        s.bind(1,assignment);
        s.bind(2,deadline);
        s.bind(3,reason);
        s.bind(4,lecturer);
        s.bind(5,nowEpoch());
        s.execute();
    }
    long long LmsService::userDeadline(int assignment,int student,std::string* source)const
    {
        long long deadline=assignmentDeadline(assignment);
        std::string origin="original";
        Statement c(db_.handle(),"SELECT new_deadline FROM class_extension WHERE assignment_id=?");
        c.bind(1,assignment);
        if(c.next()&&c.integer64(0)>deadline)
        {
            deadline=c.integer64(0);
            origin="class";
        }
        Statement i(db_.handle(),"SELECT new_deadline FROM extension WHERE assignment_id=? AND student_id=?");
        i.bind(1,assignment);
        i.bind(2,student);
        if(i.next()&&i.integer64(0)>deadline)
        {
            deadline=i.integer64(0);
            origin="individual";
        }
        if(source)*source=origin;
        return deadline;
    }
    std::vector<AssignmentView> LmsService::studentAssignments(int student)const
    {
        require(student,"Student");
        Statement s(db_.handle(),"SELECT a.assignment_id,a.course_id,a.title,a.deadline,a.max_mark FROM assignment a JOIN enrolment e ON e.course_id=a.course_id WHERE e.student_id=? ORDER BY a.deadline,a.assignment_id");
        s.bind(1,student);
        std::vector<AssignmentView> out;
        while(s.next())
        {
            AssignmentView a
            {
                s.integer(0),s.integer(1),s.str(2),s.integer64(3),0,s.real(4),""
            }
            ;
            a.effectiveDeadline=userDeadline(a.id,student,&a.extensionSource);
            out.push_back(std::move(a));
        }
        return out;
    }
    int LmsService::submit(int student,int assignment,const std::string& content,long long submittedAt)
    {
        require(student,"Student");
        nonempty(content,"Submission");
        if(submittedAt<=0)throw Error("Invalid timestamp");
        int course=courseForAssignment(assignment);
        Statement e(db_.handle(),"SELECT 1 FROM enrolment WHERE course_id=? AND student_id=?");
        e.bind(1,course);
        e.bind(2,student);
        if(!e.next())throw Error("Student is not enrolled");
        auto deadline=userDeadline(assignment,student);
        auto prior=submissions_.get(assignment,student);
        std::string status=(submittedAt<=deadline)?"Submitted":"Late";
        if(prior)
        {
            if(submittedAt>deadline||prior->submittedAt>deadline)throw Error("Resubmission after effective deadline forbidden");
            if(prior->status!="Submitted")throw Error("Already marked/moderated or late; resubmission forbidden");
            submissions_.replace(prior->id,content,submittedAt,status);
            return prior->id;
        }
        submissions_.insert(assignment,student,content,submittedAt,status);
        return static_cast<int>(db_.lastId());
    }
    void LmsService::updateMarkAudit(int markId,int actor,std::optional<double> old,double value,const std::string& reason,long long time)
    {
        MarkChange e
        {
            markId,actor,old,value,reason,time
        }
        ;
        audit_.onMarkChange(e);
        /* notification only after transaction COMMIT in callers */
    }
    void LmsService::mark(int lecturer,int submission,double raw,const std::string& feedback,long long at)
    {
        nonempty(feedback,"Written feedback");
        Statement s(db_.handle(),"SELECT s.assignment_id,s.student_id,s.submitted_at,s.status,m.mark_id,m.final_mark FROM submission s LEFT JOIN mark m ON m.submission_id=s.submission_id WHERE s.submission_id=?");
        s.bind(1,submission);
        if(!s.next())throw Error("Submission not found");
        int assignment=s.integer(0),student=s.integer(1);
        long long submitted=s.integer64(2);
        std::string status=s.str(3);
        bool existed=!s.isNull(4);
        int markId=existed?s.integer(4):0;
        std::optional<double> old=existed?std::optional<double>(s.real(5)):std::nullopt;
        requireCourseLecturer(lecturer,courseForAssignment(assignment));
        if(status!="Submitted"&&status!="Late"&&status!="Rejected")throw Error("Mark requires Submitted, Late or Rejected status");
        if(at<=0)throw Error("Invalid timestamp");
        double max=maximumForAssignment(assignment);
        double result=policy_.finalMark(raw,max,submitted-userDeadline(assignment,student));
        Transaction tx(db_);
        if(existed)
        {
            Statement q(db_.handle(),"UPDATE mark SET raw_mark=?,final_mark=?,feedback=?,marked_by=?,moderated_by=NULL WHERE mark_id=?");
            q.bind(1,raw);
            q.bind(2,result);
            q.bind(3,feedback);
            q.bind(4,lecturer);
            q.bind(5,markId);
            q.execute();
        }
        else
        {
            Statement q(db_.handle(),"INSERT INTO mark(submission_id,raw_mark,final_mark,feedback,marked_by) VALUES(?,?,?,?,?)");
            q.bind(1,submission);
            q.bind(2,raw);
            q.bind(3,result);
            q.bind(4,feedback);
            q.bind(5,lecturer);
            q.execute();
            markId=static_cast<int>(db_.lastId());
        }
        Statement state(db_.handle(),"UPDATE submission SET status='Marked' WHERE submission_id=?");
        state.bind(1,submission);
        state.execute();
        updateMarkAudit(markId,lecturer,old,result,existed?"Lecturer re-mark after HoD rejection":"Initial lecturer mark",at);
        tx.commit();
        notices_.onMarkChange(
        {
            markId,lecturer,old,result,"Mark recorded",at
        }
        );
    }
    void LmsService::moderate(int hod,int submission,double revisedRaw,const std::string& reason,long long at)
    {
        nonempty(reason,"Moderation reason");
        Statement s(db_.handle(),"SELECT s.assignment_id,s.student_id,s.submitted_at,s.status,m.mark_id,m.final_mark FROM submission s JOIN mark m ON m.submission_id=s.submission_id WHERE s.submission_id=?");
        s.bind(1,submission);
        if(!s.next())throw Error("Unmarked submission");
        int assignment=s.integer(0),student=s.integer(1);
        long long submitted=s.integer64(2);
        std::string status=s.str(3);
        int markId=s.integer(4);
        double old=s.real(5);
        requireDepartmentHoD(hod,courseForAssignment(assignment));
        if(status!="Marked")throw Error("Only marked submissions can be moderated");
        double revised=policy_.finalMark(revisedRaw,maximumForAssignment(assignment),submitted-userDeadline(assignment,student));
        Transaction tx(db_);
        Statement m(db_.handle(),"UPDATE mark SET raw_mark=?,final_mark=?,moderated_by=? WHERE mark_id=?");
        m.bind(1,revisedRaw);
        m.bind(2,revised);
        m.bind(3,hod);
        m.bind(4,markId);
        m.execute();
        Statement st(db_.handle(),"UPDATE submission SET status='Moderated' WHERE submission_id=?");
        st.bind(1,submission);
        st.execute();
        updateMarkAudit(markId,hod,old,revised,reason,at);
        tx.commit();
        notices_.onMarkChange(
        {
            markId,hod,old,revised,reason,at
        }
        );
    }
    void LmsService::reject(int hod,int submission,const std::string& reason,long long at)
    {
        nonempty(reason,"Rejection reason");
        Statement s(db_.handle(),"SELECT s.assignment_id,s.status,m.mark_id,m.final_mark FROM submission s JOIN mark m ON m.submission_id=s.submission_id WHERE s.submission_id=?");
        s.bind(1,submission);
        if(!s.next())throw Error("Unmarked submission");
        requireDepartmentHoD(hod,courseForAssignment(s.integer(0)));
        if(s.str(1)!="Marked")throw Error("Only marked submissions can be rejected");
        Transaction tx(db_);
        Statement st(db_.handle(),"UPDATE submission SET status='Rejected' WHERE submission_id=?");
        st.bind(1,submission);
        st.execute();
        Statement clear(db_.handle(),"UPDATE mark SET moderated_by=NULL WHERE mark_id=?");
        clear.bind(1,s.integer(2));
        clear.execute();
        // no mark value changed: no mark-change audit is required; record review event separately below
        Statement q(db_.handle(),"INSERT INTO audit_event(mark_id,actor_id,changed_at,old_value,new_value,reason) VALUES(?,?,?,?,?,?)");
        q.bind(1,s.integer(2));
        q.bind(2,hod);
        q.bind(3,at);
        q.bind(4,s.real(3));
        q.bind(5,s.real(3));
        q.bind(6,"Rejected for re-marking: "+reason);
        q.execute();
        tx.commit();
    }
    std::vector<ReviewRow> LmsService::lecturerSubmissions(int lecturer)const
    {
        require(lecturer,"Lecturer");
        Statement s(db_.handle(),"SELECT s.submission_id,s.assignment_id,s.student_id,a.title,s.status,s.content,m.raw_mark,m.final_mark,m.feedback FROM submission s JOIN assignment a ON a.assignment_id=s.assignment_id JOIN course_lecturer cl ON cl.course_id=a.course_id LEFT JOIN mark m ON m.submission_id=s.submission_id WHERE cl.lecturer_id=? ORDER BY s.submission_id");
        s.bind(1,lecturer);
        std::vector<ReviewRow> rows;
        while(s.next())
        {
            ReviewRow row
            {
                s.integer(0),s.integer(1),s.integer(2),s.str(3),s.str(4),s.str(5),s.str(8),std::nullopt,std::nullopt
            }
            ;
            if(!s.isNull(6))row.rawMark=s.real(6);
            if(!s.isNull(7))row.finalMark=s.real(7);
            rows.push_back(std::move(row));
        }
        return rows;
    }
    std::vector<ReviewRow> LmsService::hodSubmissions(int hod)const
    {
        require(hod,"HoD");
        Statement s(db_.handle(),"SELECT s.submission_id,s.assignment_id,s.student_id,a.title,s.status,s.content,m.raw_mark,m.final_mark,m.feedback FROM submission s JOIN assignment a ON a.assignment_id=s.assignment_id JOIN course c ON c.course_id=a.course_id JOIN users u ON u.dept_id=c.dept_id LEFT JOIN mark m ON m.submission_id=s.submission_id WHERE u.user_id=? ORDER BY s.submission_id");
        s.bind(1,hod);
        std::vector<ReviewRow> rows;
        while(s.next())
        {
            ReviewRow row
            {
                s.integer(0),s.integer(1),s.integer(2),s.str(3),s.str(4),s.str(5),s.str(8),std::nullopt,std::nullopt
            }
            ;
            if(!s.isNull(6))row.rawMark=s.real(6);
            if(!s.isNull(7))row.finalMark=s.real(7);
            rows.push_back(std::move(row));
        }
        return rows;
    }
    int LmsService::releaseCourse(int hod,int course)
    {
        requireDepartmentHoD(hod,course);
        Statement pending(db_.handle(),"SELECT COUNT(*) FROM submission s JOIN assignment a ON s.assignment_id=a.assignment_id JOIN mark m ON m.submission_id=s.submission_id WHERE a.course_id=? AND s.status IN ('Marked','Rejected')");
        pending.bind(1,course);
        pending.next();
        if(pending.integer(0)>0)throw Error("All recorded marks must be moderated before course release");
        Statement candidates(db_.handle(),"SELECT COUNT(*) FROM submission s JOIN assignment a ON s.assignment_id=a.assignment_id WHERE a.course_id=? AND s.status='Moderated'");
        candidates.bind(1,course);
        candidates.next();
        int count=candidates.integer(0);
        if(count==0)throw Error("No moderated marks to release");
        Statement s(db_.handle(),"UPDATE submission SET status='Released' WHERE status='Moderated' AND assignment_id IN (SELECT assignment_id FROM assignment WHERE course_id=?)");
        s.bind(1,course);
        s.execute();
        return count;
    }
    std::optional<MarkView> LmsService::releasedMark(int student,int assignment)const
    {
        require(student,"Student");
        Statement q(db_.handle(),"SELECT s.submission_id,s.status,m.final_mark,m.feedback FROM submission s JOIN mark m ON m.submission_id=s.submission_id WHERE s.assignment_id=? AND s.student_id=? AND s.status='Released'");
        q.bind(1,assignment);
        q.bind(2,student);
        if(!q.next())return std::nullopt;
        return MarkView
        {
            q.integer(0),q.str(1),q.real(2),q.str(3)
        }
        ;
    }
    int LmsService::countAudit(int markId)const
    {
        Statement q(db_.handle(),"SELECT COUNT(*) FROM audit_event WHERE mark_id=?");
        q.bind(1,markId);
        q.next();
        return q.integer(0);
    }
    int LmsService::totalRows(const std::string& table)const
    {
        static const std::set<std::string> allowed=
        {
            "users","department","course","assignment","submission","mark","audit_event","extension","class_extension"
        }
        ;
        if(!allowed.count(table))throw Error("Invalid table name");
        Statement s(db_.handle(),"SELECT COUNT(*) FROM "+table);
        s.next();
        return s.integer(0);
    }
    long long parseUtc(const std::string& date)
    {
        static const std::regex utcPattern(R"(^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$)");
        if(!std::regex_match(date,utcPattern))throw Error("Use YYYY-MM-DD HH:MM:SS (UTC)");
        std::tm tm
        {
        }
        ;
        std::istringstream in(date);
        in>>std::get_time(&tm,"%Y-%m-%d %H:%M:%S");
        if(in.fail()||!in.eof())throw Error("Use YYYY-MM-DD HH:MM:SS (UTC)");
        tm.tm_isdst=0;
        const std::tm original=tm;
#ifdef _WIN32
        auto epoch=_mkgmtime(&tm);
#else
        auto epoch=timegm(&tm);
#endif
        if(epoch<0)throw Error("Date must be >= 1970");
        std::tm* check=std::gmtime(&epoch);
        if(!check||check->tm_year!=original.tm_year||check->tm_mon!=original.tm_mon||check->tm_mday!=original.tm_mday||check->tm_hour!=original.tm_hour||check->tm_min!=original.tm_min||check->tm_sec!=original.tm_sec)throw Error("Invalid calendar date");
        return static_cast<long long>(epoch);
    }
    std::string formatUtc(long long epoch)
    {
        auto t=static_cast<std::time_t>(epoch);
        std::tm utc=*std::gmtime(&t);
        std::ostringstream out;
        out<<std::put_time(&utc,"%Y-%m-%d %H:%M:%S UTC");
        return out.str();
    }
}
// namespace lms
