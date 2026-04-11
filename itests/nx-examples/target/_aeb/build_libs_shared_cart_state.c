#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <unistd.h>
#include <sched.h>
#endif
#ifdef _WIN32
#  define aether_aligned_alloc(align, size) _aligned_malloc((size), (align))
#else
#  define aether_aligned_alloc(align, size) aligned_alloc((align), (size))
#endif
#ifndef likely
#  if defined(__GNUC__) || defined(__clang__)
#    define likely(x)   __builtin_expect(!!(x), 1)
#    define unlikely(x) __builtin_expect(!!(x), 0)
#  else
#    define likely(x)   (x)
#    define unlikely(x) (x)
#  endif
#endif
#ifndef AETHER_GCC_COMPAT
#  if (defined(__GNUC__) || defined(__clang__)) && !defined(__EMSCRIPTEN__)
#    define AETHER_GCC_COMPAT 1
#  else
#    define AETHER_GCC_COMPAT 0
#  endif
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifdef _WIN32
static int64_t _aether_clock_ns(void) {
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (int64_t)((double)now.QuadPart / freq.QuadPart * 1000000000.0);
}
#elif defined(__EMSCRIPTEN__)
static int64_t _aether_clock_ns(void) {
    return (int64_t)(emscripten_get_now() * 1000000.0);
}
#elif defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0)
static int64_t _aether_clock_ns(void) { return 0; }
#else
static int64_t _aether_clock_ns(void) {
    struct timespec _ts;
    clock_gettime(CLOCK_MONOTONIC, &_ts);
    return (int64_t)_ts.tv_sec * 1000000000LL + _ts.tv_nsec;
}
#endif
#include <stdarg.h>
static void* _aether_interp(const char* fmt, ...) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char* str = (char*)malloc(len + 1);
    vsnprintf(str, len + 1, fmt, args2);
    va_end(args2);
    return (void*)str;
}
static inline const char* _aether_safe_str(const void* s) {
    return s ? (const char*)s : "(null)";
}
#if !AETHER_GCC_COMPAT
static void* _aether_ref_new(intptr_t val) { intptr_t* r = malloc(sizeof(intptr_t)); *r = val; return (void*)r; }
#endif
typedef struct { void (*fn)(void); void* env; } _AeClosure;
static inline void* _aether_box_closure(_AeClosure c) { _AeClosure* p = malloc(sizeof(_AeClosure)); *p = c; return (void*)p; }
static inline _AeClosure _aether_unbox_closure(void* p) { return *(_AeClosure*)p; }
typedef struct { _AeClosure compute; intptr_t value; int evaluated; } _AeThunk;
static inline void* _aether_thunk_new(_AeClosure c) { _AeThunk* t = malloc(sizeof(_AeThunk)); t->compute = c; t->value = 0; t->evaluated = 0; return (void*)t; }
static inline intptr_t _aether_thunk_force(void* p) { _AeThunk* t = (_AeThunk*)p; if (!t->evaluated) { t->value = (intptr_t)((intptr_t(*)(void*))t->compute.fn)(t->compute.env); t->evaluated = 1; } return t->value; }
static inline void _aether_thunk_free(void* p) { if (p) free(p); }
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 1) && !defined(__arm__) && !defined(__thumb__)
#include <termios.h>
static struct termios _aether_orig_termios;
static void _aether_raw_mode(void) {
    tcgetattr(0, &_aether_orig_termios);
    struct termios raw = _aether_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);
}
static void _aether_cooked_mode(void) {
    tcsetattr(0, TCSANOW, &_aether_orig_termios);
}
#else
static void _aether_raw_mode(void) {}
static void _aether_cooked_mode(void) {}
#endif
static void* _aether_ctx_stack[64];
static int _aether_ctx_depth = 0;
static inline void _aether_ctx_push(void* ctx) { if (_aether_ctx_depth < 64) _aether_ctx_stack[_aether_ctx_depth++] = ctx; }
static inline void _aether_ctx_pop(void) { if (_aether_ctx_depth > 0) _aether_ctx_depth--; }
static inline void* _aether_ctx_get(void) { return _aether_ctx_depth > 0 ? _aether_ctx_stack[_aether_ctx_depth-1] : (void*)0; }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

void aether_args_init(int argc, char** argv);



// Forward declarations
int build_libs_shared_cart_state(void*);
void* build_session(const char*);
void* build_begin(void*, const char*);
void build_done(void*, const char*);
void* build_start();
int build_dep(void*, const char*);
void build_lib(void*, const char*);
void build_bom(void*, const char*);
void build_repo(void*, const char*);
void build_cargo_dep(void*, const char*);
void build_npm_dep(void*, const char*);
void build__mkdirs(const char*);
void* build__get(void*, const char*);
const char* build__strip_lang(const char*);
const char* build__read_dep_artifact(void*, const char*, const char*);
void build__write_artifact(void*, const char*, const char*);
int build__needs_rebuild(const char*, const char*);
const char* build__collect_file_list(const char*);
const char* build__build_dep_classpath(void*);
const char* build__build_lib_classpath(void*);
void build_extra(void*, const char*);
void build_werror(void*);
void build_nowarn(void*);
void build_verbose(void*);
int build_load_bom_file(void*, const char*);
void* build__resolve_maven_deps(void*);
void* build__build_maven_classpath(void*);
void* build__resolve_npm_deps(void*);
int build_load_deps_file(void*, const char*);
void ts_strict(void*);
void ts_ts_target(void*, const char*);
void ts_module_kind(void*, const char*);
void ts_out_dir(void*, const char*);
void ts_mocha_timeout(void*, const char*);
void ts_reporter(void*, const char*);
void ts_mocha_grep(void*, const char*);
int ts_tsc(void*, void*);
int ts_mocha(void*, const char*, void*);
void ts_tsconfig(void*, const char*);
void ts_skip_lib_check(void*);
int ts_tsc_project(void*, void*);
int ts_ngc_project(void*, void*);
int ts_jest_project(void*, void*);
void ts_webpack_config(void*, const char*);
int ts_webpack_bundle(void*, void*);
int ts_ng_build(void*, void*);

