typedef struct sqlite3 sqlite3;
typedef long long sqlite3_int64;
int sqlite3_open(const char*, sqlite3**) { return 0; }
int sqlite3_close(sqlite3*) { return 0; }
