#include <iostream>         // cin, cout
#include <iomanip>          // setbase
#include <vector>
#include <functional>
#include <cstring>
#include <string>
#include <exception>
using namespace std;
/// .hpp - macros and class prototypes
template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t) { v.push_back(t); }
    T pop() {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    T dec_i() { return v.back() -= 1; }       /// decrement stack top, CC:
    void push(T t) { v.push_back(t); }
    void clear() { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i) { v.erase(v.begin() + i, v.end()); }	/// CC:
};
#define POP()    (top=ss.pop())
#define PUSH(v)  (ss.push(top),top=(v))
class Code;                                 /// forward declaration
typedef void (*fop)(Code*);                 /// Forth operator
/// .cpp - Code class implementation
class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd = false;                    /// immediate flag
    fop    xt = NULL;                       /// primitive function
    string literal;                         /// string literal
    int    stage = 0;                       /// branching stage
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;
    Code(string n, fop fn, bool im = false);/// primitive
    Code(string n, bool f = false);         /// new colon word or temp
    Code(string n, int d);                  /// dolit
    Code(string n, string l);               /// dostr
    Code* immediate();                      /// set immediate flag
    Code* addcode(Code* w);                 /// append colon word
    void   exec();                          /// execute word
    string to_s();                          /// debugging
};
/// Forth virtual machine variables
ForthList<int>   rs;                        /// return stack
ForthList<int>   ss;                        /// parameter stack
ForthList<Code*> dict;                      /// dictionary
bool compile = false;                       /// compiling flag
int  base = 10;                             /// numeric radix
int  top = -1;                              /// cached top of stack
int  IP, WP;                                /// instruction and parameter pointers
/// dictionary search function
Code* find(string s) {                   /// search dictionary reversely
    for (int i = dict.v.size() - 1; i >= 0; --i) {
        if (s == dict.v[i]->name) return dict.v[i];
    }
    return NULL;
}
/// constructors
int Code::fence = 0;
Code::Code(string n, fop fn, bool im) { name = n; token = fence++; xt = fn; immd = im; }
Code::Code(string n, bool f) { Code* c = find(name = n); if (c) xt = c->xt; if (f) token = fence++; }
Code::Code(string n, int d) { xt = find(name = n)->xt; qf.push(d); }
Code::Code(string n, string l) { xt = find(name = n)->xt; literal = l; }
/// public methods
Code* Code::immediate() { immd = true;  return this; }
Code* Code::addcode(Code* w) { pf.push(w); return this; }
void  Code::exec() {
    if (xt) { xt(this); return; }       /// * execute primitive word and return
    rs.push(WP); rs.push(IP);           /// * execute colon word
    WP = token; IP = 0;                 /// * setup dolist call frame
    for (Code* w : pf.v) {              /// * inner interpreter
        try { w->exec(); IP++; }        /// * pass Code object to xt
        catch (...) {}
    }
    IP = rs.pop(); WP = rs.pop();       /// * return to caller
}
string Code::to_s() {
    return name + " " + to_string(token) + (immd ? "*" : "");
}
// external function examples (instead of inline)
void ss_dump() {
    cout << "< ";
    for (int i : ss.v) { cout << i << " "; }
    cout << setbase(base) << top << " >ok" << endl;
}
void see(Code* c, int dp) {                             // CC:
    auto pf = [](int dp, string s, vector<Code*> v) {   // lambda for indentation and recursive dump
        int i = dp; cout << endl; while (i--) cout << "  "; cout << s;
        for (Code* w : v) see(w, dp + 1);
    };
    auto qf = [](vector<int> v) { cout << "="; for (int i : v) cout << i << " "; };
    pf(dp, "[ " + c->to_s(), c->pf.v);
    if (c->pf1.v.size() > 0) pf(dp, "1--", c->pf1.v);
    if (c->pf2.v.size() > 0) pf(dp, "2--", c->pf2.v);
    if (c->qf.v.size() > 0)  qf(c->qf.v);
    cout << "]";
}
void words() {
    int i = 0;
    for (Code* w : dict.v) {
        cout << w->to_s() << " ";
        if ((++i % 10) == 0) cout << endl;
    }
    cout << endl;
}
/// macros to reduce verbosity (but harder to single-step debug)
#define CODE(s, g) new Code(s, [](Code *c){ g; })
#define IMMD(s, g) new Code(s, [](Code *c){ g; }, true)
/// primitives (mostly use lambda but can use external as well)
vector<Code*> prim = {
    // stack op examples
    CODE("dup", PUSH(top)),
    CODE("over", PUSH(ss[-2])),
    CODE("2dup", PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2over", PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("4dup", PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("rot", int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n); PUSH(m)),
    CODE("2swap", int n = ss.pop(); int m = ss.pop(); int l = ss.pop(); ss.push(n); PUSH(l); PUSH(m)),
    CODE("pick", int i = top; top = ss[-i]),
    //    CODE("roll", int i=top; top=ss[-i]),
    CODE("drop", POP()),
    CODE("nip", ss.pop()),
    CODE("2drop", POP(); POP()),
    CODE(">r", rs.push(top); POP()),
    CODE("r>", PUSH(rs.pop())),
    CODE("r@", PUSH(rs[-1])),
    CODE("push", rs.push(top); POP()),
    CODE("pop", PUSH(rs.pop())),
    // ALU examples
    CODE("+",  top += ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",  top = ss.pop() - top),
    CODE("*",  top *= ss.pop()),
    CODE("/",  top = ss.pop() / top),
    CODE("mod", top = ss.pop() % top),
    CODE("*/", top = ss.pop() * ss.pop() / top),
    CODE("*/mod", int n = ss.pop() * ss.pop();
        ss.push(n% top); top = (n / top)),
    CODE("and", top = top & ss.pop()),
    CODE("or", top = top | ss.pop()),
    CODE("xor", top = top ^ ss.pop()),
    CODE("negate", top = -top),
    CODE("abs", top = abs(top)),
    // logic
    CODE("0=", top = (top == 0) ? -1 : 0),                            // CC:1
    CODE("0<", top = (top < 0) ? -1 : 0),
    CODE("0>", top = (top > 0) ? -1 : 0),
    CODE("=", top = (ss.pop() == top) ? -1 : 0),                     // CC:1
    CODE(">", top = (ss.pop() > top) ? -1 : 0),                      // CC:1
    CODE("<", top = (ss.pop() < top) ? -1 : 0),
    CODE("<>", top = (ss.pop() != top) ? -1 : 0),
    CODE(">=", top = (ss.pop() >= top) ? -1 : 0),
    CODE("<=", top = (ss.pop() <= top) ? -1 : 0),
    // output
    CODE("base@", PUSH(base)),
    CODE("base!", base = top; POP()),
    CODE("hex", base = 16; cout << setbase(base)),
    CODE("decimal", base = 10; cout << setbase(base)),
    CODE("cr", cout << ("\n")),
    CODE(".", cout << top << " "; POP()),
    CODE(".r", int n = top; POP(); string s = to_string(top); POP();
        for (int i = 0; (i + s.size()) < n; i++) cout << (" ");
        cout << s ),
    CODE("u.r", int n = top; POP(); 
        string s = to_string(top & 0x7fffffff); POP();
        for (int i = 0; (i + s.size()) < n; i++) cout << (" ");
        cout << s ),
    CODE("key", string s; cin >> s; PUSH(s[0])),
    CODE("emit", char b = (char)top; POP(); cout << b),
    CODE("space", cout << (" ")),
    CODE("spaces", int n = top; POP(); for (int i = 0; i < n; i++) cout << (" ")),
    // literals
    CODE("dotstr",cout << c->literal),
    CODE("dolit", PUSH(c->qf[0])),
    CODE("dovar", PUSH(c->token)),
    CODE("[", compile = false),
    CODE("]", compile = true),
    CODE("$\"",
        string s; getline(cin, s, '"');             /// * copy string upto delimiter
        s=s.substr(1,s.size()-1);
        dict[-1]->addcode(new Code("dovar", s))),
    IMMD(".\"",
        string s; getline(cin, s, '"');             /// * copy string upto delimiter
        s=s.substr(1,s.size()-1);
        dict[-1]->addcode(new Code("dotstr",s))),
    IMMD("(", string s; getline(cin, s, ')')),
    IMMD(".(", string s; getline(cin, s, ')'); cout << s),
    CODE("\\", string s; getline(cin, s, '\n')),
    // branching examples
    IMMD("branch",
        bool f = top != 0; POP();                   // check flag then update top
        for (Code* w : (f ? c->pf.v : c->pf1.v)) w->exec()),
    IMMD("if",
        dict[-1]->addcode(new Code("branch"));      // bran=word->pf
        dict.push(new Code("temp"))),               // use last cell of dictionay as scratch pad
    IMMD("else",
        Code * temp = dict[-1];
        Code * last = dict[-2]->pf[-1];              // branching node
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 1),
    IMMD("then",
        Code * temp = dict[-1];
        Code * last = dict[-2]->pf[-1];
        if (last->stage == 0) {                     // if...then
            last->pf.merge(temp->pf.v);
            dict.pop();
        }
         else {                                      // if...else...then, or
             last->pf1.merge(temp->pf.v);            // for...aft...then...next
             if (last->stage == 1) dict.pop();
             else temp->pf.clear();
        }),
    // loops
    CODE("loops",                                         // CC:
        while (true) {
            for (Code* w : c->pf.v) w->exec();            // begin...
            int f = top;
            if (c->stage == 0 && (POP(), f != 0)) break;  // ...until
            // if (c->stage == 1) continue;               // ...again
            if (c->stage == 2 && (POP(), f == 0)) break;  // while...repeat
            for (Code* w : c->pf1.v) w->exec();
        }),
    IMMD("begin",
        dict[-1]->addcode(new Code("loops"));
        dict.push(new Code("temp"))),
    IMMD("while",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 2),
    IMMD("repeat",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        last->pf1.merge(temp->pf.v); dict.pop()),
    IMMD("again",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        last->pf.merge(temp->pf.v);
        last->stage = 1; dict.pop()),
    IMMD("until",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        last->pf.merge(temp->pf.v); dict.pop()),
    // for next
    CODE("cycles",                                  // CC:
        do {                                        // for
            for (Code* w : c->pf.v) w->exec();
        } while (c->stage == 0 && rs.dec_i() >= 0);   // for...next only
        while (c->stage > 0) {                      // aft
            for (Code* w : c->pf2.v) w->exec();    // then...next
            if (rs.dec_i() < 0) break;
            for (Code* w : c->pf1.v) w->exec();    // aft...then
        }
        rs.pop()),
    IMMD("for",
        dict[-1]->addcode(new Code(">r"));
        dict[-1]->addcode(new Code("cycles"));
        dict.push(new Code("temp"))),
    IMMD("aft",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 3),
    IMMD("next",
        Code * last = dict[-2]->pf[-1];
        Code * temp = dict[-1];
        if (last->stage == 0) last->pf.merge(temp->pf.v);
        else last->pf2.merge(temp->pf.v); dict.pop()),
    // compiler 
    CODE("exit", exit(0)),                  // exit interpreter, CC:
    CODE("exec", int n = top; dict[n]->exec()),
    CODE(":",
        string s; cin >> s;                 /// * get next token
        dict.push(new Code(s, true));       /// * create new word
        compile = true),
    IMMD(";",     compile = false),
    CODE("variable",
        string s; cin >> s;                 /// * create new variable
        dict.push(new Code(s, true));
        Code * last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token),
    CODE("create",
        string s; cin >> s;                 /// * create new variable
        dict.push(new Code(s, true));
        Code * last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token;
        last->pf[0]->qf.pop()),
    CODE("constant",   // n --
        string s; cin >> s;                 /// * create new constant
        dict.push(new Code(s, true));
        Code * last = dict[-1]->addcode(new Code("dolit", top));
        last->pf[0]->token = last->token; POP()),
    CODE("@",   // w -- n
        Code * last = dict[top]; POP();
        PUSH(last->pf[0]->qf[0])),
    CODE("!",   // n w --
        Code * last = dict[top]; POP();
        last->pf[0]->qf[0] = top; POP()),
    CODE("+!",   // n w --
        Code * last = dict[top]; POP();
        int n = last->pf[0]->qf[0] += top; POP()),
    CODE("?",   // w --
        Code * last = dict[top]; POP();
        cout << last->pf[0]->qf[0] << " "),
    CODE("array@",   // w a -- n
        int a = top; POP();
        Code * last = dict[top]; POP();
        PUSH(last->pf[0]->qf[a])),
    CODE("array!",   // n w a --
        int a = top; POP();
        Code * last = dict[top]; POP();
        last->pf[0]->qf[a] = top; POP()),
    CODE(",",       // n --
        Code * last = dict[-1];
        last->pf[0]->qf.push(top); POP()),
    CODE("allot",   // n --
        int n = top; POP();
        Code * last = dict[-1];
        for (int i = 0; i < n; i++) last->pf[0]->qf.push(0)),
    CODE("does",    // n --
        Code * last = dict[-1];
        Code * source = dict[WP];
        last->pf = source->pf;
        ),
    CODE("to",    // n -- , compile only
        Code * last = dict[WP]; IP++;                // current colon word
        last->pf[IP++]->pf[0]->qf.push(top); POP()),// next constant
    CODE("is",                                      // w -- , execute only
        Code * source = dict[top]; POP();            // source word
        string s; cin >> s;
        Code * w = find(s);
        if (w == NULL) throw length_error(" ");
        dict[w->token]->pf = source->pf),
    // tools
    CODE("here", PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",  ss_dump()),
    CODE("'",
        string s; cin >> s;                         /// * fetch next token
        Code * w = find(s); PUSH(w->token)),
    CODE("see",
        string s; cin >> s;            /// * fetch next token
        Code * w = find(s);
        if (w) see(w, 0); cout << endl),
    CODE("forget",
        string s; cin >> s;
        dict.erase(max(find(s)->token, 100))),
    CODE("boot", dict.erase(find("boot")->token + 1))	// CC:
};
/// main class
void dict_setup() {
    dict.merge(prim);                   /// * populate dictionary
    prim.clear();}                      /// * reduce memory footprint
void outer() {
    string idiom;
    while (cin >> idiom) {
        //cout << ">>" << idiom << "<<" << endl;
        Code* w = find(idiom);            /// * search through dictionary
        if (w) {                        /// * word found?
            if (compile && !w->immd) {     /// * in compile mode?
                dict[-1]->addcode(w);   /// * add to colon word
            }
            else {
                try { w->exec(); }      /// * execute forth word
                catch (exception& e) {
                    cout << e.what() << endl;
        }}}
        else {                          /// * try as numeric
            try {
                int n = stoi(idiom, nullptr, base);   /// * convert to integer
                if (compile) {
                    dict[-1]->addcode(new Code("dolit", n)); /// * add to current word
                }
                else PUSH(n);           /// * add value onto data stack
            }
            catch (...) {                /// * failed to parse number
                cout << idiom << "? " << endl;
                ss.clear(); top = -1; compile = false;
                getline(cin, idiom, '\n');
            }}
        if (!compile) ss_dump();           /// * stack dump and display ok prompt
    }}
// Main Program
int main(int ac, char* av[]) {
    dict_setup();
    cout << "ceforth 4.02" << endl;
    outer();
    cout << "done!" << endl;
    return 0;
}