// Import: build
// Extern C function: map_new
void* map_new();

// Extern C function: map_put
void map_put(void*, const char*, void*);

// Extern C function: map_get
void* map_get(void*, const char*);

// Extern C function: map_has
int map_has(void*, const char*);

// Extern C function: list_new
void* list_new();

// Extern C function: list_add
void list_add(void*, void*);

// Extern C function: list_get
void* list_get(void*, int);

// Extern C function: list_size
int list_size(void*);

// Extern C function: os_getenv
const char* os_getenv(const char*);

// Extern C function: os_system
int os_system(const char*);

// Extern C function: os_exec
const char* os_exec(const char*);

// Extern C function: aether_args_get
const char* aether_args_get(int);

// Extern C function: file_exists
int file_exists(const char*);

// Extern C function: file_mtime
int file_mtime(const char*);

// Extern C function: dir_create
int dir_create(const char*);

// Extern C function: dir_list_count
int dir_list_count(void*);

// Extern C function: dir_list_get
const char* dir_list_get(void*, int);

// Extern C function: dir_list_free
void dir_list_free(void*);

// Extern C function: fs_glob
void* fs_glob(const char*);

// Extern C function: path_join
const char* path_join(const char*, const char*);

// Extern C function: string_concat
const char* string_concat(const char*, const char*);

// Extern C function: string_length
int string_length(const char*);

// Extern C function: string_index_of
int string_index_of(const char*, const char*);

// Extern C function: string_substring
const char* string_substring(const char*, int, int);

// Extern C function: string_trim
const char* string_trim(const char*);

// Extern C function: string_ends_with
int string_ends_with(const char*, const char*);

// Extern C function: io_read_file
const char* io_read_file(const char*);

// Extern C function: io_write_file
int io_write_file(const char*, const char*);


// Import: build as load_deps_file
// Extern C function: map_new
void* map_new();

// Extern C function: map_put
void map_put(void*, const char*, void*);

// Extern C function: map_get
void* map_get(void*, const char*);

// Extern C function: map_has
int map_has(void*, const char*);

// Extern C function: list_new
void* list_new();

// Extern C function: list_add
void list_add(void*, void*);

// Extern C function: list_get
void* list_get(void*, int);

// Extern C function: list_size
int list_size(void*);

// Extern C function: os_getenv
const char* os_getenv(const char*);

// Extern C function: os_system
int os_system(const char*);

// Extern C function: os_exec
const char* os_exec(const char*);

// Extern C function: aether_args_get
const char* aether_args_get(int);

// Extern C function: file_exists
int file_exists(const char*);

// Extern C function: file_mtime
int file_mtime(const char*);

// Extern C function: dir_create
int dir_create(const char*);

// Extern C function: dir_list_count
int dir_list_count(void*);

// Extern C function: dir_list_get
const char* dir_list_get(void*, int);

// Extern C function: dir_list_free
void dir_list_free(void*);

// Extern C function: fs_glob
void* fs_glob(const char*);

// Extern C function: path_join
const char* path_join(const char*, const char*);

// Extern C function: string_concat
const char* string_concat(const char*, const char*);

// Extern C function: string_length
int string_length(const char*);

// Extern C function: string_index_of
int string_index_of(const char*, const char*);

// Extern C function: string_substring
const char* string_substring(const char*, int, int);

// Extern C function: string_trim
const char* string_trim(const char*);

// Extern C function: string_ends_with
int string_ends_with(const char*, const char*);

// Extern C function: io_read_file
const char* io_read_file(const char*);

// Extern C function: io_write_file
int io_write_file(const char*, const char*);


// Import: ts
// Extern C function: map_put
void map_put(void*, const char*, void*);

// Extern C function: map_get
void* map_get(void*, const char*);

// Extern C function: map_has
int map_has(void*, const char*);

// Extern C function: list_get
void* list_get(void*, int);

// Extern C function: list_size
int list_size(void*);

// Extern C function: os_system
int os_system(const char*);

// Extern C function: os_exec
const char* os_exec(const char*);

// Extern C function: file_exists
int file_exists(const char*);

// Extern C function: path_join
const char* path_join(const char*, const char*);

// Extern C function: string_concat
const char* string_concat(const char*, const char*);

// Extern C function: string_length
int string_length(const char*);

// Extern C function: string_trim
const char* string_trim(const char*);

// Extern C function: io_read_file
const char* io_read_file(const char*);

// Extern C function: io_write_file
int io_write_file(const char*, const char*);


// Import: ts as skip_lib_check
// Extern C function: map_put
void map_put(void*, const char*, void*);

// Extern C function: map_get
void* map_get(void*, const char*);

// Extern C function: map_has
int map_has(void*, const char*);

// Extern C function: list_get
void* list_get(void*, int);

// Extern C function: list_size
int list_size(void*);

// Extern C function: os_system
int os_system(const char*);

// Extern C function: os_exec
const char* os_exec(const char*);

// Extern C function: file_exists
int file_exists(const char*);

// Extern C function: path_join
const char* path_join(const char*, const char*);

// Extern C function: string_concat
const char* string_concat(const char*, const char*);

// Extern C function: string_length
int string_length(const char*);

// Extern C function: string_trim
const char* string_trim(const char*);

// Extern C function: io_read_file
const char* io_read_file(const char*);

// Extern C function: io_write_file
int io_write_file(const char*, const char*);


