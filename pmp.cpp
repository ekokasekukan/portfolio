#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>

static volatile std::sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

class gdbinfo {
public:
    pid_t gdb_pid = -1;
    int   to_gdb   = -1;
    int   from_gdb = -1;
    std::string buf;
    bool start(const std::string& gdb_path, int target_pid) {
        int in_pipe[2], out_pipe[2];
        if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) return false;
        gdb_pid = fork();
        if (gdb_pid < 0) return false;
        if (gdb_pid == 0) {
            dup2(in_pipe[0], 0);
            dup2(out_pipe[1], 1);
            dup2(out_pipe[1], 2);
            close(in_pipe[0]); close(in_pipe[1]);
            close(out_pipe[0]); close(out_pipe[1]);
            std::string pid_s = std::to_string(target_pid);
            execlp(gdb_path.c_str(), gdb_path.c_str(),
                   "--interpreter=mi2", "-q", "-p", pid_s.c_str(),(char*)nullptr);
            _exit(127);}
        close(in_pipe[0]);
        close(out_pipe[1]);
        to_gdb   = in_pipe[1];
        from_gdb = out_pipe[0];
        if (!readUntilPrompt(3000)) return false;
        return true;}
    void send(const std::string& cmd) {
        std::string line = cmd + "\n";
        ssize_t r = write(to_gdb, line.c_str(), line.size());
        (void)r;}
    bool readLine(std::string& out, int timeout_ms = 3000) {
        out.clear();
        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto pos = buf.find('\n');
            if (pos != std::string::npos) {
                out = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                return true;}
            char tmp[4096];
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(from_gdb, &fds);
            timeval tv{0, 100000};
            int r = select(from_gdb + 1, &fds, nullptr, nullptr, &tv);
            if (r > 0) {
                ssize_t n = read(from_gdb, tmp, sizeof(tmp));
                if (n <= 0) return false;
                buf.append(tmp, n);} 
            else {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start).count();
                if (ms > timeout_ms) return false;}}}
    bool readUntilPrompt(int timeout_ms = 3000) {
        std::string line;
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (!readLine(line, 500)) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start).count();
                if (ms > timeout_ms) return false;
                continue;}
            if (line.find("(gdb)") != std::string::npos) return true;}}
    void stop() {
        if (to_gdb >= 0) { send("-gdb-exit"); close(to_gdb); to_gdb = -1; }
        if (from_gdb >= 0) close(from_gdb);
        if (gdb_pid > 0) { kill(gdb_pid, SIGTERM); waitpid(gdb_pid, nullptr, 0); }}};

static std::string unescapeMi(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[i + 1];
            if (c == 'n') { out += '\n'; ++i; }
            else if (c == 't') { out += '\t'; ++i; }
            else if (c == '"') { out += '"'; ++i; }
            else if (c == '\\') { out += '\\'; ++i; }
            else out += s[i];}
            else out += s[i];}
    return out;}

static std::string extractFunc(const std::string& frame) {
    auto pos = frame.find(" in ");
    if (pos != std::string::npos) {
        std::string rest = frame.substr(pos + 4);
        if (rest.rfind("??", 0) == 0) return "[unknown]";
        auto end_pos = rest.find_first_of(" (");
        std::string func = (end_pos == std::string::npos) ? rest : rest.substr(0, end_pos);
        while (!func.empty() && (func.back() == ' ' || func.back() == '\r' || func.back() == '\n'))
            func.pop_back();
        if (!func.empty()) return func;}
    auto hash_pos = frame.find('#');
    if (hash_pos != std::string::npos) {
        auto space_pos = frame.find(' ', hash_pos);
        if (space_pos != std::string::npos) {
            std::string rest = frame.substr(space_pos + 1);
            if (rest.rfind("0x", 0) == 0) {
                auto next_space = rest.find(' ');
                if (next_space != std::string::npos) {
                    rest = rest.substr(next_space + 1);
                    if (rest.rfind("in ", 0) == 0) rest = rest.substr(3);}}
            auto paren = rest.find(" (");
            if (paren != std::string::npos) {
                std::string func = rest.substr(0, paren);
                // Убираем мусор
                while (!func.empty() && (func.back() == ' ' || func.back() == '\r' || func.back() == '\n'))
                    func.pop_back();
                if (!func.empty() && func != "??") return func;}}}
    return "[unknown]";}

