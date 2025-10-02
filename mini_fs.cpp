// mini_fs.cpp
//#include <bits/stdc++.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <cctype>
#include <utility>

using namespace std;

struct Node : enable_shared_from_this<Node> {
    enum class Type { File, Directory };
    string name;
    weak_ptr<Node> parent;
    explicit Node(string n) : name(move(n)) {}
    virtual ~Node() = default;
    virtual Type type() const = 0;
};

struct Directory;

struct File : Node {
    static inline size_t created_count = 0; // tracks ever-created files
    string content;
    bool executable = false;
    // references to other nodes (files or directories)
    vector<weak_ptr<Node>> references;

    explicit File(string n) : Node(move(n)) { ++created_count; }
    Type type() const override { return Type::File; }
};

struct Directory : Node {
    // name -> node
    map<string, shared_ptr<Node>> entries;
    explicit Directory(string n) : Node(move(n)) {}
    Type type() const override { return Type::Directory; }

    bool has(const string& key) const { return entries.count(key) > 0; }

    void add(const shared_ptr<Node>& node) {
        entries[node->name] = node;
        node->parent = weak_ptr<Node>(static_pointer_cast<Node>(const_cast<Directory*>(this)->shared_from_this()));
    }
};

struct FileSystem {
    shared_ptr<Directory> root;
    shared_ptr<Directory> cwd;

    FileSystem() {
        root = make_shared<Directory>("/");
        root->parent.reset(); // root has no parent
        cwd = root;
    }

    // Resolve a path to a node (if mustExist), optionally return parent dir + leaf name
    struct Resolved {
        shared_ptr<Node> node;                  // may be null if !mustExist
        shared_ptr<Directory> parentDir;        // parent dir of leaf, if requested
        string leaf;                             // final component
    };

    vector<string> splitPath(const string& p) const {
        vector<string> parts;
        string cur;
        for (char c : p) {
            if (c == '/') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) parts.push_back(cur);
        return parts;
    }

    shared_ptr<Directory> asDir(const shared_ptr<Node>& n) const {
        if (!n) return nullptr;
        if (n->type() == Node::Type::Directory) return static_pointer_cast<Directory>(n);
        return nullptr;
    }

    shared_ptr<Node> resolveExisting(const string& path) const {
        if (path.empty()) return cwd;
        shared_ptr<Node> cur = (path[0] == '/') ? static_pointer_cast<Node>(root) : static_pointer_cast<Node>(cwd);
        auto parts = splitPath(path);
        for (auto& seg : parts) {
            if (seg == "." || seg.empty()) continue;
            if (seg == "..") {
                auto p = cur->parent.lock();
                if (p) cur = p;
                continue;
            }
            auto d = asDir(cur);
            if (!d || !d->has(seg)) return nullptr;
            cur = d->entries.at(seg);
        }
        return cur;
    }