int build_libs_shared_cart_state(void* s) {
void* b = build_begin(s, "libs/shared/cart/state");
if ((intptr_t)b == 0) {
        {
            return 0;
        }
    }
build_load_deps_file(b, "../../../../angular.deps.ae");
build_dep(b, "libs/shared/product/data");
build_dep(b, "libs/shared/product/state");
    {
        void* _bcfg = map_new();
        _aether_ctx_push(_bcfg);
        {
ts_skip_lib_check(_aether_ctx_get());
        }
        _aether_ctx_pop();
        ts_ngc_project(b, _bcfg);
    }
}

void* build_session(const char* root_path) {
void* s = map_new();
map_put(s, "root", root_path);
map_put(s, "visited", map_new());
    return s;
}

void* build_begin(void* s, const char* module_dir) {
void* visited = map_get(s, "visited");
if (map_has(visited, module_dir) == 1) {
        {
            return map_get(s, "_null_");
        }
    }
void* root = map_get(s, "root");
void* ctx = map_new();
map_put(ctx, "root", root);
map_put(ctx, "module_dir", module_dir);
const char* fs_dir = module_dir;
int is_colocated_test = 0;
int test_prefix = string_index_of(module_dir, "test:");
if (test_prefix == 0) {
        {
fs_dir = string_substring(module_dir, 5, string_length(module_dir));
is_colocated_test = 1;
        }
    }
int dist_prefix = string_index_of(module_dir, "dist:");
if (dist_prefix == 0) {
        {
fs_dir = string_substring(module_dir, 5, string_length(module_dir));
        }
    }
const char* mod_name = fs_dir;
const char* lang_prefix = "";
int slash = string_index_of(fs_dir, "/");
if (slash >= 0) {
        {
lang_prefix = string_substring(fs_dir, 0, slash);
mod_name = string_substring(fs_dir, (slash + 1), string_length(fs_dir));
        }
    }
map_put(ctx, "module", mod_name);
const char* source_dir = path_join(root, fs_dir);
map_put(ctx, "source_dir", source_dir);
const char* target_base = path_join(root, "target");
int is_test = string_ends_with(lang_prefix, "tests");
if (is_test == 1) {
        {
target_base = path_join(target_base, "tests");
        }
    }
if (is_colocated_test == 1) {
        {
target_base = path_join(target_base, "tests");
        }
    }
const char* tgt_dir = path_join(target_base, fs_dir);
map_put(ctx, "target_dir", tgt_dir);
map_put(ctx, "deps", list_new());
map_put(ctx, "libs", list_new());
map_put(ctx, "cargo_deps", list_new());
map_put(ctx, "npm_deps", list_new());
map_put(ctx, "maven_deps", list_new());
map_put(ctx, "boms", list_new());
map_put(ctx, "repos", list_new());
    return ctx;
}

void build_done(void* s, const char* module_dir) {
void* visited = map_get(s, "visited");
map_put(visited, module_dir, "1");
}

void* build_start() {
void* ctx = map_new();
const char* root = os_getenv("BUILD_ROOT");
const char* module_dir = os_getenv("BUILD_MODULE_DIR");
map_put(ctx, "root", root);
map_put(ctx, "module_dir", module_dir);
const char* mod_name = module_dir;
int slash = string_index_of(module_dir, "/");
if (slash >= 0) {
        {
mod_name = string_substring(module_dir, (slash + 1), string_length(module_dir));
        }
    }
map_put(ctx, "module", mod_name);
const char* source_dir = path_join(root, module_dir);
map_put(ctx, "source_dir", source_dir);
const char* target_base = path_join(root, "target");
const char* tgt_dir = path_join(target_base, module_dir);
map_put(ctx, "target_dir", tgt_dir);
map_put(ctx, "deps", list_new());
map_put(ctx, "libs", list_new());
map_put(ctx, "cargo_deps", list_new());
map_put(ctx, "npm_deps", list_new());
map_put(ctx, "maven_deps", list_new());
map_put(ctx, "boms", list_new());
map_put(ctx, "repos", list_new());
    return ctx;
}

int build_dep(void* ctx, const char* path) {
int npm_prefix = string_index_of(path, "npm:");
if (npm_prefix == 0) {
        {
void* npm_deps = map_get(ctx, "npm_deps");
const char* pkg_spec = string_substring(path, 4, string_length(path));
list_add(npm_deps, pkg_spec);
            return 0;
        }
    }
int colon = string_index_of(path, ":");
if (colon >= 0) {
        {
void* maven_deps = map_get(ctx, "maven_deps");
list_add(maven_deps, path);
        }
    } else {
        {
void* deps = map_get(ctx, "deps");
list_add(deps, path);
        }
    }
}

void build_lib(void* ctx, const char* path) {
void* libs = map_get(ctx, "libs");
list_add(libs, path);
}

void build_bom(void* ctx, const char* coord) {
void* boms = map_get(ctx, "boms");
list_add(boms, coord);
}

void build_repo(void* ctx, const char* url) {
void* repos = map_get(ctx, "repos");
list_add(repos, url);
}

void build_cargo_dep(void* ctx, const char* path) {
void* cdeps = map_get(ctx, "cargo_deps");
list_add(cdeps, path);
}

void build_npm_dep(void* ctx, const char* path) {
void* ndeps = map_get(ctx, "npm_deps");
list_add(ndeps, path);
}

void build__mkdirs(const char* dir_path) {
os_system(_aether_interp("mkdir -p %s", _aether_safe_str(dir_path)));
}

void* build__get(void* ctx, const char* key) {
    return map_get(ctx, key);
}

const char* build__strip_lang(const char* dep_path) {
    return dep_path;
}

const char* build__read_dep_artifact(void* ctx, const char* dep_module, const char* artifact) {
void* root = build__get(ctx, "root");
const char* t = path_join(root, "target");
const char* t2 = path_join(t, dep_module);
const char* p = path_join(t2, artifact);
if (file_exists(p) == 1) {
        {
            return io_read_file(p);
        }
    }
    return "";
}

