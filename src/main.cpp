#include "lms/lms.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
using namespace lms;
namespace
{
    std::string passwordInput(const std::string& prompt)
    {
        std::cout<<prompt<<": "<<std::flush;
#ifdef _WIN32
        std::string result;
        while(true)
        {
            int ch=_getch();
            if(ch=='\r'||ch=='\n')break;
            if(ch==8)
            {
                if(!result.empty())result.pop_back();
                continue;
            }
            if(ch==0||ch==224)
            {
                (void)_getch();
                continue;
            }
            if(ch>=32&&ch<=126)result.push_back(static_cast<char>(ch));
        }
        std::cout<<"\n";
        return result;
#else
        if(!isatty(STDIN_FILENO))throw Error("Password entry requires an interactive terminal");
        struct termios before
        {
        }
        ;
        if(tcgetattr(STDIN_FILENO,&before)!=0)throw Error("Terminal configuration failed");
        struct termios hidden=before;
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&hidden)!=0)throw Error("Unable to hide password input");
        std::string result;
        try
        {
            if(!std::getline(std::cin,result))throw Error("Password input ended");
        }
        catch(...)
        {
            tcsetattr(STDIN_FILENO,TCSAFLUSH,&before);
            std::cout<<"\n";
            throw;
        }
        tcsetattr(STDIN_FILENO,TCSAFLUSH,&before);
        std::cout<<"\n";
        return result;
