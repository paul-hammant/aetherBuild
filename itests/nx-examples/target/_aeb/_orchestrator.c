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

// Extern C function: build_session
void* build_session(const char*);

// Extern C function: build_done
void build_done(void*, const char*);

// Extern C function: aether_args_get
const char* aether_args_get(int);

// Extern C function: build_libs_products_product_H_detail_H_page
int build_libs_products_product_H_detail_H_page(void*);

// Extern C function: build_libs_shared_header
int build_libs_shared_header(void*);

// Extern C function: test_libs_shared_product_ui
int test_libs_shared_product_ui(void*);

// Extern C function: test_apps_products
int test_apps_products(void*);

// Extern C function: build_libs_shared_product_ui
int build_libs_shared_product_ui(void*);

// Extern C function: build_libs_shared_product_types
int build_libs_shared_product_types(void*);

// Extern C function: test_libs_shared_cart_state
int test_libs_shared_cart_state(void*);

// Extern C function: build_libs_shared_cart_state
int build_libs_shared_cart_state(void*);

// Extern C function: build_libs_shared_product_state
int build_libs_shared_product_state(void*);

// Extern C function: test_libs_shared_header
int test_libs_shared_header(void*);

// Extern C function: test_libs_products_home_H_page
int test_libs_products_home_H_page(void*);

// Extern C function: build_apps_cart
int build_apps_cart(void*);

// Extern C function: build_libs_cart_cart_H_page
int build_libs_cart_cart_H_page(void*);

// Extern C function: build_libs_shared_e2e_H_utils
int build_libs_shared_e2e_H_utils(void*);

// Extern C function: build_libs_shared_product_data
int build_libs_shared_product_data(void*);

// Extern C function: build_apps_products
int build_apps_products(void*);

// Extern C function: test_libs_products_product_H_detail_H_page
int test_libs_products_product_H_detail_H_page(void*);

// Extern C function: test_libs_cart_cart_H_page
int test_libs_cart_cart_H_page(void*);

// Extern C function: test_apps_cart
int test_apps_cart(void*);

// Extern C function: build_libs_shared_jsxify
int build_libs_shared_jsxify(void*);

// Extern C function: build_libs_products_home_H_page
int build_libs_products_home_H_page(void*);

// Extern C function: test_libs_shared_product_state
int test_libs_shared_product_state(void*);

// Extern C function: dist_apps_products
int dist_apps_products(void*);

// Extern C function: dist_apps_cart
int dist_apps_cart(void*);

int main(int argc, char** argv) {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);  // CP_UTF8
    SetConsoleCP(65001);
    #endif
    aether_args_init(argc, argv);
    
    {
void* s = build_session(aether_args_get(1));
build_libs_shared_product_types(s);
build_done(s, "libs/shared/product/types");
build_libs_shared_product_data(s);
build_done(s, "libs/shared/product/data");
build_libs_shared_product_state(s);
build_done(s, "libs/shared/product/state");
build_libs_shared_cart_state(s);
build_done(s, "libs/shared/cart/state");
build_libs_shared_jsxify(s);
build_done(s, "libs/shared/jsxify");
build_libs_shared_product_ui(s);
build_done(s, "libs/shared/product/ui");
build_libs_cart_cart_H_page(s);
build_done(s, "libs/cart/cart-page");
build_libs_shared_header(s);
build_done(s, "libs/shared/header");
build_apps_cart(s);
build_done(s, "apps/cart");
build_libs_products_home_H_page(s);
build_done(s, "libs/products/home-page");
build_libs_products_product_H_detail_H_page(s);
build_done(s, "libs/products/product-detail-page");
build_apps_products(s);
build_done(s, "apps/products");
build_libs_shared_e2e_H_utils(s);
build_done(s, "libs/shared/e2e-utils");
dist_apps_cart(s);
build_done(s, "dist:apps/cart");
dist_apps_products(s);
build_done(s, "dist:apps/products");
test_apps_cart(s);
build_done(s, "test:apps/cart");
test_apps_products(s);
build_done(s, "test:apps/products");
test_libs_cart_cart_H_page(s);
build_done(s, "test:libs/cart/cart-page");
test_libs_products_home_H_page(s);
build_done(s, "test:libs/products/home-page");
test_libs_products_product_H_detail_H_page(s);
build_done(s, "test:libs/products/product-detail-page");
test_libs_shared_cart_state(s);
build_done(s, "test:libs/shared/cart/state");
test_libs_shared_header(s);
build_done(s, "test:libs/shared/header");
test_libs_shared_product_state(s);
build_done(s, "test:libs/shared/product/state");
test_libs_shared_product_ui(s);
build_done(s, "test:libs/shared/product/ui");
    }
    return 0;
}