void build__write_artifact(void* ctx, const char* artifact, const char* content) {
void* td = build__get(ctx, "target_dir");
const char* p = path_join(td, artifact);
io_write_file(p, content);
}

int build__needs_rebuild(const char* source_dir, const char* ts_file) {
if (file_exists(ts_file) == 0) {
        {
            return 1;
        }
    }
int prev = file_mtime(ts_file);
const char* pat = path_join(source_dir, "*");
void* files = fs_glob(pat);
int count = dir_list_count(files);
int i = 0;
while (i < count) {
        {
const char* f = dir_list_get(files, i);
int mt = file_mtime(f);
if (mt > prev) {
                {
dir_list_free(files);
                    return 1;
                }
            }
i = (i + 1);
        }
    }
dir_list_free(files);
    return 0;
}

const char* build__collect_file_list(const char* pattern) {
void* files = fs_glob(pattern);
int count = dir_list_count(files);
const char* result = "";
int i = 0;
while (i < count) {
        {
const char* f = dir_list_get(files, i);
if (i == 0) {
                {
result = string_concat(f, "");
                }
            } else {
                {
const char* sp = string_concat(result, " ");
result = string_concat(sp, f);
                }
            }
i = (i + 1);
        }
    }
dir_list_free(files);
    return result;
}

const char* build__build_dep_classpath(void* ctx) {
void* deps = map_get(ctx, "deps");
const char* cp = "";
int count = list_size(deps);
int i = 0;
while (i < count) {
        {
void* dp = list_get(deps, i);
const char* dm = build__strip_lang(dp);
const char* dep_cp_raw = build__read_dep_artifact(ctx, dm, "jvm_classpath_deps_including_transitive");
if (string_length(dep_cp_raw) > 0) {
                {
const char* dep_cp = string_trim(os_exec(_aether_interp("echo '%s' | tr '\n' ':' | sed 's/:$//'", _aether_safe_str(dep_cp_raw))));
if (string_length(cp) > 0) {
                        {
const char* tmp = string_concat(cp, ":");
cp = string_concat(tmp, dep_cp);
                        }
                    } else {
                        {
cp = dep_cp;
                        }
                    }
                }
            }
i = (i + 1);
        }
    }
    return cp;
}

const char* build__build_lib_classpath(void* ctx) {
void* root = build__get(ctx, "root");
void* libs = map_get(ctx, "libs");
const char* cp = "";
int count = list_size(libs);
int i = 0;
while (i < count) {
        {
void* lp = list_get(libs, i);
const char* libs_dir = path_join(root, "libs");
const char* full = path_join(libs_dir, lp);
if (string_length(cp) > 0) {
                {
const char* tmp = string_concat(cp, ":");
cp = string_concat(tmp, full);
                }
            } else {
                {
cp = full;
                }
            }
i = (i + 1);
        }
    }
    return cp;
}

void build_extra(void* _ctx, const char* flag) {
if (map_has(_ctx, "extra") == 1) {
        {
void* prev = map_get(_ctx, "extra");
const char* combined = string_concat(prev, " ");
combined = string_concat(combined, flag);
map_put(_ctx, "extra", combined);
        }
    } else {
        {
map_put(_ctx, "extra", flag);
        }
    }
}

void build_werror(void* _ctx) {
map_put(_ctx, "werror", "true");
}

void build_nowarn(void* _ctx) {
map_put(_ctx, "nowarn", "true");
}

void build_verbose(void* _ctx) {
map_put(_ctx, "verbose", "true");
}

int build_load_bom_file(void* ctx, const char* rel_path) {
void* source_dir = build__get(ctx, "source_dir");
const char* full_path = path_join(source_dir, rel_path);
if (file_exists(full_path) == 0) {
        {
printf("warning: BOM file not found: %s", _aether_safe_str(full_path)); putchar('\n');
            return 0;
        }
    }
map_put(ctx, "_bom_file", full_path);
    return 1;
}

void* build__resolve_maven_deps(void* ctx) {
if (map_has(ctx, "_maven_resolved") == 1) {
        {
            return map_get(ctx, "_maven_resolved");
        }
    }
void* maven_deps = map_get(ctx, "maven_deps");
int dep_count = list_size(maven_deps);
if (dep_count == 0) {
        {
map_put(ctx, "_maven_resolved", "");
            return "";
        }
    }
void* target_dir = build__get(ctx, "target_dir");
build__mkdirs(target_dir);
const char* aeb_home = os_getenv("AEB_HOME");
const char* resolver_jar = path_join(aeb_home, "tools/aeb-resolve.jar");
const char* cmd = _aether_interp("java -jar %s --output classpath", _aether_safe_str(resolver_jar));
if (map_has(ctx, "_bom_file") == 1) {
        {
void* bom_file = map_get(ctx, "_bom_file");
cmd = string_concat(cmd, _aether_interp(" --bom-file %s", _aether_safe_str(bom_file)));
        }
    }
void* boms = map_get(ctx, "boms");
int bom_count = list_size(boms);
int bi = 0;
while (bi < bom_count) {
        {
void* b_coord = list_get(boms, bi);
cmd = string_concat(cmd, _aether_interp(" --bom %s", _aether_safe_str(b_coord)));
bi = (bi + 1);
        }
    }
void* repos = map_get(ctx, "repos");
int repo_count = list_size(repos);
int ri = 0;
while (ri < repo_count) {
        {
void* r_url = list_get(repos, ri);
cmd = string_concat(cmd, _aether_interp(" --repo %s", _aether_safe_str(r_url)));
ri = (ri + 1);
        }
    }
int di = 0;
while (di < dep_count) {
        {
void* d_coord = list_get(maven_deps, di);
cmd = string_concat(cmd, _aether_interp(" %s", _aether_safe_str(d_coord)));
di = (di + 1);
        }
    }
void* mod_dir = build__get(ctx, "module_dir");
printf("%s: resolving maven dependencies", _aether_safe_str(mod_dir)); putchar('\n');
const char* result = string_trim(os_exec(cmd));
if (string_length(result) == 0) {
        {
printf("%s: warning: no maven jars resolved", _aether_safe_str(mod_dir)); putchar('\n');
        }
    }
map_put(ctx, "_maven_resolved", result);
build__write_artifact(ctx, "maven_classpath", result);
    return result;
}