struct SampleResult {std::vector<std::vector<std::string>> stacks;};

SampleResult takeSample(gdbinfo & gdb) {
    SampleResult res;
    gdb.send("-interpreter-exec console \"thread apply all bt 50\"");
    std::string line;
    std::vector<std::string> current_frames;
    bool in_thread = false;
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        if (!gdb.readLine(line, 500)) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start).count();
            if (ms > 4000) break;
            continue;}
        if (line.find("(gdb)") != std::string::npos) break;
        if (line.empty()) continue;
        std::string text = line;
        if (text[0] == '~') {
            if (text.size() > 2 && text[1] == '"') text = text.substr(2);
            if (!text.empty() && text.back() == '"') text.pop_back();
            text = unescapeMi(text);} 
        else if (text[0] == '^' || text[0] == '*' || text[0] == '=') {
            continue;} 
        else if (text[0] == '&') {
            continue;}
        std::istringstream iss(text);
        std::string sub;
        while (std::getline(iss, sub)) {
            if (sub.empty()) continue;
            if (sub.rfind("Thread ", 0) == 0) {
                if (!current_frames.empty()) {
                    std::reverse(current_frames.begin(), current_frames.end());
                    res.stacks.push_back(current_frames);
                    current_frames.clear();}
                in_thread = true;
                continue;}
            if (sub[0] == '#' && in_thread) {
                std::string func = extractFunc(sub);
                if (!func.empty()) current_frames.push_back(func);}}}
    if (!current_frames.empty()) {
        std::reverse(current_frames.begin(), current_frames.end());
        res.stacks.push_back(current_frames);}
    return res;}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0]
                  << " <PID> [-d сек] [-i мс] [-o out.collapsed] [--gdb путь]\n";
        return 1;}
    int pid = std::atoi(argv[1]);
    double duration_s = 10.0;
    int    interval_ms = 100;
    std::string out_file = "profile.collapsed";
    std::string gdb_path = "gdb";
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-d" && i + 1 < argc) duration_s = std::atof(argv[++i]);
        else if (a == "-i" && i + 1 < argc) interval_ms = std::atoi(argv[++i]);
        else if (a == "-o" && i + 1 < argc) out_file = argv[++i];
        else if (a == "--gdb" && i + 1 < argc) gdb_path = argv[++i];}
    std::signal(SIGINT, on_sigint);
    std::cout << "[pmp] PID=" << pid
              << " duration=" << duration_s << "s"
              << " interval=" << interval_ms << "ms"
              << " gdb=" << gdb_path << "\n";
    gdbinfo gdb;
    if (!gdb.start(gdb_path, pid)) {
        std::cerr << "[pmp] Не удалось запустить GDB. Проверь путь и ptrace_scope.\n";
        return 1;}
    std::cout << "[pmp] GDB подключён\n";
    std::map<std::string, int> counter;
    int samples = 0;
    auto t_end = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds((long)(duration_s * 1000));
    while (!g_stop && std::chrono::steady_clock::now() < t_end) {
        auto t0 = std::chrono::steady_clock::now();
        SampleResult sr = takeSample(gdb);
        for (auto& stack : sr.stacks) {
            std::string key;
            for (size_t i = 0; i < stack.size(); ++i) {
                if (i) key += ";";
                key += stack[i];}
            counter[key]++;}
        samples += (int)sr.stacks.size();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - t0).count();
        if (elapsed < interval_ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms - elapsed));}
    gdb.stop();
    std::cout << "[pmp] Собрано стеков: " << samples << "\n";
    std::ofstream out(out_file);
    if (!out) {
        std::cerr << "[pmp] Не могу открыть " << out_file << "\n";
        return 1;}
    for (auto& kv : counter) out << kv.first << " " << kv.second << "\n";
    out.close();
    std::cout << "[pmp] Collapsed-файл: " << out_file << "\n";
    std::cout << "[pmp] Построй график:\n"
              << "      FlameGraph/flamegraph.pl " << out_file
              << " > flame.svg\n";
    return 0;}