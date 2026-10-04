/* Optional recovery hints, never a login credential or recent identity proof.
 * Answers are independently salted like passwords. Only a human-reviewed
 * recovery request can lead to an administrator resetting the password. */
static const char XA_SecuritySchema1[] =
    "CREATE TABLE member_security_question(member_id INTEGER PRIMARY KEY REFERENCES member(id),revision TEXT NOT NULL,q1 INTEGER NOT NULL,a1 TEXT NOT NULL,q2 INTEGER NOT NULL,a2 TEXT NOT NULL,q3 INTEGER NOT NULL,a3 TEXT NOT NULL,updated_at INTEGER NOT NULL);"
    "CREATE TABLE member_security_recovery(id TEXT PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES member(id),revision TEXT NOT NULL,created_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,closed_at INTEGER NOT NULL DEFAULT 0,admin_user TEXT NOT NULL DEFAULT '',resolution TEXT NOT NULL DEFAULT '',reason TEXT NOT NULL DEFAULT '');"
    "CREATE INDEX member_security_recovery_owner ON member_security_recovery(member_id,closed_at,expires_at);"
    "CREATE TABLE member_security_review(id INTEGER PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES member(id),request_id TEXT NOT NULL,action TEXT NOT NULL,admin_user TEXT NOT NULL,reason TEXT NOT NULL,created_at INTEGER NOT NULL);";

static const char* const XA_SecurityQuestions[] = {
    "你记得最清楚的一次学校旅行去了哪里？",
    "你童年最珍爱的玩具叫什么？",
    "你曾申请但没有就读的学校叫什么？",
    "你第一次独自完成的作品叫什么？",
    "你小时候给某件私人物品起过什么名字？",
    "你和朋友约定过的一个秘密代号是什么？",
    "你第一次参加的课外活动叫什么？",
    "你记忆中的一位启蒙老师叫什么？"
};
#define XA_SECURITY_QUESTION_COUNT (sizeof(XA_SecurityQuestions)/sizeof(XA_SecurityQuestions[0]))
typedef struct XASecurityAnswers {
    int question[3];
    char answer[3][129];
} XASecurityAnswers;
typedef struct XASecuritySnapshot {
    char revision[65],record[3][257];
    int question[3];
    int64 updated;
} XASecuritySnapshot;

/* Trim ASCII surrounding whitespace and fold ASCII case, consistently on
 * setup and verification. UTF-8 non-ASCII bytes remain significant. */
static bool XA_SecurityAnswer(const char* input,char out[129])
{
    if(!input)return false;
    size_t start=0,end=strlen(input),i,n;
    while(start<end&&(input[start]==' '||input[start]=='\t'||input[start]=='\r'||input[start]=='\n'))start++;
    while(end>start&&(input[end-1]==' '||input[end-1]=='\t'||input[end-1]=='\r'||input[end-1]=='\n'))end--;
    n=end-start;if(n<4||n>128)return false;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)input[start+i];
        if(c<32||c==127)return false;
        out[i]=c>='A'&&c<='Z'?(char)(c+32):(char)c;
    }
    out[n]=0;return true;
}
static bool XA_SecurityParse(xvalue* body,XASecurityAnswers* answers)
{
    int i,j;memset(answers,0,sizeof(*answers));
    for(i=0;i<3;i++){
        char field[16];int64 question=0;
        snprintf(field,sizeof(field),"question%d",i+1);
        if(!xrtValueGetInt(ValueGet(body,field),&question)||question<1||question>XA_SECURITY_QUESTION_COUNT)return false;
        answers->question[i]=(int)question;
        snprintf(field,sizeof(field),"answer%d",i+1);
        if(!XA_SecurityAnswer(XA_Text(body,field,128),answers->answer[i]))return false;
        for(j=0;j<i;j++)if(answers->question[i]==answers->question[j]||!strcmp(answers->answer[i],answers->answer[j]))return false;
    }
    return true;
}
static int XA_SecurityRead(int64 owner,XASecuritySnapshot* snapshot)
{
    memset(snapshot,0,sizeof(*snapshot));
    sqlite3_stmt* s=XA_SQL("SELECT revision,q1,a1,q2,a2,q3,a3,updated_at FROM member_security_question WHERE member_id=?");
    if(s)sqlite3_bind_int64(s,1,owner);
    int rc=s?sqlite3_step(s):SQLITE_ERROR,result=rc==SQLITE_DONE?0:-1;
    if(rc==SQLITE_ROW){
        bool ok=XA_CopyColumn(s,0,snapshot->revision,sizeof(snapshot->revision));int i;
        for(i=0;i<3;i++){
            snapshot->question[i]=sqlite3_column_int(s,1+i*2);
            ok=ok&&XA_CopyColumn(s,2+i*2,snapshot->record[i],sizeof(snapshot->record[i]));
        }
        snapshot->updated=sqlite3_column_int64(s,7);result=ok?1:-1;
    }
    sqlite3_finalize(s);return result;
}
static bool XA_SecurityCurrent(int64 owner,const XASecuritySnapshot* snapshot,int found)
{
    XASecuritySnapshot current;int now=XA_SecurityRead(owner,&current);
    bool ok=found>=0&&now==found&&(found==0||!strcmp(snapshot->revision,current.revision));
    xrtSecureZero(&current,sizeof(current));return ok;
}
static bool XA_SecurityClose(int64 owner,const char* resolution,const char* admin,const char* reason)
{
    sqlite3_stmt* s=XA_SQL("UPDATE member_security_recovery SET closed_at=?,resolution=?,admin_user=?,reason=? WHERE member_id=? AND closed_at=0");
    if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,5,owner);}
    XA_BindText(s,2,resolution);XA_BindText(s,3,admin?admin:"");XA_BindText(s,4,reason?reason:"");
    return XA_Done(s,false);
}
static xvalue* XA_SecurityCatalog(void)
{
    xvalue* list=ValueArray();size_t i;
    for(i=0;i<XA_SECURITY_QUESTION_COUNT;i++){
        xvalue* item=ValueObject();ValueSetInt(item,"id",i+1);ValueSetText(item,"text",XA_SecurityQuestions[i]);ValueArrayOwn(list,item);
    }
    return list;
}