void* build__build_maven_classpath(void* ctx) {
if (map_has(ctx, "_maven_cp_built") == 1) {
        {
            return map_get(ctx, "_maven_cp_built");
        }
    }
void* own_cp = build__resolve_maven_deps(ctx);
void* deps = map_get(ctx, "deps");
int count = list_size(deps);
int i = 0;
while (i < count) {
        {
void* dp = list_get(deps, i);
const char* dm = build__strip_lang(dp);
const char* dep_mvn = build__read_dep_artifact(ctx, dm, "maven_classpath");
if (string_length(dep_mvn) > 0) {
                {
if (string_length(own_cp) > 0) {
                        {
const char* tmp = string_concat(own_cp, ":");
own_cp = string_concat(tmp, dep_mvn);
                        }
                    } else {
                        {
own_cp = dep_mvn;
                        }
                    }
                }
            }
i = (i + 1);
        }
    }
map_put(ctx, "_maven_cp_built", own_cp);
    return own_cp;
}

void* build__resolve_npm_deps(void* ctx) {
if (map_has(ctx, "_npm_resolved") == 1) {
        {
            return map_get(ctx, "_npm_resolved");
        }
    }
void* npm_deps = map_get(ctx, "npm_deps");
int dep_count = list_size(npm_deps);
if (dep_count == 0) {
        {
map_put(ctx, "_npm_resolved", "");
            return "";
        }
    }
void* root = build__get(ctx, "root");
const char* aeb_dir = path_join(root, ".aeb");
const char* nm_dir = path_join(aeb_dir, "node_modules");
void* mod_dir = build__get(ctx, "module_dir");
if (file_exists(nm_dir) == 1) {
        {
map_put(ctx, "_npm_resolved", nm_dir);
            return nm_dir;
        }
    }
printf("%s: resolving npm dependencies via pnpm", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_pkg = path_join(aeb_dir, "package.json");
if (file_exists(aeb_pkg) == 0) {
        {
io_write_file(aeb_pkg, "{}");
        }
    }
const char* aeb_npmrc = path_join(aeb_dir, ".npmrc");
if (file_exists(aeb_npmrc) == 0) {
        {
io_write_file(aeb_npmrc, "node-linker=hoisted");
        }
    }
const char* cmd = _aether_interp("COREPACK_ENABLE_STRICT=0 pnpm add --prefix '%s'", _aether_safe_str(aeb_dir));
int di = 0;
while (di < dep_count) {
        {
void* pkg = list_get(npm_deps, di);
int pkg_len = string_length(pkg);
int last_colon = (-(1));
int ci = 0;
while (ci < pkg_len) {
                {
const char* ch = string_substring(pkg, ci, (ci + 1));
if (strcmp(_aether_safe_str(ch), _aether_safe_str(":")) == 0) {
                        {
last_colon = ci;
                        }
                    }
ci = (ci + 1);
                }
            }
void* pnpm_spec = pkg;
if (last_colon > 0) {
                {
const char* pkg_name = string_substring(pkg, 0, last_colon);
const char* pkg_ver = string_substring(pkg, (last_colon + 1), pkg_len);
pnpm_spec = string_concat(pkg_name, "@");
pnpm_spec = string_concat(pnpm_spec, pkg_ver);
                }
            }
cmd = string_concat(cmd, _aether_interp(" '%s'", _aether_safe_str(pnpm_spec)));
di = (di + 1);
        }
    }
int exit_code = os_system(cmd);
if (exit_code != 0) {
        {
printf("%s: warning: pnpm add failed", _aether_safe_str(mod_dir)); putchar('\n');
        }
    }
map_put(ctx, "_npm_resolved", nm_dir);
build__write_artifact(ctx, "npm_node_modules", nm_dir);
    return nm_dir;
}

int build_load_deps_file(void* ctx, const char* rel_path) {
void* source_dir = build__get(ctx, "source_dir");
const char* full_path = path_join(source_dir, rel_path);
if (file_exists(full_path) == 0) {
        {
printf("warning: deps file not found: %s", _aether_safe_str(full_path)); putchar('\n');
            return 0;
        }
    }
map_put(ctx, "_bom_file", full_path);
const char* parsed = string_trim(os_exec(_aether_interp("grep 'dep.*npm:' '%s' | grep -o '\"npm:[^\"]*\"' | tr -d '\"' | sed 's/^npm://'", _aether_safe_str(full_path))));
if (string_length(parsed) == 0) {
        {
            return 1;
        }
    }
void* npm_deps_list = map_get(ctx, "npm_deps");
int line_idx = 1;
while (line_idx <= 500) {
        {
const char* line = string_trim(os_exec(_aether_interp("echo '%s' | sed -n '%dp'", _aether_safe_str(parsed), line_idx)));
if (string_length(line) == 0) {
                {
line_idx = 501;
                }
            } else {
                {
list_add(npm_deps_list, line);
line_idx = (line_idx + 1);
                }
            }
        }
    }
    return 1;
}

void ts_strict(void* _ctx) {
map_put(_ctx, "strict", "true");
}

void ts_ts_target(void* _ctx, const char* t) {
map_put(_ctx, "target", t);
}

void ts_module_kind(void* _ctx, const char* m) {
map_put(_ctx, "module_kind", m);
}

void ts_out_dir(void* _ctx, const char* d) {
map_put(_ctx, "out_dir", d);
}

void ts_mocha_timeout(void* _ctx, const char* ms) {
map_put(_ctx, "timeout", ms);
}

void ts_reporter(void* _ctx, const char* r) {
map_put(_ctx, "reporter", r);
}

void ts_mocha_grep(void* _ctx, const char* pat) {
map_put(_ctx, "grep", pat);
}

int ts_tsc(void* ctx, void* _builder) {
void* root = build__get(ctx, "root");
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_name = build__get(ctx, "module");
void* mod_dir = build__get(ctx, "module_dir");
void* deps = map_get(ctx, "deps");
build__mkdirs(target_dir);
const char* ts_file = path_join(target_dir, ".timestamp");
if (build__needs_rebuild(source_dir, ts_file) == 0) {
        {
printf("%s: skipping compilation of prod code (not changed)", _aether_safe_str(mod_dir)); putchar('\n');
            return 0;
        }
    }
printf("%s: compiling prod code", _aether_safe_str(mod_dir)); putchar('\n');
int dep_count = list_size(deps);
const char* ts_deps = path_join(target_dir, "js");
int i = 0;
while (i < dep_count) {
        {
void* dp6 = list_get(deps, i);
const char* dm6 = build__strip_lang(dp6);
const char* dep_td = build__read_dep_artifact(ctx, dm6, "typescript_module_deps_including_transitive");
if (string_length(dep_td) > 0) {
                {
ts_deps = string_concat(string_concat(ts_deps, "\n"), dep_td);
                }
            }
i = (i + 1);
        }
    }
build__write_artifact(ctx, "typescript_module_deps_including_transitive", ts_deps);
void* npm_deps_list = map_get(ctx, "npm_deps");
int npm_count = list_size(npm_deps_list);
const char* npm_out = "";
i = 0;
while (i < npm_count) {
        {
void* nd = list_get(npm_deps_list, i);
if (string_length(npm_out) > 0) {
                {
npm_out = string_concat(string_concat(npm_out, "\n"), nd);
                }
            } else {
                {
npm_out = string_concat(nd, "");
                }
            }
i = (i + 1);
        }
    }
i = 0;
while (i < dep_count) {
        {
void* dp7 = list_get(deps, i);
const char* dm7 = build__strip_lang(dp7);
const char* dep_npm = build__read_dep_artifact(ctx, dm7, "npm_deps_including_transitive");
if (string_length(dep_npm) > 0) {
                {
if (string_length(npm_out) > 0) {
                        {
npm_out = string_concat(string_concat(npm_out, "\n"), dep_npm);
                        }
                    } else {
                        {
npm_out = dep_npm;
                        }
                    }
                }
            }
i = (i + 1);
        }
    }
build__write_artifact(ctx, "npm_deps_including_transitive", npm_out);
const char* shared_out = "";
i = 0;
while (i < dep_count) {
        {
void* dp8 = list_get(deps, i);
const char* dm8 = build__strip_lang(dp8);
const char* dep_sh = build__read_dep_artifact(ctx, dm8, "shared_library_deps_including_transitive");
if (string_length(dep_sh) > 0) {
                {
if (string_length(shared_out) > 0) {
                        {
shared_out = string_concat(string_concat(shared_out, "\n"), dep_sh);
                        }
                    } else {
                        {
shared_out = dep_sh;
                        }
                    }
                }
            }
i = (i + 1);
        }
    }
build__write_artifact(ctx, "shared_library_deps_including_transitive", shared_out);
const char* gen_cmd = _aether_interp("%s/shared-build-scripts/generate-typescript-base-tsconfig-json.sh '%s' '%s' '%s' '%s'", _aether_safe_str(root), _aether_safe_str(root), _aether_safe_str(source_dir), _aether_safe_str(target_dir), _aether_safe_str(ts_deps));
os_system(gen_cmd);
const char* npm_file = path_join(target_dir, "npm_deps_including_transitive");
if (file_exists(npm_file) == 1) {
        {
const char* npm_content = io_read_file(npm_file);
if (string_length(npm_content) > 0) {
                {
const char* npm_args = string_trim(os_exec(_aether_interp("echo '%s' | tr '\n' ' '", _aether_safe_str(npm_content))));
const char* tgt_suffix = string_trim(os_exec(_aether_interp("echo '%s' | sed 's|.*/target/||'", _aether_safe_str(target_dir))));
const char* npm_cmd = _aether_interp("%s/shared-build-scripts/add-npm-deps-to-base-tsconfig-json.sh %s %s %s %s", _aether_safe_str(root), _aether_safe_str(root), _aether_safe_str(mod_name), _aether_safe_str(tgt_suffix), _aether_safe_str(npm_args));
os_system(npm_cmd);
                }
            }
        }
    }
const char* tsc_cmd = _aether_interp("cd %s && tsc", _aether_safe_str(source_dir));
if (_builder != NULL) {
        {
if (map_has(_builder, "strict") == 1) {
                {
tsc_cmd = string_concat(tsc_cmd, " --strict");
                }
            }
if (map_has(_builder, "target") == 1) {
                {
void* val = map_get(_builder, "target");
tsc_cmd = string_concat(tsc_cmd, _aether_interp(" --target %s", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "module_kind") == 1) {
                {
void* val = map_get(_builder, "module_kind");
tsc_cmd = string_concat(tsc_cmd, _aether_interp(" --module %s", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "out_dir") == 1) {
                {
void* val = map_get(_builder, "out_dir");
tsc_cmd = string_concat(tsc_cmd, _aether_interp(" --outDir %s", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "extra") == 1) {
                {
void* val = map_get(_builder, "extra");
tsc_cmd = string_concat(tsc_cmd, _aether_interp(" %s", _aether_safe_str(val)));
                }
            }
        }
    }
int exit_code = os_system(tsc_cmd);
if (exit_code != 0) {
        {
printf("%s: tsc failed", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
io_write_file(ts_file, "");
    return 0;
}

int ts_mocha(void* ctx, const char* test_file, void* _builder) {
void* root = build__get(ctx, "root");
void* target_dir = build__get(ctx, "target_dir");
void* mod_name = build__get(ctx, "module");
void* mod_dir = build__get(ctx, "module_dir");
void* deps = map_get(ctx, "deps");
const char* js_dir = path_join(target_dir, "js");
const char* node_path = js_dir;
const char* target_base = path_join(root, "target");
node_path = string_concat(string_concat(node_path, ":"), target_base);
int dep_count = list_size(deps);
int i = 0;
while (i < dep_count) {
        {
void* dp9 = list_get(deps, i);
const char* dm9 = build__strip_lang(dp9);
const char* dep_td = build__read_dep_artifact(ctx, dm9, "typescript_module_deps_including_transitive");
if (string_length(dep_td) > 0) {
                {
const char* dep_td_flat = string_trim(os_exec(_aether_interp("echo '%s' | tr '\n' ':' | sed 's/:$//'", _aether_safe_str(dep_td))));
node_path = string_concat(string_concat(node_path, ":"), dep_td_flat);
                }
            }
i = (i + 1);
        }
    }
const char* npm_vendored = path_join(root, "libs/javascript/npm_vendored/node_modules");
node_path = string_concat(string_concat(node_path, ":"), npm_vendored);
const char* ld_path = "";
i = 0;
while (i < dep_count) {
        {
void* dpA = list_get(deps, i);
const char* dmA = build__strip_lang(dpA);
const char* dep_sh = build__read_dep_artifact(ctx, dmA, "shared_library_deps_including_transitive");
if (string_length(dep_sh) > 0) {
                {
const char* sh_dir = os_exec(_aether_interp("dirname %s | head -1", _aether_safe_str(dep_sh)));
sh_dir = string_trim(sh_dir);
if (string_length(ld_path) > 0) {
                        {
ld_path = string_concat(string_concat(ld_path, ":"), sh_dir);
                        }
                    } else {
                        {
ld_path = sh_dir;
                        }
                    }
                }
            }
i = (i + 1);
        }
    }
const char* mocha_bin = path_join(root, "libs/javascript/npm_vendored/node_modules/mocha/bin/mocha.js");
printf("%s: running tests", _aether_safe_str(mod_dir)); putchar('\n');
const char* mocha_opts = "";
if (_builder != NULL) {
        {
if (map_has(_builder, "timeout") == 1) {
                {
void* val = map_get(_builder, "timeout");
mocha_opts = string_concat(mocha_opts, _aether_interp(" --timeout %s", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "reporter") == 1) {
                {
void* val = map_get(_builder, "reporter");
mocha_opts = string_concat(mocha_opts, _aether_interp(" --reporter %s", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "grep") == 1) {
                {
void* val = map_get(_builder, "grep");
mocha_opts = string_concat(mocha_opts, _aether_interp(" --grep '%s'", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "extra") == 1) {
                {
void* val = map_get(_builder, "extra");
mocha_opts = string_concat(mocha_opts, _aether_interp(" %s", _aether_safe_str(val)));
                }
            }
        }
    }
const char* run_cmd = "";
if (string_length(ld_path) > 0) {
        {
run_cmd = _aether_interp("NODE_PATH=%s LD_LIBRARY_PATH=%s node %s%s %s", _aether_safe_str(node_path), _aether_safe_str(ld_path), _aether_safe_str(mocha_bin), _aether_safe_str(mocha_opts), _aether_safe_str(test_file));
        }
    } else {
        {
run_cmd = _aether_interp("NODE_PATH=%s node %s%s %s", _aether_safe_str(node_path), _aether_safe_str(mocha_bin), _aether_safe_str(mocha_opts), _aether_safe_str(test_file));
        }
    }
int exit_code = os_system(run_cmd);
if (exit_code != 0) {
        {
printf("%s: tests FAILED", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
printf("%s: tests PASSED", _aether_safe_str(mod_dir)); putchar('\n');
    return 0;
}

void ts_tsconfig(void* _ctx, const char* path) {
map_put(_ctx, "tsconfig", path);
}

void ts_skip_lib_check(void* _ctx) {
map_put(_ctx, "skip_lib_check", "true");
}

int ts_tsc_project(void* ctx, void* _builder) {
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_dir = build__get(ctx, "module_dir");
void* root = build__get(ctx, "root");
build__mkdirs(target_dir);
build__resolve_npm_deps(ctx);
const char* tc = path_join(source_dir, "tsconfig.lib.json");
if (_builder != NULL) {
        {
if (map_has(_builder, "tsconfig") == 1) {
                {
void* tc_rel = map_get(_builder, "tsconfig");
tc = path_join(source_dir, tc_rel);
                }
            }
        }
    }
printf("%s: compiling (tsc)", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_dir = path_join(root, ".aeb");
const char* tsc_bin = path_join(aeb_dir, "node_modules/.bin/tsc");
const char* cmd = _aether_interp("%s -p '%s'", _aether_safe_str(tsc_bin), _aether_safe_str(tc));
if (_builder != NULL) {
        {
if (map_has(_builder, "skip_lib_check") == 1) {
                {
cmd = string_concat(cmd, " --skipLibCheck");
                }
            }
if (map_has(_builder, "out_dir") == 1) {
                {
void* val = map_get(_builder, "out_dir");
cmd = string_concat(cmd, _aether_interp(" --outDir '%s'", _aether_safe_str(val)));
                }
            }
if (map_has(_builder, "extra") == 1) {
                {
void* val = map_get(_builder, "extra");
cmd = string_concat(cmd, _aether_interp(" %s", _aether_safe_str(val)));
                }
            }
        }
    }
int exit_code = os_system(cmd);
if (exit_code != 0) {
        {
printf("%s: tsc failed", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
io_write_file(path_join(target_dir, ".timestamp"), "");
    return 0;
}

int ts_ngc_project(void* ctx, void* _builder) {
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_dir = build__get(ctx, "module_dir");
void* root = build__get(ctx, "root");
build__mkdirs(target_dir);
build__resolve_npm_deps(ctx);
const char* tc = path_join(source_dir, "tsconfig.lib.json");
if (_builder != NULL) {
        {
if (map_has(_builder, "tsconfig") == 1) {
                {
void* tc_rel = map_get(_builder, "tsconfig");
tc = path_join(source_dir, tc_rel);
                }
            }
        }
    }
printf("%s: compiling (ngc)", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_dir = path_join(root, ".aeb");
const char* ngc_bin = path_join(aeb_dir, "node_modules/.bin/ngc");
const char* cmd = _aether_interp("%s -p '%s'", _aether_safe_str(ngc_bin), _aether_safe_str(tc));
if (_builder != NULL) {
        {
if (map_has(_builder, "skip_lib_check") == 1) {
                {
cmd = string_concat(cmd, " --skipLibCheck");
                }
            }
if (map_has(_builder, "extra") == 1) {
                {
void* val = map_get(_builder, "extra");
cmd = string_concat(cmd, _aether_interp(" %s", _aether_safe_str(val)));
                }
            }
        }
    }
int exit_code = os_system(cmd);
if (exit_code != 0) {
        {
printf("%s: ngc failed", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
io_write_file(path_join(target_dir, ".timestamp"), "");
    return 0;
}

int ts_jest_project(void* ctx, void* _builder) {
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_dir = build__get(ctx, "module_dir");
void* root = build__get(ctx, "root");
build__mkdirs(target_dir);
build__resolve_npm_deps(ctx);
printf("%s: running tests (jest)", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_dir = path_join(root, ".aeb");
const char* jest_bin = path_join(aeb_dir, "node_modules/.bin/jest");
const char* timeout_secs = "60";
if (_builder != NULL) {
        {
if (map_has(_builder, "timeout") == 1) {
                {
timeout_secs = map_get(_builder, "timeout");
                }
            }
        }
    }
const char* jest_config = path_join(source_dir, "jest.config.ts");
const char* cmd = _aether_interp("timeout %s %s --config '%s' --passWithNoTests", _aether_safe_str(timeout_secs), _aether_safe_str(jest_bin), _aether_safe_str(jest_config));
if (_builder != NULL) {
        {
if (map_has(_builder, "extra") == 1) {
                {
void* val = map_get(_builder, "extra");
cmd = string_concat(cmd, _aether_interp(" %s", _aether_safe_str(val)));
                }
            }
        }
    }
int exit_code = os_system(cmd);
if (exit_code == 124) {
        {
printf("%s: tests TIMEOUT", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
if (exit_code != 0) {
        {
printf("%s: tests FAILED", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
printf("%s: tests PASSED", _aether_safe_str(mod_dir)); putchar('\n');
    return 0;
}

void ts_webpack_config(void* _ctx, const char* path) {
map_put(_ctx, "webpack_config", path);
}

int ts_webpack_bundle(void* ctx, void* _builder) {
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_dir = build__get(ctx, "module_dir");
void* root = build__get(ctx, "root");
build__mkdirs(target_dir);
build__resolve_npm_deps(ctx);
const char* wp_config = path_join(source_dir, "webpack.aeb.config.js");
if (_builder != NULL) {
        {
if (map_has(_builder, "webpack_config") == 1) {
                {
void* cfg = map_get(_builder, "webpack_config");
wp_config = path_join(source_dir, cfg);
                }
            }
        }
    }
printf("%s: bundling (webpack)", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_dir = path_join(root, ".aeb");
const char* webpack_bin = path_join(aeb_dir, "node_modules/.bin/webpack");
const char* nm_dir = path_join(aeb_dir, "node_modules");
const char* cmd = _aether_interp("NODE_PATH='%s' %s --config '%s'", _aether_safe_str(nm_dir), _aether_safe_str(webpack_bin), _aether_safe_str(wp_config));
int exit_code = os_system(cmd);
if (exit_code != 0) {
        {
printf("%s: webpack failed", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
printf("%s: bundle complete", _aether_safe_str(mod_dir)); putchar('\n');
    return 0;
}

int ts_ng_build(void* ctx, void* _builder) {
void* source_dir = build__get(ctx, "source_dir");
void* target_dir = build__get(ctx, "target_dir");
void* mod_dir = build__get(ctx, "module_dir");
void* root = build__get(ctx, "root");
build__mkdirs(target_dir);
build__resolve_npm_deps(ctx);
const char* app_name = string_trim(os_exec(_aether_interp("basename '%s'", _aether_safe_str(source_dir))));
printf("%s: building (ng build)", _aether_safe_str(mod_dir)); putchar('\n');
const char* aeb_dir = path_join(root, ".aeb");
const char* ng_bin = path_join(aeb_dir, "node_modules/.bin/ng");
const char* cmd = _aether_interp("%s build %s --configuration production", _aether_safe_str(ng_bin), _aether_safe_str(app_name));
int exit_code = os_system(cmd);
if (exit_code != 0) {
        {
printf("%s: ng build failed", _aether_safe_str(mod_dir)); putchar('\n');
            return exit_code;
        }
    }
printf("%s: build complete", _aether_safe_str(mod_dir)); putchar('\n');
    return 0;
}

