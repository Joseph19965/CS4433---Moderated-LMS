from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
dir=root/'diagrams';dir.mkdir(exist_ok=True)
puml={
'01_component':'''@startuml
skinparam componentStyle rectangle
actor "Student/Lecturer/HoD/Admin" as Actor
package "Presentation layer" { [ConsoleUI\nmain.cpp] as UI }
package "Business logic layer" { [LmsService] as SVC
 [StandardLatePenalty\n(Strategy)] as PEN
 [UserFactory] as FAC
 [AuditService] as AUD
 [NotificationService] as NOT }
package "Data access layer" { [Database / Statement] as DB
 [SubmissionRepository] as REPO }
database "SQLite school.db" as SQL
Actor --> UI
UI --> SVC
SVC --> PEN
SVC --> FAC
SVC --> AUD
SVC --> NOT
SVC --> REPO
SVC --> DB
REPO --> DB
AUD --> DB
DB --> SQL
@enduml''',
'02_class':'''@startuml
skinparam classAttributeIconSize 0
abstract class User { #id_: int\n#username_: string\n+id(): int\n+username(): string\n+role(): string {abstract} }
class Student
class Lecturer
class HoD
class Administrator
User <|-- Student
User <|-- Lecturer
User <|-- HoD
User <|-- Administrator
class UserFactory { +create(id:int,name:string,role:string): unique_ptr<User> }
UserFactory ..> User : creates
interface LatePenaltyStrategy { +finalMark(raw:double,max:double,secondsLate:int64): double }
class StandardLatePenalty
LatePenaltyStrategy <|.. StandardLatePenalty
interface MarkObserver { +onMarkChange(event:MarkChange):void }
class AuditService { -db_: Database&\n+onMarkChange(event):void }
class NotificationService { -messages_:vector<string>\n+onMarkChange(event):void }
MarkObserver <|.. AuditService
MarkObserver <|.. NotificationService
class MarkChange { +markId:int\n+actor:int\n+oldValue:optional<double>\n+newValue:double\n+reason:string\n+when:int64 }
class Database { -db_:sqlite3*\n+exec(sql:string):void\n+schema():void\n+lastId():int64 }
class Statement { -st_:sqlite3_stmt*\n+bind(index:int,value):void\n+next():bool\n+execute():void }
class SubmissionRepository { -db_:Database&\n+get(assignment:int,student:int):optional<SubmissionView>\n+insert(...):void\n+replace(...):void }
class LmsService { -db_:Database\n-policy_:StandardLatePenalty\n-audit_:AuditService\n-notices_:NotificationService\n-submissions_:SubmissionRepository\n+login(...):int\n+addAssignment(...):int\n+submit(...):int\n+mark(...):void\n+moderate(...):void\n+releaseCourse(...):int\n+individualExtension(...):void\n+classExtension(...):void\n+studentAssignments(...):vector<AssignmentView> }
LmsService *-- Database
LmsService *-- SubmissionRepository
LmsService *-- StandardLatePenalty
LmsService *-- AuditService
LmsService *-- NotificationService
SubmissionRepository --> Database
AuditService --> Database
Database ..> Statement
class AssignmentView { +id:int\n+courseId:int\n+deadline:int64\n+effectiveDeadline:int64\n+maximum:double\n+extensionSource:string }
class SubmissionView { +id:int\n+status:string\n+submittedAt:int64 }
class MarkView { +finalMark:double\n+feedback:string\n+status:string }
LmsService ..> AssignmentView
LmsService ..> MarkView
SubmissionRepository ..> SubmissionView
@enduml''',
'03_state':'''@startuml
[*] --> Created
Created --> Submitted : submitAt <= effectiveDeadline
Created --> Late : submitAt > effectiveDeadline
Submitted --> Submitted : resubmit before effective deadline
Submitted --> Marked : lecturer marks / feedback
Late --> Marked : lecturer marks / penalty
Marked --> Moderated : HoD reviews and approves
Marked --> Rejected : HoD rejects / reason
Rejected --> Marked : lecturer remarks
Moderated --> Released : HoD releases course
Released --> [*]
@enduml''',
'04_activity':'''@startuml
|Student|
start
:Log in and select enrolled assignment;
:Enter work;
|System|
if (Valid content and enrolment?) then (yes)
 :Resolve effective deadline;
 if (Previous submission?) then (yes)
  if (Before deadline, not marked?) then (yes)
   :Replace submission;
  else (no)
   :Show resubmission denied;
   stop
  endif
 else (no)
  :Insert submission as Submitted or Late;
 endif
 :Confirm receipt;
else (no)
 :Display validation error;
 stop
endif
|Lecturer|
:Review submission;
:Enter raw mark and feedback;
|System|
:Calculate penalty and final mark;
:Store mark and audit event;
|HoD|
:Review all departmental marks;
if (Accept?) then (yes)
 :Moderate marks (reason mandatory);
 |System|
 :Write audit event and mark Moderated;
 |HoD|
 :Release when all recorded course marks moderated;
 |System|
 :Mark Released;
 |Student|
 :View final mark and feedback;
 stop
else (no)
 :Reject for re-marking;
 |Lecturer|
 :Re-mark and send back for HoD review;
 |HoD|
 :Review revised mark;
 stop
endif
@enduml''',
'05_sequence_submit':'''@startuml
actor Student
participant ConsoleUI
participant LmsService
participant SubmissionRepository
participant Database
Student -> ConsoleUI: submit(assignment, content)
ConsoleUI -> LmsService: submit(student, assignment, text, timestamp)
LmsService -> Database: check user role, enrolment; resolve deadline
Database --> LmsService: eligible, effective deadline
alt empty text or not enrolled
 LmsService --> ConsoleUI: Error (no mutation)
else existing submission
 LmsService -> SubmissionRepository: get(assignment,student)
 alt before effective deadline and status Submitted
  LmsService -> SubmissionRepository: replace(content, timestamp, Submitted)
 else late / already marked
  LmsService --> ConsoleUI: Error: resubmit not allowed
 end
else first submission
 LmsService -> SubmissionRepository: insert(content, timestamp, Submitted/Late)
 SubmissionRepository -> Database: prepared INSERT
 Database --> SubmissionRepository: id
end
LmsService --> ConsoleUI: submission id
ConsoleUI --> Student: confirmation / error
@enduml''',
'06_sequence_moderate':'''@startuml
actor HoD
participant ConsoleUI
participant LmsService
participant Database
participant StandardLatePenalty
participant AuditService
actor Student
HoD -> ConsoleUI: moderate(submission,revisedRaw,reason)
ConsoleUI -> LmsService: moderate(hod,...)
LmsService -> Database: verify HoD department; load Marked row
alt role invalid, missing reason, wrong state
 LmsService --> ConsoleUI: Error (unchanged)
else permitted
 LmsService -> StandardLatePenalty: finalMark(revisedRaw,max,lateSeconds)
 StandardLatePenalty --> LmsService: revised final
 LmsService -> Database: BEGIN IMMEDIATE; UPDATE mark; status Moderated
 LmsService -> AuditService: onMarkChange(old,new,who,when,why)
 AuditService -> Database: INSERT audit_event
 LmsService -> Database: COMMIT
end
HoD -> ConsoleUI: release(course)
ConsoleUI -> LmsService: releaseCourse(hod,course)
LmsService -> Database: assert no Marked or Rejected rows; UPDATE Moderated -> Released
Database --> LmsService: released count
Student -> ConsoleUI: results
ConsoleUI -> LmsService: releasedMark(student,assignment)
LmsService -> Database: SELECT ... WHERE status='Released' AND student_id=?
Database --> Student: released mark and feedback only
@enduml''',
'07_erd':'''@startuml
hide circle
skinparam linetype ortho
entity DEPARTMENT { * department_id : INTEGER <<PK>>\n--\n name : TEXT <<UQ>> }
entity USERS { * user_id : INTEGER <<PK>>\n--\n username : TEXT <<UQ>>\n password_hash : TEXT\n role : TEXT\n dept_id : INTEGER <<FK>>\n active : BOOL }
entity COURSE { * course_id : INTEGER <<PK>>\n--\n code : TEXT <<UQ>>\n title : TEXT\n dept_id : INTEGER <<FK>> }
entity COURSE_LECTURER { * course_id : INTEGER <<PK,FK>>\n * lecturer_id : INTEGER <<PK,FK>> }
entity ENROLMENT { * course_id : INTEGER <<PK,FK>>\n * student_id : INTEGER <<PK,FK>> }
entity ASSIGNMENT { * assignment_id : INTEGER <<PK>>\n--\n course_id : INTEGER <<FK>>\n title : TEXT\n deadline : INTEGER\n max_mark : REAL\n created_by : INTEGER <<FK>> }
entity EXTENSION { * extension_id : INTEGER <<PK>>\n--\n assignment_id : INTEGER <<FK>>\n student_id : INTEGER <<FK>>\n new_deadline : INTEGER\n reason : TEXT\n granted_by : INTEGER <<FK>> }
entity CLASS_EXTENSION { * assignment_id : INTEGER <<PK,FK>>\n--\n new_deadline : INTEGER\n reason : TEXT\n granted_by : INTEGER <<FK>>\n updated_at : INTEGER }
entity SUBMISSION { * submission_id : INTEGER <<PK>>\n--\n assignment_id : INTEGER <<FK>>\n student_id : INTEGER <<FK>>\n content : TEXT\n submitted_at : INTEGER\n status : TEXT }
entity MARK { * mark_id : INTEGER <<PK>>\n--\n submission_id : INTEGER <<FK,UQ>>\n raw_mark : REAL\n final_mark : REAL\n feedback : TEXT\n marked_by : INTEGER <<FK>>\n moderated_by : INTEGER <<FK>> }
entity AUDIT_EVENT { * audit_id : INTEGER <<PK>>\n--\n mark_id : INTEGER <<FK>>\n actor_id : INTEGER <<FK>>\n changed_at : INTEGER\n old_value : REAL NULL\n new_value : REAL\n reason : TEXT }
DEPARTMENT ||--o{ COURSE
DEPARTMENT |o--o{ USERS
COURSE ||--o{ ASSIGNMENT
COURSE ||--o{ COURSE_LECTURER
USERS ||--o{ COURSE_LECTURER
COURSE ||--o{ ENROLMENT
USERS ||--o{ ENROLMENT
ASSIGNMENT ||--o{ EXTENSION
ASSIGNMENT ||--o| CLASS_EXTENSION
USERS ||--o{ EXTENSION
USERS ||--o{ SUBMISSION
ASSIGNMENT ||--o{ SUBMISSION
SUBMISSION ||--o| MARK
MARK ||--o{ AUDIT_EVENT
@enduml''',
'08_deployment':'''@startuml
node "School computer / Ubuntu or Windows" as HOST {
 artifact "lms console executable" as EXE
 node "local process" as PROC {
  component "Console presentation" as UI
  component "Business services + policies" as BL
  component "SQLite data-access / prepared SQL" as DA
 }
 database "school.db on protected local disk" as SQL
 folder "offline backup (access restricted)" as BK
 UI --> BL
 BL --> DA
 DA --> SQL
 SQL ..> BK : technician backup
}
actor "Administrator / Lecturer / HoD / Student" as PERSON
PERSON --> EXE : local terminal
EXE --> PROC
note right of HOST
No cloud, no web/mobile,
no online dependence;
single host deployment.
end note
@enduml'''
}
for name,value in puml.items(): (dir/f'{name}.puml').write_text(value+'\n')
# Dot fallback previews for documents when PlantUML is not installed.
dots={
'01_component':'''digraph{graph[rankdir=LR,label="Layered Component Architecture",labelloc=t,fontsize=20,pad=.2,nodesep=.3,ranksep=.8];node[shape=box,style="rounded,filled",fillcolor="#edf3f9",color="#427097",fontname=Helvetica]; actors[label="Four user roles"];ui[label="Presentation\nConsoleUI (main.cpp)"];bl[label="Business Logic\nLmsService + Policy\nUserFactory, Observer"];da[label="Data Access\nSubmissionRepository\nDatabase/Statement"];db[label="SQLite\nschool.db",shape=cylinder,fillcolor="#e7f5ee"];actors->ui->bl->da->db;}''',
'02_class':'''digraph{graph[label="Design-level Classes, Interfaces & Dependencies",labelloc=t,fontsize=19,rankdir=LR,pad=.2,nodesep=.32,ranksep=.85];node[shape=record,fontname=Helvetica,style=filled,fillcolor="#eef3fa",color="#6c89a7",fontsize=10];User[label="{Abstract User|#id_:int; #username_:string|+role():string (virtual)}"];roles[label="{Student / Lecturer / HoD / Administrator|+role():string}"];Factory[label="{UserFactory|+create(...):unique_ptr<User>}"];Service[label="{LmsService|-Database; -Policy; -Audit; -Repository|+submit(); +mark(); +moderate(); +classExtension(); +releaseCourse()}"];Strategy[label="{Interface LatePenaltyStrategy|+finalMark(raw,max,secondsLate):double}"];Penalty[label="{StandardLatePenalty|+finalMark(...):double}"];Observer[label="{Interface MarkObserver|+onMarkChange(...):void}"];Audit[label="{AuditService|+onMarkChange(...):void}"];Notice[label="{NotificationService|+onMarkChange(...):void}"];Repository[label="{SubmissionRepository|+get(); +insert(); +replace()}"];Database[label="{Database / Statement|+exec(); +bind(); +next()}"];roles->User[arrowhead=empty]; Factory->User[style=dashed];Service->Factory;Service->Penalty;Service->Audit;Service->Repository;Penalty->Strategy[arrowhead=empty];Audit->Observer[arrowhead=empty];Notice->Observer[arrowhead=empty];Repository->Database;Audit->Database;}''',
'03_state':'''digraph{graph[label="Submission Lifecycle",labelloc=t,rankdir=LR,fontsize=19,nodesep=.35,ranksep=.55];node[shape=box,style="rounded,filled",fillcolor="#edf3f9",color="#427097"];Created;Submitted;Late;Marked;Moderated;Released[fillcolor="#e7f5ee"];Rejected[fillcolor="#faeded"];Created->Submitted[label="on time"];Created->Late[label="late"];Submitted->Submitted[label="resubmit before due"];Submitted->Marked;Late->Marked;Marked->Moderated[label="HoD approves"];Marked->Rejected[label="HoD rejects"];Rejected->Marked[label="re-mark"];Moderated->Released[label="HoD releases"];}''',
'04_activity':'''digraph{graph[label="End-to-end Workflow with Roles",labelloc=t,fontsize=18,rankdir=TB,nodesep=.25,ranksep=.35];node[shape=box,style="rounded,filled",fontname=Helvetica,fontsize=11,color="#427097",fillcolor="#edf3f9"];a[label="STUDENT\nSelect assignment + submit"];b[label="SYSTEM\nValidate role, enrolment, deadline"];c[label="SYSTEM\nPersist Submitted/Late or replace resubmission"];d[label="LECTURER\nMark + written feedback"];e[label="SYSTEM\nApply penalty + audit mark change"];f[label="HoD\nReview / adjust with reason"];g[label="SYSTEM\nAudit moderation; set Moderated"];h[label="HoD\nRelease only if all recorded marks moderated"];i[label="STUDENT\nView released mark + feedback",fillcolor="#e7f5ee"];j[label="LECTURER\nRe-mark rejected submission",fillcolor="#faeded"];a->b->c->d->e->f;f->g[label="approve"];f->j[label="reject"];j->f[label="re-review"];g->h->i;}''',
'05_sequence_submit':'''digraph{graph[label="Submit Assignment - Sequence of Calls",labelloc=t,rankdir=TB,fontsize=18,nodesep=.25,ranksep=.4];node[shape=box,style="rounded,filled",color="#427097",fillcolor="#edf3f9",fontsize=11];a[label="1 Student → ConsoleUI\nsubmit assignment + content"];b[label="2 ConsoleUI → LmsService\nsubmit(student, assignment, content, now)"];c[label="3 LmsService → Database\ncheck role/enrolment; effective deadline"];d[label="4 LmsService → SubmissionRepository\nget existing submission"];e[label="5a New → INSERT Submitted/Late"];f[label="5b Existing valid → UPDATE before deadline"];g[label="5c Invalid, late resubmit, wrong state → Error",fillcolor="#faeded"];h[label="6 Confirmation with submission ID",fillcolor="#e7f5ee"];a->b->c->d;d->e[label="new"];d->f[label="allowed resubmit"];d->g[label="blocked"];e->h;f->h;}''',
'06_sequence_moderate':'''digraph{graph[label="Moderate and Release - Sequence of Calls",labelloc=t,rankdir=TB,fontsize=18,nodesep=.22,ranksep=.35];node[shape=box,style="rounded,filled",color="#427097",fillcolor="#edf3f9",fontsize=11];a[label="1 HoD → ConsoleUI → LmsService\nmoderate(submission, raw, reason)"];b[label="2 LmsService → Database\ncheck HoD dept and Marked state"];c[label="3 LmsService → StandardLatePenalty\nrecompute final mark"];d[label="4 LmsService → Database\nBEGIN + UPDATE Mark/Submission"];e[label="5 LmsService → AuditService\nINSERT who/when/old/new/reason"];f[label="6 Database COMMIT"];g[label="7 HoD → LmsService\nreleaseCourse: require all Marked/Rejected cleared"];h[label="8 Database → Student\nReleased mark + feedback only",fillcolor="#e7f5ee"];a->b->c->d->e->f->g->h;}''',
'07_erd':'''digraph{graph[label="3NF Relational ERD (PK / FK and Cardinality)",labelloc=t,rankdir=LR,fontsize=19,nodesep=.3,ranksep=1.1,splines=polyline];node[shape=record,fontname=Helvetica,style=filled,fillcolor="#edf3f9",color="#427097",fontsize=10];department[label="{DEPARTMENT|PK department_id\lname UQ\l}"];users[label="{USERS|PK user_id\lusername UQ, password_hash\lrole, dept_id FK, active\l}"];course[label="{COURSE|PK course_id\lcode UQ, title, dept_id FK\l}"];cl[label="{COURSE_LECTURER|PK (course_id,lecturer_id)\lFK course_id, lecturer_id\l}"];en[label="{ENROLMENT|PK (course_id,student_id)\lFK course_id, student_id\l}"];assignment[label="{ASSIGNMENT|PK assignment_id\lFK course_id, created_by\ltitle, deadline, max_mark\l}"];extension[label="{EXTENSION|PK extension_id\lFK assignment_id, student_id, granted_by\lnew_deadline, reason\l}"];class[label="{CLASS_EXTENSION (CR1)|PK/FK assignment_id\lnew_deadline, reason\lgranted_by FK, updated_at\l}",fillcolor="#e7f5ee"];submission[label="{SUBMISSION|PK submission_id\lFK assignment_id, student_id\lcontent, submitted_at, status\l}"];mark[label="{MARK|PK mark_id\lFK/UQ submission_id\lraw_mark, final_mark, feedback\lmarked_by, moderated_by FK\l}"];audit[label="{AUDIT_EVENT|PK audit_id\lFK mark_id, actor_id\lchanged_at, old_value, new_value, reason\l}"];department->course[label="1:N"];department->users[label="1:N"];course->cl[label="1:N"];users->cl;course->en;users->en;course->assignment[label="1:N"];assignment->extension;assignment->class[label="1:0..1"];assignment->submission;users->submission;submission->mark[label="1:0..1"];mark->audit[label="1:N"]; }''',
'08_deployment':'''digraph{graph[label="Single-Host Offline Deployment",labelloc=t,rankdir=LR,fontsize=19,nodesep=.4,ranksep=.6];node[shape=box,style="rounded,filled",fontname=Helvetica,fillcolor="#edf3f9",color="#427097"];people[label="Four roles\nLocal terminal"];executable[label="School workstation\nC++17 console executable"];process[label="Business rules + SQLite DAL"];database[label="Protected local disk\nschool.db",shape=cylinder,fillcolor="#e7f5ee"];backup[label="Technician backup\nOffline, access-controlled"];people->executable->process->database->backup[style=dashed];}'''
}
for name,value in dots.items():
 dotpath=dir/f'{name}.dot';dotpath.write_text(value+'\n')
 subprocess.run(['dot','-Tpng','-Gdpi=145',str(dotpath),'-o',str(dir/f'{name}.png')],check=True)
print('Produced',len(dots),'diagrams + editable PlantUML files')