#endif
    }
    std::string ask(const std::string& prompt)
    {
        std::cout<<prompt<<": ";
        std::string x;
        if(!std::getline(std::cin,x))throw Error("Input ended");
        return x;
    }
    int num(const std::string& label)
    {
        auto s=ask(label);
        size_t pos=0;
        int n=std::stoi(s,&pos);
        if(pos!=s.size())throw Error("Invalid integer");
        return n;
    }
    double decimal(const std::string& label)
    {
        auto s=ask(label);
        size_t pos=0;
        double n=std::stod(s,&pos);
        if(pos!=s.size())throw Error("Invalid number");
        return n;
    }
    long long date(const std::string& label)
    {
        return parseUtc(ask(label+" [YYYY-MM-DD HH:MM:SS UTC]"));
    }
    void help(const std::string& role)
    {
        std::cout<<"\nRole: "<<role<<" | Type a command or 'help' or 'logout'\n";
        if(role=="Administrator")std::cout<<"add-dept, rename-dept, del-dept, add-user, edit-user, deactivate-user, add-course, edit-course, del-course, assign-lecturer, enrol\n";
        if(role=="Lecturer")std::cout<<"add-assignment, extend-student, extend-class, submissions, mark\n";
        if(role=="HoD")std::cout<<"review, moderate, reject, release\n";
        if(role=="Student")std::cout<<"assignments, submit, results\n";
    }
    void printReviewRows(const std::vector<ReviewRow>& rows)
    {
        if(rows.empty())
        {
            std::cout<<"No submissions available for review\n";
            return;
        }
        for(const auto& r:rows)
        {
            std::cout<<"Submission "<<r.submissionId<<" | assignment "<<r.assignmentId<<" ("<<r.title<<") | student "<<r.studentId<<" | "<<r.status<<"\n";
            std::cout<<"  Work: "<<r.content<<"\n";
            if(r.rawMark)std::cout<<"  Raw: "<<*r.rawMark<<" | final: "<<*r.finalMark<<" | feedback: "<<r.feedback<<"\n";
        }
    }
    void execute(LmsService& app,int id,const std::string& role,const std::string& cmd)
    {
        if(role=="Administrator")
        {
            if(cmd=="add-dept")
            {
                std::cout<<"Department ID="<<app.addDepartment(id,ask("Name"))<<"\n";
                return;
            }
            if(cmd=="rename-dept")
            {
                app.renameDepartment(id,num("Department ID"),ask("New name"));
                return;
            }
            if(cmd=="del-dept")
            {
                app.deleteDepartment(id,num("Department ID"));
                return;
            }
            if(cmd=="add-user")
            {
                auto name=ask("Username"),pass=passwordInput("Password");
                auto role=ask("Role [Student/Lecturer/HoD/Administrator]");
                int dept=num("Department ID (0 if not applicable)");
                std::cout<<"User ID="<<app.addUser(id,name,pass,role,dept)<<"\n";
                return;
            }
            if(cmd=="edit-user")
            {
                int target=num("User ID");
                auto name=ask("Username"),role=ask("Role");
                app.updateUser(id,target,name,role,num("Department ID (0 for none)"));
                return;
            }
            if(cmd=="deactivate-user")
            {
                app.deactivateUser(id,num("User ID"));
                return;
            }
            if(cmd=="add-course")
            {
                int dept=num("Department ID");
                auto code=ask("Course code"),title=ask("Title");
                std::cout<<"Course ID="<<app.addCourse(id,dept,code,title)<<"\n";
                return;
            }
            if(cmd=="edit-course")
            {
                int course=num("Course ID");
                app.editCourse(id,course,ask("New code"),ask("New title"));
                return;
            }
            if(cmd=="del-course")
            {
                app.deleteCourse(id,num("Course ID"));
                return;
            }
            if(cmd=="assign-lecturer")
            {
                app.assignLecturer(id,num("Course ID"),num("Lecturer ID"));
                return;
            }
            if(cmd=="enrol")
            {
                app.enrol(id,num("Course ID"),num("Student ID"));
                return;
            }
        }
        if(role=="Lecturer")
        {
            if(cmd=="submissions")
            {
                printReviewRows(app.lecturerSubmissions(id));
                return;
            }
            if(cmd=="add-assignment")
            {
                int course=num("Course ID");
                auto title=ask("Title");
                auto deadline=date("Deadline");
                std::cout<<"Assignment ID="<<app.addAssignment(id,course,title,deadline,decimal("Maximum mark"))<<"\n";
                return;
            }
            if(cmd=="extend-student")
            {
                int a=num("Assignment ID"),s=num("Student ID");
                auto d=date("New deadline");
                app.individualExtension(id,a,s,d,ask("Reason"));
                return;
            }
            if(cmd=="extend-class")
            {
                int a=num("Assignment ID");
                auto d=date("New class deadline");
                app.classExtension(id,a,d,ask("Reason"));
                return;
            }
            if(cmd=="mark")
            {
                int s=num("Submission ID");
                double m=decimal("Raw mark");
                app.mark(id,s,m,ask("Written feedback"),static_cast<long long>(std::time(nullptr)));
                return;
            }
        }
        if(role=="HoD")
        {
            if(cmd=="review")
            {
                printReviewRows(app.hodSubmissions(id));
                return;
            }
            if(cmd=="moderate")
            {
                int s=num("Submission ID");
                double m=decimal("Reviewed raw mark");
                app.moderate(id,s,m,ask("Reason (required)"),static_cast<long long>(std::time(nullptr)));
                return;
            }
            if(cmd=="reject")
            {
                app.reject(id,num("Submission ID"),ask("Reason"),static_cast<long long>(std::time(nullptr)));
                return;
            }
            if(cmd=="release")
            {
                std::cout<<"Released "<<app.releaseCourse(id,num("Course ID"))<<" mark(s)\n";
                return;
            }
        }
        if(role=="Student")
        {
            if(cmd=="assignments")
            {
                for(const auto& a:app.studentAssignments(id))std::cout<<"ID "<<a.id<<" | "<<a.title<<" | max "<<a.maximum<<" | effective due "<<formatUtc(a.effectiveDeadline)<<" ("<<a.extensionSource<<")\n";
                return;
            }
            if(cmd=="submit")
            {
                int assignment=num("Assignment ID");
                std::cout<<"Submission ID="<<app.submit(id,assignment,ask("Text content"),static_cast<long long>(std::time(nullptr)))<<"\n";
                return;
            }
            if(cmd=="results")
            {
                for(const auto& a:app.studentAssignments(id))
                {
                    auto m=app.releasedMark(id,a.id);
                    if(m)std::cout<<a.title<<": "<<m->finalMark<<" | "<<m->feedback<<"\n";
                    else std::cout<<a.title<<": not released\n";
                }
                return;
            }
        }
        throw Error("Unknown command for current role");
    }
}
int main(int argc,char** argv)
{
    try
    {
        std::string path=(argc>=2)?argv[1]:"lms.db";
        LmsService app(path);
        if(argc>=3&&std::string(argv[2])=="--init-admin")
        {
            auto name=ask("Admin username");
            auto password=passwordInput("Admin password (at least 10 characters)");
            std::cout<<"Administrator ID="<<app.bootstrapAdmin(name,password)<<"\n";
            return 0;
        }
        std::cout<<"CS4433 Moderated LMS v1.1 | SQLite: "<<path<<"\n";
        while(true)
        {
            try
            {
                auto name=ask("Login username (or exit) ");
                if(name=="exit")break;
                int id=app.login(name,passwordInput("Password"));
                auto actor=app.user(id);
                std::cout<<"Welcome "<<actor->username()<<"\n";
                help(actor->role());
                while(true)
                {
                    try
                    {
                        auto cmd=ask(actor->role()+" command");
                        if(cmd=="logout")break;
                        if(cmd=="help")
                        {
                            help(actor->role());
                            continue;
                        }
                        execute(app,id,actor->role(),cmd);
                        std::cout<<"OK\n";
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr<<"Error: "<<e.what()<<"\n";
                    }
                }
            }
            catch(const std::exception& e)
            {
                std::cerr<<"Error: "<<e.what()<<"\n";
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr<<"Fatal: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
