#include "lms/lms.hpp"
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
using namespace lms;
namespace
{
    int passed=0, failed=0;
    void check(bool b,const std::string& what)
    {
        if(!b)throw std::runtime_error(what);
    }
    void eq(double a,double b)
    {
        check(std::fabs(a-b)<0.000001,"expected "+std::to_string(b)+", got "+std::to_string(a));
    }
    void fails(const std::function<void()>& f)
    {
        bool error=false;
        try
        {
            f();
        }
        catch(const std::exception&)
        {
            error=true;
        }
        check(error,"expected exception");
    }
    void testcase(const std::string& name,const std::function<void()>& f)
    {
        try
        {
            f();
            ++passed;
            std::cout<<"PASS "<<name<<"\n";
        }
        catch(const std::exception& e)
        {
            ++failed;
            std::cerr<<"FAIL "<<name<<" "<<e.what()<<"\n";
        }
    }
    struct Fixture
    {
        LmsService app
        {
            ":memory:"
        }
        ;
        int admin,dept,otherDept,lecturer,otherLecturer,hod,otherHod,student,student2,course,course2,assignment;
        long long deadline=1900000000;
        Fixture()
        {
            admin=app.bootstrapAdmin("admin","Administrator-2026!");
            dept=app.addDepartment(admin,"Science");
            otherDept=app.addDepartment(admin,"History");
            lecturer=app.addUser(admin,"lecturer","Lecturer-2026!","Lecturer",dept);
            otherLecturer=app.addUser(admin,"outsider","Outsider-2026!","Lecturer",otherDept);
            hod=app.addUser(admin,"hod","Moderation-2026!","HoD",dept);
            otherHod=app.addUser(admin,"otherhod","Moderation-2026!","HoD",otherDept);
            student=app.addUser(admin,"alice","StudentOne-2026!","Student");
            student2=app.addUser(admin,"bob","StudentTwo-2026!","Student");
            course=app.addCourse(admin,dept,"SCI101","Science");
            course2=app.addCourse(admin,otherDept,"HIS101","History");
            app.assignLecturer(admin,course,lecturer);
            app.assignLecturer(admin,course2,otherLecturer);
            app.enrol(admin,course,student);
            app.enrol(admin,course,student2);
            assignment=app.addAssignment(lecturer,course,"Quiz",deadline,100.0);
        }
        int markId(int submission)
        {
            Statement s(app.database().handle(),"SELECT mark_id FROM mark WHERE submission_id=?");
            s.bind(1,submission);
            check(s.next(),"mark missing");
            return s.integer(0);
        }
        std::string status(int submission)
        {
            Statement s(app.database().handle(),"SELECT status FROM submission WHERE submission_id=?");
            s.bind(1,submission);
            check(s.next(),"submission missing");
            return s.str(0);
        }
        double final(int submission)
        {
            Statement s(app.database().handle(),"SELECT final_mark FROM mark WHERE submission_id=?");
            s.bind(1,submission);
            check(s.next(),"mark missing");
            return s.real(0);
        }
    }
    ;
}
int main()
{
    testcase("UTC parser validation and round-trip",[]
    {
        auto t=parseUtc("2030-03-15 12:13:14");check(formatUtc(t)=="2030-03-15 12:13:14 UTC","utc");fails([]
        {
            parseUtc("2030-02-30 00:00:00");
        }
        );fails([]
        {
            parseUtc("2030-01-01");
        }
        );
    }
    );
    testcase("penalty equivalence classes and deadline boundaries",[]
    {
        StandardLatePenalty p;eq(p.finalMark(84,100,-1),84);eq(p.finalMark(84,100,0),84);eq(p.finalMark(84,100,1),74);eq(p.finalMark(84,100,86400),74);eq(p.finalMark(84,100,86401),64);eq(p.finalMark(84,100,2*86400),64);eq(p.finalMark(84,100,2*86400+1),54);eq(p.finalMark(84,100,3*86400),54);eq(p.finalMark(84,100,3*86400+1),0);eq(p.finalMark(5,100,3*86400),0);fails([&]
        {
            p.finalMark(101,100,0);
        }
        );fails([&]
        {
            p.finalMark(-2,100,0);
        }
        );
    }
    );
    testcase("authentication, protected salted password, factory polymorphism",[]
    {
        Fixture f;check(f.app.login("admin","Administrator-2026!")==f.admin,"login failed");fails([&]
        {
            f.app.login("admin","invalid");
        }
        );fails([&]
        {
            f.app.login("nobody","Administrator-2026!");
        }
        );auto u=f.app.user(f.student);check(u->role()=="Student","polymorphic student");check(f.app.user(f.hod)->role()=="HoD","hod");check(f.app.user(f.lecturer)->role()=="Lecturer","lecturer");Statement q(f.app.database().handle(),"SELECT password_hash FROM users WHERE user_id=?");q.bind(1,f.student);check(q.next(),"hash");auto h=q.str(0);check(h.find("StudentOne-2026!")==std::string::npos&&h.find("pbkdf2-sha256$")==0,"password is improperly stored");fails([&]
        {
            f.app.addUser(f.admin,"weak","short","Student");
        }
        );
    }
    );
    testcase("administrator CRUD and RBAC restrictions",[]
    {
        Fixture f;fails([&]
        {
            f.app.addDepartment(f.student,"Hacked");
        }
        );fails([&]
        {
            f.app.assignLecturer(f.student,f.course,f.lecturer);
        }
        );fails([&]
        {
            f.app.addAssignment(f.otherLecturer,f.course,"No",f.deadline,100);
        }
        );fails([&]
        {
            f.app.addAssignment(f.lecturer,f.course,"No",f.deadline,-1);
        }
        );fails([&]
        {
            f.app.addUser(f.admin,"test","Password-2026!","Alien");
        }
        );int d=f.app.addDepartment(f.admin,"Arts");f.app.renameDepartment(f.admin,d,"Music");f.app.deleteDepartment(f.admin,d);int c=f.app.addCourse(f.admin,f.dept,"BIO101","Biology");f.app.editCourse(f.admin,c,"BIO102","Biology II");f.app.deleteCourse(f.admin,c);int u=f.app.addUser(f.admin,"newuser","Newuser-2026!","Student");f.app.updateUser(f.admin,u,"updated","Student");f.app.deactivateUser(f.admin,u);fails([&]
        {
            f.app.login("updated","Newuser-2026!");
        }
        );
    }
    );
    testcase("enrolment and initial/resubmitted submissions",[]
    {
        Fixture f;fails([&]
        {
            f.app.submit(f.hod,f.assignment,"work",f.deadline);
        }
        );fails([&]
        {
            f.app.submit(f.student,f.assignment,"  ",f.deadline);
        }
        );auto id=f.app.submit(f.student,f.assignment,"v1",f.deadline-100);check(f.status(id)=="Submitted","on time");check(f.app.submit(f.student,f.assignment,"v2",f.deadline)==id,"replace record");check(f.app.totalRows("submission")==1,"single active submission");fails([&]
        {
            f.app.submit(f.student,f.assignment,"v3",f.deadline+1);
        }
        );fails([&]
        {
            f.app.submit(f.student,999,"v",f.deadline);
        }
        );
    }
    );
    testcase("late acceptance, penalty, no late resubmission",[]
    {
        Fixture f;int s=f.app.submit(f.student,f.assignment,"late",f.deadline+86401);check(f.status(s)=="Late","late status");fails([&]
        {
            f.app.submit(f.student,f.assignment,"late2",f.deadline+86402);
        }
        );f.app.mark(f.lecturer,s,85,"Well done",f.deadline+90000);eq(f.final(s),65);fails([&]
        {
            f.app.releasedMark(f.hod,f.assignment);
        }
        );check(!f.app.releasedMark(f.student,f.assignment).has_value(),"early exposure");f.app.moderate(f.hod,s,85,"reviewed",f.deadline+100000);eq(f.final(s),65);f.app.releaseCourse(f.hod,f.course);auto m=f.app.releasedMark(f.student,f.assignment);check(m.has_value()&&m->status=="Released","not released");eq(m->finalMark,65);check(m->feedback=="Well done","feedback");check(f.app.countAudit(f.markId(s))==2,"audit for mark and moderation");
    }
    );
    testcase("late >72 hours accepted with zero",[]
    {
        Fixture f;int s=f.app.submit(f.student,f.assignment,"late",f.deadline+3*86400+1);f.app.mark(f.lecturer,s,100,"Too late",f.deadline+4*86400);eq(f.final(s),0);
    }
    );
    testcase("individual extensions apply to only selected student",[]
    {
        Fixture f;f.app.individualExtension(f.lecturer,f.assignment,f.student,f.deadline+2*86400,"illness");auto a=f.app.studentAssignments(f.student);auto b=f.app.studentAssignments(f.student2);check(a.size()==1&&a[0].effectiveDeadline==f.deadline+2*86400&&a[0].extensionSource=="individual","extension visible");check(b[0].effectiveDeadline==f.deadline&&b[0].extensionSource=="original","other student changed");fails([&]
        {
            f.app.individualExtension(f.otherLecturer,f.assignment,f.student,f.deadline+10,"not assigned");
        }
        );fails([&]
        {
            f.app.individualExtension(f.lecturer,f.assignment,f.student,f.deadline,"not extended");
        }
        );int s=f.app.submit(f.student,f.assignment,"work",f.deadline+86400);check(f.status(s)=="Submitted","individual effective due");
    }
    );
    testcase("CR1 class extension, overlapping overrides, student list",[]
    {
        Fixture f;f.app.classExtension(f.lecturer,f.assignment,f.deadline+3*86400,"weather");auto a=f.app.studentAssignments(f.student);check(a[0].effectiveDeadline==f.deadline+3*86400&&a[0].extensionSource=="class","class extension missing");f.app.individualExtension(f.lecturer,f.assignment,f.student,f.deadline+4*86400,"medical");check(f.app.studentAssignments(f.student)[0].extensionSource=="individual","individual precedence");f.app.individualExtension(f.lecturer,f.assignment,f.student2,f.deadline+86400,"initial");check(f.app.studentAssignments(f.student2)[0].extensionSource=="class","longest deadline wins");int s=f.app.submit(f.student2,f.assignment,"allowed",f.deadline+2*86400);check(f.status(s)=="Submitted","class extension ignored");fails([&]
        {
            f.app.classExtension(f.otherLecturer,f.assignment,f.deadline+4*86400,"unauthorized");
        }
        );fails([&]
        {
            f.app.classExtension(f.lecturer,f.assignment,f.deadline,"not extended");
        }
        );check(f.app.totalRows("class_extension")==1,"upsert");
    }
    );
    testcase("HoD separation, rejection/re-mark/moderation/release",[]
    {
        Fixture f;int s=f.app.submit(f.student,f.assignment,"v1",f.deadline);f.app.mark(f.lecturer,s,77,"draft",f.deadline+10);fails([&]
        {
            f.app.releaseCourse(f.hod,f.course);
        }
        );fails([&]
        {
            f.app.moderate(f.otherHod,s,70,"cross-department",f.deadline+20);
        }
        );f.app.reject(f.hod,s,"check rubric",f.deadline+30);check(f.status(s)=="Rejected","rejected status");fails([&]
        {
            f.app.submit(f.student,f.assignment,"new",f.deadline-1);
        }
        );f.app.mark(f.lecturer,s,81,"remark",f.deadline+40);f.app.moderate(f.hod,s,82,"correct sum",f.deadline+50);check(f.status(s)=="Moderated","moderated");f.app.releaseCourse(f.hod,f.course);eq(f.app.releasedMark(f.student,f.assignment)->finalMark,82);check(f.app.countAudit(f.markId(s))==4,"audit all events");fails([&]
        {
            f.app.releaseCourse(f.otherHod,f.course);
        }
        );
    }
    );
    testcase("batch release only after all recorded marks moderated",[]
    {
        Fixture f;int s1=f.app.submit(f.student,f.assignment,"one",f.deadline);int s2=f.app.submit(f.student2,f.assignment,"two",f.deadline);f.app.mark(f.lecturer,s1,70,"ok",f.deadline+1);f.app.mark(f.lecturer,s2,80,"ok",f.deadline+1);f.app.moderate(f.hod,s1,70,"accepted",f.deadline+2);fails([&]
        {
            f.app.releaseCourse(f.hod,f.course);
        }
        );check(!f.app.releasedMark(f.student,f.assignment),"partial leak");f.app.moderate(f.hod,s2,80,"accepted",f.deadline+2);check(f.app.releaseCourse(f.hod,f.course)==2,"batch size");
    }
    );
    testcase("SQL injection is data, SQL errors handled",[]
    {
        Fixture f;f.app.addDepartment(f.admin,"Science'); DROP TABLE users;--");check(f.app.totalRows("users")==7,"injected SQL executed");fails([&]
        {
            f.app.totalRows("users;DROP TABLE users");
        }
        );fails([&]
        {
            f.app.addDepartment(f.admin,"Science");
        }
        );
    }
    );
    testcase("persistence reopens DB",[]
    {
        namespace fs=std::filesystem;auto tmp=fs::temp_directory_path()/("cs4433-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".db");int expected=0;
        {
            LmsService a(tmp.string());expected=a.bootstrapAdmin("persistadmin","Persistence-2026!");int d=a.addDepartment(expected,"Persist Dept");a.addCourse(expected,d,"PER1","Stored");
        }
        {
            LmsService b(tmp.string());check(b.login("persistadmin","Persistence-2026!")==expected,"not persisted");check(b.totalRows("course")==1,"course not persisted");
        }
        fs::remove(tmp);fs::remove(tmp.string()+"-wal");fs::remove(tmp.string()+"-shm");
    }
    );
    testcase("lecturer and departmental HoD review queues",[]
    {
        Fixture f;int sub=f.app.submit(f.student,f.assignment,"Reviewed work",f.deadline);check(f.app.lecturerSubmissions(f.lecturer).size()==1,"lecturer cannot see assigned submission");check(f.app.lecturerSubmissions(f.lecturer)[0].content=="Reviewed work","work text hidden from lecturer");check(f.app.lecturerSubmissions(f.otherLecturer).empty(),"cross-course disclosure");check(f.app.hodSubmissions(f.hod).size()==1,"HoD department queue absent");check(f.app.hodSubmissions(f.otherHod).empty(),"cross-department disclosure");fails([&]
        {
            f.app.hodSubmissions(f.student);
        }
        );fails([&]
        {
            f.app.lecturerSubmissions(f.student);
        }
        );f.app.mark(f.lecturer,sub,91,"Good",f.deadline+1);check(f.app.hodSubmissions(f.hod)[0].rawMark.has_value(),"review mark missing");eq(*f.app.hodSubmissions(f.hod)[0].rawMark,91);
    }
    );
    testcase("audit prevents unauthorized access and supports FK constraints",[]
    {
        Fixture f;int s=f.app.submit(f.student,f.assignment,"work",f.deadline);fails([&]
        {
            f.app.mark(f.otherLecturer,s,90,"bad actor",f.deadline+1);
        }
        );f.app.mark(f.lecturer,s,90,"valid",f.deadline+1);fails([&]
        {
            f.app.moderate(f.student,s,90,"bad actor",f.deadline+2);
        }
        );int id=f.markId(s);Statement q(f.app.database().handle(),"SELECT actor_id,old_value,new_value,reason FROM audit_event WHERE mark_id=?");q.bind(1,id);check(q.next(),"audit absent");check(q.integer(0)==f.lecturer&&q.isNull(1)&&q.real(2)==90&&!q.str(3).empty(),"audit columns");
    }
    );
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed\n";
    return failed?1:0;
}