    Resolved resolveForCreate(const string& path) const {
        Resolved r;
        auto parts = splitPath(path);
        if (parts.empty()) { r.node = cwd; r.parentDir = cwd; r.leaf = ""; return r; }
        shared_ptr<Node> cur = (path[0] == '/') ? static_pointer_cast<Node>(root) : static_pointer_cast<Node>(cwd);
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            const string& seg = parts[i];
            if (seg == "." || seg.empty()) continue;
            if (seg == "..") {
                auto p = cur->parent.lock();
                if (p) cur = p;
                continue;
            }
            auto d = asDir(cur);
            if (!d || !d->has(seg)) { r.node = nullptr; r.parentDir = nullptr; r.leaf = ""; return r; }
            cur = d->entries.at(seg);
        }
        r.parentDir = asDir(cur);
        r.leaf = parts.back();
        if (!r.parentDir) { r.node = nullptr; return r; }
        if (r.parentDir->has(r.leaf)) r.node = r.parentDir->entries.at(r.leaf);
        else r.node = nullptr;
        return r;
    }

    // Commands
    string pwd() const {
        // build path by walking parents
        vector<string> names;
        auto cur = static_pointer_cast<Node>(cwd);
        while (true) {
            if (cur.get() == root.get()) { names.push_back(""); break; }
            names.push_back(cur->name);
            auto p = cur->parent.lock();
            if (!p) break;
            cur = p;
        }
        reverse(names.begin(), names.end());
        string out = "/";
        for (size_t i = 1; i < names.size(); ++i) {
            out += names[i];
            if (i + 1 < names.size()) out += "/";
        }
        return out;
    }

    bool cd(const string& path, string& err) {
        auto n = resolveExisting(path);
        if (!n) { err = "cd: path not found"; return false; }
        auto d = asDir(n);
        if (!d) { err = "cd: not a directory"; return false; }
        cwd = d;
        return true;
    }

    bool mkdir(const string& path, string& err) {
        auto r = resolveForCreate(path);
        if (!r.parentDir) { err = "mkdir: parent directory does not exist"; return false; }
        if (r.node) { err = "mkdir: path exists"; return false; }
        auto d = make_shared<Directory>(r.leaf);
        r.parentDir->add(d);
        return true;
    }

    bool touch(const string& path, string& err) {
        auto r = resolveForCreate(path);
        if (!r.parentDir) { err = "touch: parent directory does not exist"; return false; }
        if (r.node) {
            if (r.node->type() != Node::Type::File) { err = "touch: exists and is not a file"; return false; }
            return true; // already exists
        }
        auto f = make_shared<File>(r.leaf);
        r.parentDir->add(f);
        return true;
    }

    bool ls(const string& path, vector<string>& out, string& err) const {
        shared_ptr<Node> n;
        if (path.empty()) n = cwd; else n = resolveExisting(path);
        if (!n) { err = "ls: path not found"; return false; }
        if (n->type() == Node::Type::File) {
            out.push_back(n->name);
            return true;
        }
        auto d = asDir(n);
        for (auto& [k, v] : d->entries) {
            string suffix = (v->type() == Node::Type::Directory) ? "/" : "";
            out.push_back(k + suffix);
        }
        return true;
    }

    bool removeNode(const string& path, bool recursive, string& err) {
        if (path == "/" || path.empty()) { err = "rm: cannot remove root"; return false; }
        auto target = resolveExisting(path);
        if (!target) { err = "rm: path not found"; return false; }
        auto p = target->parent.lock();
        auto pd = dynamic_pointer_cast<Directory>(p);
        if (!pd) { err = "rm: internal error (no parent)"; return false; }
        if (target->type() == Node::Type::Directory) {
            auto d = static_pointer_cast<Directory>(target);
            if (!recursive && !d->entries.empty()) { err = "rm: directory not empty (use rmr)"; return false; }
            if (recursive) {
                // depth-first clear
                vector<shared_ptr<Directory>> stack{d};
                while (!stack.empty()) {
                    auto curd = stack.back(); stack.pop_back();
                    for (auto it = curd->entries.begin(); it != curd->entries.end(); ) {
                        auto child = it->second;
                        if (child->type() == Node::Type::Directory) {
                            stack.push_back(static_pointer_cast<Directory>(child));
                        }
                        it = curd->entries.erase(it);
                    }
                }
            } else if (!d->entries.empty()) {
                err = "rm: directory not empty"; return false;
            }
        }
        pd->entries.erase(target->name);
        return true;
    }

    bool edit(const string& path, const string& text, string& err) {
        auto n = resolveExisting(path);
        if (!n) { err = "edit: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "edit: not a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        f->content = text;
        return true;
    }

    bool cat(const string& path, string& out, string& err) const {
        auto n = resolveExisting(path);
        if (!n) { err = "cat: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "cat: not a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        out = f->content;
        return true;
    }

    bool chmodx(const string& path, bool& newVal, string& err) {
        auto n = resolveExisting(path);
        if (!n) { err = "chmodx: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "chmodx: not a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        f->executable = !f->executable;
        newVal = f->executable;
        return true;
    }

    bool exec(const string& path, string& out, string& err) const {
        auto n = resolveExisting(path);
        if (!n) { err = "exec: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "exec: not a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        if (!f->executable) { err = "exec: permission denied (not executable)"; return false; }
        // For demo: "execute" just echos the content.
        out = ">> Running " + path + "\n" + f->content + "\n<< Exit 0";
        return true;
    }

    bool refadd(const string& filePath, const vector<string>& targets, string& err) {
        auto n = resolveExisting(filePath);
        if (!n) { err = "refadd: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "refadd: first arg must be a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        for (auto& t : targets) {
            auto tn = resolveExisting(t);
            if (!tn) { err = "refadd: target not found: " + t; return false; }
            f->references.push_back(tn);
        }
        return true;
    }

    bool refls(const string& filePath, vector<string>& out, string& err) const {
        auto n = resolveExisting(filePath);
        if (!n) { err = "refls: file not found"; return false; }
        if (n->type() != Node::Type::File) { err = "refls: not a file"; return false; }
        auto f = static_pointer_cast<File>(n);
        size_t i = 0;
        for (auto& wk : f->references) {
            auto sn = wk.lock();
            if (sn) {
                string type = (sn->type() == Node::Type::Directory) ? "dir" : "file";
                out.push_back(to_string(i++) + ": " + type + " " + sn->name);
            }
        }
        return true;
    }
};

// --- Simple shell wrapper ---
static vector<string> tokenize(const string& line) {
    // Split by spaces; allow quoted strings
    vector<string> tok;
    string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) tok.push_back(cur);
    return tok;
}

static void printHelp() {
    cout <<
R"(Commands:
  help                         - show this help
  pwd                          - print working directory
  ls [path]                    - list entries
  cd <path>                    - change directory
  mkdir <path>                 - create directory
  touch <path>                 - create file
  rm <path>                    - remove empty file/dir
  rmr <path>                   - remove recursively
  edit <file> "<text>"         - replace file contents
  cat <file>                   - print file contents
  chmodx <file>                - toggle executable flag
  exec <file>                  - "run" file (prints content)
  refadd <file> <t1> [t2...]   - add references from file to targets (file/dir)
  refls <file>                 - list references on file
  exit                         - quit
Note: Paths can be absolute (/...) or relative. Use quotes for multi-word text.)" << "\n";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(&cout);

    FileSystem fs;

    cout << "MiniFS shell. Type 'help' for commands.\n";
    if (File::created_count == 0) {
        cout << "(Hint) No files created yet. Use `touch <name>` to create one.\n";
    }

    string line;
    while (true) {
        cout << fs.pwd() << " $ ";
        if (!getline(cin, line)) break;
        auto args = tokenize(line);
        if (args.empty()) continue;
        const string& cmd = args[0];

        string err;
        if (cmd == "help") {
            printHelp();
        } else if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "pwd") {
            cout << fs.pwd() << "\n";
        } else if (cmd == "ls") {
            string p = (args.size() >= 2) ? args[1] : "";
            vector<string> out;
            if (fs.ls(p, out, err)) {
                for (auto& s : out) cout << s << "\n";
            } else {
                cout << err << "\n";
            }
        } else if (cmd == "cd") {
            if (args.size() < 2) { cout << "cd: missing operand\n"; continue; }
            if (!fs.cd(args[1], err)) cout << err << "\n";
        } else if (cmd == "mkdir") {
            if (args.size() < 2) { cout << "mkdir: missing operand\n"; continue; }
            if (!fs.mkdir(args[1], err)) cout << err << "\n";
        } else if (cmd == "touch") {
            if (args.size() < 2) { cout << "touch: missing operand\n"; continue; }
            if (!fs.touch(args[1], err)) cout << err << "\n";
            else if (File::created_count == 1) {
                cout << "(Info) First file created.\n";
            }
        } else if (cmd == "rm") {
            if (args.size() < 2) { cout << "rm: missing operand\n"; continue; }
            if (!fs.removeNode(args[1], /*recursive*/false, err)) cout << err << "\n";
        } else if (cmd == "rmr") {
            if (args.size() < 2) { cout << "rmr: missing operand\n"; continue; }
            if (!fs.removeNode(args[1], /*recursive*/true, err)) cout << err << "\n";
        } else if (cmd == "edit") {
            if (args.size() < 3) { cout << "edit: usage: edit <file> \"text\"\n"; continue; }
            // Reconstruct everything after second token as text if user omitted quotes
            size_t pos = line.find(args[1]);
            string rest = (pos == string::npos) ? "" : line.substr(pos + args[1].size());
            // Strip leading spaces
            rest.erase(rest.begin(), find_if(rest.begin(), rest.end(), [](unsigned char ch){return !isspace(ch);} ));
            if (!fs.edit(args[1], rest, err)) cout << err << "\n";
        } else if (cmd == "cat") {
            if (args.size() < 2) { cout << "cat: missing operand\n"; continue; }
            string out;
            if (fs.cat(args[1], out, err)) cout << out << "\n";
            else cout << err << "\n";
        } else if (cmd == "chmodx") {
            if (args.size() < 2) { cout << "chmodx: missing operand\n"; continue; }
            bool val=false;
            if (fs.chmodx(args[1], val, err)) cout << "executable=" << (val ? "true" : "false") << "\n";
            else cout << err << "\n";
        } else if (cmd == "exec") {
            if (args.size() < 2) { cout << "exec: missing operand\n"; continue; }
            string out;
            if (fs.exec(args[1], out, err)) cout << out << "\n";
            else cout << err << "\n";
        } else if (cmd == "refadd") {
            if (args.size() < 3) { cout << "refadd: usage: refadd <file> <target1> [target2 ...]\n"; continue; }
            vector<string> targets(args.begin()+2, args.end());
            if (!fs.refadd(args[1], targets, err)) cout << err << "\n";
        } else if (cmd == "refls") {
            if (args.size() < 2) { cout << "refls: missing operand\n"; continue; }
            vector<string> out;
            if (fs.refls(args[1], out, err)) {
                for (auto& s : out) cout << s << "\n";
                if (out.empty()) cout << "(no references)\n";
            } else cout << err << "\n";
        } else {
            cout << "Unknown command: " << cmd << " (type 'help')\n";
        }
    }

    cout << "Bye.\n";
    return 0;
}
