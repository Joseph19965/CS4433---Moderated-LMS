#pragma once
#include <sqlite3.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace lms
{
    struct Error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    }
    ;
    class Statement
    {
        sqlite3_stmt* st_
        {
        }
        ;
        public: Statement(sqlite3* db,const std::string& sql);
        ~Statement();
        Statement(const Statement&)=delete;
        Statement& operator=(const Statement&)=delete;
        void bind(int i,int v);
        void bind(int i,long long v);
        void bind(int i,double v);
        void bind(int i,const std::string& v);
        void null(int i);
        bool next();
        void execute();
        int integer(int i)const;
        long long integer64(int i)const;
        double real(int i)const;
        std::string str(int i)const;
        bool isNull(int i)const;
    }
    ;
    class Database
    {
        sqlite3* db_
        {
        }
        ;
        public: explicit Database(const std::string& path);
        ~Database();
        Database(const Database&)=delete;
        Database& operator=(const Database&)=delete;
        sqlite3* handle()const
        {
            return db_;
        }
        void exec(const std::string& sql);
        long long lastId()const;
        void schema();
    }
    ;
    class User
    {
        protected:int id_;
        std::string username_;
        public:User(int id,std::string username):id_(id),username_(std::move(username))
        {
        }
        virtual ~User()=default;
        virtual std::string role()const=0;
        int id()const
        {
            return id_;
        }
        std::string username()const
        {
            return username_;
        }
    }
    ;
    class Student:public User
    {
        public:using User::User;
        std::string role()const override
        {
            return "Student";
        }
    }
    ;
    class Lecturer:public User
    {
        public:using User::User;
        std::string role()const override
        {
            return "Lecturer";
        }
    }
    ;
    class HoD:public User
    {
        public:using User::User;
        std::string role()const override
        {
            return "HoD";
        }
    }
    ;
    class Administrator:public User
    {
        public:using User::User;
        std::string role()const override
        {
            return "Administrator";
        }
    }
    ;
    struct UserFactory
    {
        static std::unique_ptr<User> create(int id,const std::string& username,const std::string& role);
    }
    ;
    class LatePenaltyStrategy
    {
        public:virtual ~LatePenaltyStrategy()=default;
        virtual double finalMark(double raw,double maximum,long long secondsLate)const=0;
    }
    ;
    class StandardLatePenalty:public LatePenaltyStrategy
    {
        public:double finalMark(double raw,double maximum,long long secondsLate)const override;
    }
    ;
    struct MarkChange
    {
        int markId,actor;
        std::optional<double> oldValue;
        double newValue;
        std::string reason;
        long long when;
    }
    ;
    class MarkObserver
    {
        public:virtual ~MarkObserver()=default;
        virtual void onMarkChange(const MarkChange& event)=0;
    }
    ;
    class AuditService final: public MarkObserver
    {
        Database& db_;
        public:explicit AuditService(Database& d):db_(d)
        {
        }
        void onMarkChange(const MarkChange& event) override;
    }
    ;
    class NotificationService final:public MarkObserver
    {
        std::vector<std::string> messages_;
        public:void onMarkChange(const MarkChange& event)override;
        const std::vector<std::string>& messages()const
        {
            return messages_;
        }
    }
    ;
    struct SubmissionView
    {
        int id;
        int assignmentId;
        int studentId;
        std::string content,status;
        long long submittedAt;
    }
    ;
    class SubmissionRepository
    {
        Database& db_;
        public:explicit SubmissionRepository(Database& db):db_(db)
        {
        }
        std::optional<SubmissionView> get(int assignment,int student)const;
        void insert(int assignment,int student,const std::string& content,long long timestamp,const std::string& status);
        void replace(int id,const std::string& content,long long timestamp,const std::string& status);
    }
    ;
    struct AssignmentView
    {
        int id,courseId;
        std::string title;
        long long deadline,effectiveDeadline;
        double maximum;
        std::string extensionSource;
    }
    ;
    struct MarkView
    {
        int submissionId;
        std::string status;
        double finalMark;
        std::string feedback;
    }
    ;
    struct ReviewRow
    {
        int submissionId,assignmentId,studentId;
        std::string title,status,content,feedback;
        std::optional<double> rawMark,finalMark;
    }
    ;
    class LmsService
    {
        Database db_;
        StandardLatePenalty policy_;
        AuditService audit_;
        NotificationService notices_;
        SubmissionRepository submissions_;
        void require(int user,const std::string& role)const;
        void requireCourseLecturer(int lecturer,int course)const;
        void requireDepartmentHoD(int hod,int course)const;
        long long assignmentDeadline(int assignment)const;
        long long userDeadline(int assignment,int student,std::string* source=nullptr)const;
        int courseForAssignment(int assignment)const;
        double maximumForAssignment(int assignment)const;
        void updateMarkAudit(int markId,int actor,std::optional<double> oldMark,double newMark,const std::string& reason,long long at);
        public: explicit LmsService(const std::string& databasePath);
        Database& database()
        {
            return db_;
        }
        int bootstrapAdmin(const std::string& username,const std::string& password);
        int login(const std::string& username,const std::string& password)const;
        std::unique_ptr<User> user(int id)const;
        int addUser(int admin,const std::string& name,const std::string& password,const std::string& role,int department=0);
        void updateUser(int admin,int target,const std::string& username,const std::string& role,int department=0);
        void deactivateUser(int admin,int target);
        int addDepartment(int admin,const std::string& name);
        void renameDepartment(int admin,int id,const std::string& name);
        void deleteDepartment(int admin,int id);
        int addCourse(int admin,int department,const std::string& code,const std::string& title);
        void editCourse(int admin,int id,const std::string& code,const std::string& title);
        void deleteCourse(int admin,int id);
        void assignLecturer(int admin,int course,int lecturer);
        void enrol(int admin,int course,int student);
        int addAssignment(int lecturer,int course,const std::string& title,long long deadline,double maximum);
        void individualExtension(int lecturer,int assignment,int student,long long newDeadline,const std::string& reason);
        void classExtension(int lecturer,int assignment,long long newDeadline,const std::string& reason);
        std::vector<AssignmentView> studentAssignments(int student)const;
        int submit(int student,int assignment,const std::string& content,long long submittedAt);
        void mark(int lecturer,int submission,double raw,const std::string& feedback,long long at);
        void moderate(int hod,int submission,double revisedRaw,const std::string& reason,long long at);
        void reject(int hod,int submission,const std::string& reason,long long at);
        std::vector<ReviewRow> lecturerSubmissions(int lecturer)const;
        std::vector<ReviewRow> hodSubmissions(int hod)const;
        int releaseCourse(int hod,int course);
        std::optional<MarkView> releasedMark(int student,int assignment)const;
        int countAudit(int markId)const;
        int totalRows(const std::string& table)const;
        // allowed whitelist; diagnostic only
    }
    ;
    long long parseUtc(const std::string& datetime);
    std::string formatUtc(long long epoch);
}
