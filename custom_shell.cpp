#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

// Simple job states
enum JobState { RUNNING, STOPPED, DONE };

struct Job {
    int id;
    pid_t pgid;            // process group id
    string cmdline;
    JobState state;
};

static vector<Job> jobs;
static int next_job_id = 1;
static pid_t shell_pgid;

// Utility: trim
static inline string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    if (a==string::npos) return "";
    size_t b = s.find_last_not_of(" \t\n\r");
    return s.substr(a, b-a+1);
}

// Split by whitespace but keep >,>>,<,|,& as tokens
vector<string> tokenize(const string &line) {
    vector<string> toks;
    string cur;
    for (size_t i=0;i<line.size();){
        if (isspace((unsigned char)line[i])) { ++i; continue; }
        // multi-char tokens: >>
        if (line[i]=='>' && i+1<line.size() && line[i+1]=='>') { toks.push_back(">>"); i+=2; continue; }
        if (strchr("<>|&", line[i])) { toks.push_back(string(1,line[i])); ++i; continue; }
        // normal word
        cur.clear();
        while (i<line.size() && !isspace((unsigned char)line[i]) && !strchr("<>|&", line[i])){
            cur.push_back(line[i]); ++i;
        }
        toks.push_back(cur);
    }
    return toks;
}

// Command structure for pipeline
struct Command {
    vector<char*> argv;    // nullptr-terminated
    string infile;         // if not empty -> redirect from
    string outfile;        // if not empty -> redirect to
    bool append = false;   // >> if true
};

// Parse tokens into a list of Command representing a pipeline, and whether background
pair<vector<Command>, bool> parse_commands(const vector<string>& toks) {
    vector<Command> cmds;
    cmds.emplace_back();
    bool background = false;
    for (size_t i=0;i<toks.size();++i){
        const string &t = toks[i];
        if (t=="|") {
            cmds.emplace_back();
            continue;
        } else if (t=="<"){
            if (i+1 < toks.size()) { cmds.back().infile = toks[++i]; }
        } else if (t==">"){
            if (i+1 < toks.size()) { cmds.back().outfile = toks[++i]; cmds.back().append = false; }
        } else if (t==">>"){
            if (i+1 < toks.size()) { cmds.back().outfile = toks[++i]; cmds.back().append = true; }
        } else if (t=="&"){
            // only meaningful at end; support if last token
            if (i==toks.size()-1) background = true;
        } else {
            // normal arg
            cmds.back().argv.push_back(strdup(t.c_str()));
        }
    }
    // null terminate argv
    for (auto &c: cmds) { c.argv.push_back(nullptr); }
    return {cmds, background};
}

// Job control helpers
int add_job(pid_t pgid, const string &cmdline, JobState st) {
    Job j; j.id = next_job_id++; j.pgid = pgid; j.cmdline = cmdline; j.state = st;
    jobs.push_back(j);
    return j.id;
}

void remove_done_jobs() {
    jobs.erase(remove_if(jobs.begin(), jobs.end(), [](const Job &j){ return j.state==DONE; }), jobs.end());
}

Job* find_job_by_pgid(pid_t pgid){
    for (auto &j: jobs) if (j.pgid==pgid) return &j; return nullptr;
}

Job* find_job_by_id(int id){
    for (auto &j: jobs) if (j.id==id) return &j; return nullptr;
}

void print_jobs(){
    remove_done_jobs();
    for (auto &j: jobs){
        cout << '['<<j.id<<"] ";
        if (j.state==RUNNING) cout << "Running ";
        else if (j.state==STOPPED) cout << "Stopped ";
        else cout << "Done ";
        cout << j.cmdline << " (pgid="<<j.pgid<<")"<<"\n";
    }
}

// Signal handlers
void sigchld_handler(int){
    int saved_errno = errno;
    pid_t pid; int status;
    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
        pid_t pgid = getpgid(pid);
        Job *j = find_job_by_pgid(pgid);
        if (!j) continue;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            j->state = DONE;
        } else if (WIFSTOPPED(status)) {
            j->state = STOPPED;
        } else if (WIFCONTINUED(status)) {
            j->state = RUNNING;
        }
    }
    errno = saved_errno;
}

void install_signal_handlers(){
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, nullptr);
    // Ignore SIGTTOU, SIGTTIN in shell so tcsetpgrp works
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

// Builtin commands
bool is_builtin(const string &cmd){
    return cmd=="cd" || cmd=="exit" || cmd=="jobs" || cmd=="fg" || cmd=="bg";
}

