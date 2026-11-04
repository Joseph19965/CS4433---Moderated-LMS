#include <catch2/catch_test_macros.hpp>
#include "lms/lms.hpp"
using namespace lms;
TEST_CASE("Penalty boundaries: on-time, daily ceiling and >72 hours", "[unit][penalty]")
{
    StandardLatePenalty p;
    REQUIRE(p.finalMark(90,100,0)==90);
    REQUIRE(p.finalMark(90,100,1)==80);
    REQUIRE(p.finalMark(90,100,86400)==80);
    REQUIRE(p.finalMark(90,100,86401)==70);
    REQUIRE(p.finalMark(90,100,259200)==60);
    REQUIRE(p.finalMark(90,100,259201)==0);
    REQUIRE_THROWS_AS(p.finalMark(110,100,0),Error);
}
TEST_CASE("Password storage, login and role polymorphism", "[integration][security]")
{
    LmsService s(":memory:");
    int admin=s.bootstrapAdmin("root","SecureSecret-2026!");
    REQUIRE(s.login("root","SecureSecret-2026!")==admin);
    REQUIRE(s.user(admin)->role()=="Administrator");
    REQUIRE_THROWS_AS(s.login("root","wrong"),Error);
    Statement q(s.database().handle(),"SELECT password_hash FROM users WHERE user_id=?");
    q.bind(1,admin);
    REQUIRE(q.next());
    REQUIRE(q.str(0).find("pbkdf2-sha256$")==0);
    REQUIRE(q.str(0).find("SecureSecret-2026!")==std::string::npos);
}
TEST_CASE("HoD release gate and audit trail", "[integration][moderation]")
{
    LmsService a(":memory:");
    int root=a.bootstrapAdmin("root","SecureSecret-2026!");
    int dep=a.addDepartment(root,"Math");
    int lecturer=a.addUser(root,"teach","SecureSecret-2026!","Lecturer",dep);
    int hod=a.addUser(root,"head","SecureSecret-2026!","HoD",dep);
    int student=a.addUser(root,"learner","SecureSecret-2026!","Student");
    int course=a.addCourse(root,dep,"MATH","Math");
    a.assignLecturer(root,course,lecturer);
    a.enrol(root,course,student);
    int assignment=a.addAssignment(lecturer,course,"Homework",1900000000,100);
    int submission=a.submit(student,assignment,"work",1900000000);
    a.mark(lecturer,submission,89,"Good",1900000001);
    REQUIRE_FALSE(a.releasedMark(student,assignment).has_value());
    REQUIRE_THROWS_AS(a.releaseCourse(hod,course),Error);
    a.moderate(hod,submission,87,"Corrected calculation",1900000002);
    REQUIRE_FALSE(a.releasedMark(student,assignment).has_value());
    REQUIRE(a.releaseCourse(hod,course)==1);
    REQUIRE(a.releasedMark(student,assignment)->finalMark==87);
    Statement q(a.database().handle(),"SELECT mark_id FROM mark WHERE submission_id=?");
    q.bind(1,submission);
    REQUIRE(q.next());
    REQUIRE(a.countAudit(q.integer(0))==2);
}
TEST_CASE("CR1 class extension and individual extension choose latest deadline", "[integration][CR1]")
{
    LmsService a(":memory:");
    int root=a.bootstrapAdmin("root","SecureSecret-2026!");
    int dep=a.addDepartment(root,"Math");
    int teacher=a.addUser(root,"teach","SecureSecret-2026!","Lecturer",dep);
    int student=a.addUser(root,"learn","SecureSecret-2026!","Student");
    int course=a.addCourse(root,dep,"MATH","Math");
    a.assignLecturer(root,course,teacher);
    a.enrol(root,course,student);
    int assignment=a.addAssignment(teacher,course,"Homework",1900000000,100);
    a.classExtension(teacher,assignment,1900000000+2*86400,"School closure");
    REQUIRE(a.studentAssignments(student)[0].extensionSource=="class");
    REQUIRE(a.submit(student,assignment,"after original due",1900000000+86400)>0);
    a.individualExtension(teacher,assignment,student,1900000000+3*86400,"Illness");
    REQUIRE(a.studentAssignments(student)[0].extensionSource=="individual");
    REQUIRE(a.studentAssignments(student)[0].effectiveDeadline==1900000000+3*86400);
}
TEST_CASE("Lecturer and HoD restricted review queues", "[integration][review]")
{
    LmsService a(":memory:");
    int admin=a.bootstrapAdmin("admin","Administrative-2026!");
    int dept=a.addDepartment(admin,"Science");
    int other=a.addDepartment(admin,"Arts");
    int lec=a.addUser(admin,"lect","LecturerPass-2026!","Lecturer",dept);
    int hod=a.addUser(admin,"hod","HoDPassword-2026!","HoD",dept);
    int hod2=a.addUser(admin,"hod2","HoDPassword-2026!","HoD",other);
    int student=a.addUser(admin,"student","StudentPass-2026!","Student");
    int course=a.addCourse(admin,dept,"SC01","Science");
    a.assignLecturer(admin,course,lec);
    a.enrol(admin,course,student);
    int assignment=a.addAssignment(lec,course,"Essay",1900000000,100);
    a.submit(student,assignment,"Essay text",1900000000);
    REQUIRE(a.lecturerSubmissions(lec).size()==1);
    REQUIRE(a.hodSubmissions(hod).size()==1);
    REQUIRE(a.hodSubmissions(hod2).empty());
    REQUIRE_THROWS_AS(a.hodSubmissions(student),Error);
}
