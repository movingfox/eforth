///
/// @file
/// @brief eForth implemented for micro controllers (Aruino & ESP)
///
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
#if CC_DEBUG
void mem_stat()  {
    LOG_KV("Core:",          xPortGetCoreID());
    LOG_KV(" heap[maxblk=",  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    LOG_KV(", avail=",       heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_KV(", ss_max=",      ss.max);
    LOG_KV(", rs_max=",      rs.max);
    LOG_KV(", pmem=",        HERE);
    LOG_KV("], lowest[heap=",heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    LOG_KV(", stack=",       uxTaskGetStackHighWaterMark(NULL));
    LOGS("]\n");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
void dict_dump() {
    LOG_KX("XT0=",        Code::XT0);
    LOG_KX("NM0=",        Code::NM0);
    LOG_KV(", sizeof(Code)=", sizeof(Code));
    LOGS("\n");
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        LOG(i);
        LOG_KX("> xt=",   c.xtoff());
        LOG_KX(":",       (UFP)c.xt);
        LOG_KX(", name=", (UFP)c.name - Code::NM0);
        LOG_KX(":"),      (UFP)c.name);
        LOGS(" ");        LOG(c.name);
        LOGS("\n");
    }
}
#else  // CC_DEBUG
void mem_stat()   {}
void dict_dump()  {}

#endif // CC_DEBUG

///====================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///
/// Arduino extra string handlers
int  find(string &s)  { return find(s.c_str()); }
void colon(string &s) { colon(s.c_str()); }
///
///> eForth turn-key code loader (from Flash memory)
///
#include <SPIFFS.h>
int forth_load(const char *fname) {
    auto dummy = [](int, const char *) { /* do nothing */ };
    if (!SPIFFS.begin()) {
        LOGS("Error mounting SPIFFS"); return 1;
    }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOGS("Error opening file:"); LOG(fname); return 1;
    }
    LOGS("Loading file: "); LOG(fname); LOGS("...");
    while (file.available()) {
        char cmd[256], *p = cmd, c;
        while ((c = file.read())!='\n') *p++ = c;   // one line a time
        *p = '\0';
        LOGS("\n<< "); LOG(cmd);                    // show bootstrap command
        forth_outer(cmd, dummy);
    }
    LOGS("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}
///
/// main program - Note: Arduino&ESP32 has their own main-loop
///
///====================================================================