int run_builtin(vector<string> argv){
    if (argv.empty()) return 0;
    const string &cmd = argv[0];
    if (cmd=="cd"){
        const char *path = (argv.size()>1? argv[1].c_str(): getenv("HOME"));
        if (chdir(path)!=0) perror("cd");
        return 0;
    } else if (cmd=="exit"){
        exit(0);
    } else if (cmd=="jobs"){
        print_jobs();
        return 0;
    } else if (cmd=="fg"){
        if (argv.size()<2) { cout<<"Usage: fg <jobid>\n"; return 0; }
        int id = stoi(argv[1]);
        Job *j = find_job_by_id(id);
        if (!j) { cout<<"No such job\n"; return 0; }
        // put job in foreground
        tcsetpgrp(STDIN_FILENO, j->pgid);
        if (kill(-j->pgid, SIGCONT) < 0) perror("kill");
        j->state = RUNNING;
        // wait for it
        int status; waitpid(-j->pgid, &status, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        return 0;
    } else if (cmd=="bg"){
        if (argv.size()<2) { cout<<"Usage: bg <jobid>\n"; return 0; }
        int id = stoi(argv[1]);
        Job *j = find_job_by_id(id);
        if (!j) { cout<<"No such job\n"; return 0; }
        if (kill(-j->pgid, SIGCONT) < 0) perror("kill");
        j->state = RUNNING;
        return 0;
    }
    return 0;
}

// execute a pipeline of commands
void execute_pipeline(vector<Command> &cmds, bool background, const string &cmdline) {
    size_t n = cmds.size();
    vector<int> pipes; // will store fds flattened
    for (size_t i=0;i+1<n;++i){ int f[2]; if (pipe(f)==-1){ perror("pipe"); return; } pipes.push_back(f[0]); pipes.push_back(f[1]); }

    pid_t pgid = 0;
    vector<pid_t> pids;
    for (size_t i=0;i<n;++i){
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        if (pid==0){
            // child
            // set pgid
            if (i==0) pgid = getpid();
            setpgid(0, pgid);
            if (!background) tcsetpgrp(STDIN_FILENO, pgid);

            // setup pipes
            if (i>0){ // read end of previous
                int read_fd = pipes[(i-1)*2];
                dup2(read_fd, STDIN_FILENO);
            }
            if (i+1<n){ // write end for current
                int write_fd = pipes[i*2 + 1];
                dup2(write_fd, STDOUT_FILENO);
            }
            // close all pipes
            for (int fd: pipes) close(fd);

            // redirection
            if (!cmds[i].infile.empty()){
                int in = open(cmds[i].infile.c_str(), O_RDONLY);
                if (in<0){ perror("open infile"); exit(1);} dup2(in, STDIN_FILENO); close(in);
            }
            if (!cmds[i].outfile.empty()){
                int flags = O_WRONLY | O_CREAT | (cmds[i].append? O_APPEND : O_TRUNC);
                int out = open(cmds[i].outfile.c_str(), flags, 0644);
                if (out<0){ perror("open outfile"); exit(1);} dup2(out, STDOUT_FILENO); close(out);
            }

            // build argv
            if (cmds[i].argv.empty() || cmds[i].argv[0]==nullptr) exit(0);
            string first = cmds[i].argv[0];
            if (is_builtin(first)){
                // builtins in pipelines - execute in child
                vector<string> a;
                for (char **p = cmds[i].argv.data(); *p; ++p) a.push_back(string(*p));
                run_builtin(a);
                exit(0);
            }
            // allow default signals
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            execvp(cmds[i].argv[0], cmds[i].argv.data());
            perror("execvp");
            exit(1);
        } else {
            // parent
            if (i==0) pgid = pid;
            setpgid(pid, pgid);
            pids.push_back(pid);
        }
    }
    // parent closes pipes
    for (int fd: pipes) close(fd);

    // add job
    int jid = add_job(pgid, cmdline, RUNNING);
    if (!background){
        // put in foreground
        tcsetpgrp(STDIN_FILENO, pgid);
        // wait for the process group
        int status;
        // wait for any in group
        while (true){
            pid_t w = waitpid(-pgid, &status, WUNTRACED);
            if (w==-1) break;
            if (WIFSTOPPED(status)){
                Job *j = find_job_by_pgid(pgid);
                if (j) j->state = STOPPED;
                break;
            }
            // when all exit, loop will get -1
            if (WIFEXITED(status) || WIFSIGNALED(status)){
                // continue waiting until no more
                // we rely on SIGCHLD handler to mark DONE
            }
        }
        // restore shell as foreground
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    } else {
        cout << "["<<jid<<"] "<<pgid<<"\n";
    }
}

int main(){
    // make shell's own pgid and take control of terminal
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    install_signal_handlers();

    string line;
    while (true){
        // prompt
        char cwd[1024]; getcwd(cwd, sizeof(cwd));
        cout << "myshell:" << cwd << "$ ";
        if (!getline(cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;

        auto toks = tokenize(line);
        auto parsed = parse_commands(toks);
        auto cmds = parsed.first;
        bool background = parsed.second;

        // handle simple single builtin without forking when not in pipeline and not background and no redir
        if (cmds.size()==1 && !background && !cmds[0].infile.size() && !cmds[0].outfile.size()){
            if (cmds[0].argv.size()>1 && cmds[0].argv[0]!=nullptr){
                string first = cmds[0].argv[0];
                if (is_builtin(first)){
                    vector<string> a; for (char**p=cmds[0].argv.data(); *p; ++p) a.emplace_back(*p);
                    run_builtin(a);
                    continue;
                }
            }
        }

        execute_pipeline(cmds, background, line);
        remove_done_jobs();
    }
    return 0;
}
