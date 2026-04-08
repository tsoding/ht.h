// ht.h - v1.0.1 - Public Domain - Hash Table in C
//
// # STB-style Single Header Library
//
// This is a classic stb-style single header library. See
// https://github.com/nothings/stb/blob/28d546d5eb77d4585506a20480f4de2e706dff4c/docs/stb_howto.txt
// for more info.
//
// So if you just include it as is it acts as a header file, that is it only provides declarations
// without implementations. To make it also provide implementations you define HT_IMPLEMENTATION macro:
//
// ```c
// #define HT_IMPLEMENTATION
// #include "ht.h"
// ```
//
// So now it acts more like a .c file. This gives you control over where you want the implementations go.
// For example if you want to compile the library as a separate translation unit with GCC on Linux,
// you can do it like this:
//
// ```console
// $ gcc -DHT_IMPLEMENTATION -x c -c ht.h
// ```
//
// This creates an .o file which you can link with your main program. And everywhere in your main program
// you include ht.h WITHOUT defining HT_IMPLEMENTATION, because all the implementations are now in
// the object file. But I don't think you want to do this for this library in particular because all the
// actually useful functions are static anyway and expected to be used through C macros.
//
// # C++ support
//
// This is a pure C library and C++ is somewhat supported, but only for the case when you have pure C code that
// uses ht.h and you need to compile it with a C++ compiler. Maybe you ended up in a situation where you don't
// have a pure C compiler. You should still be able to compile your code. The minimal supported standard is C++20
// since we make use of designated initializers.
//
// Any other C++ related case is outside of the scope of this library. Using ht.h from C++ code is definetely not
// supported, and all the unexpected side effects are fully on you. For example ht.h expects the keys to be Trivially
// Copyable [https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable.html] so be careful with that. Besides,
// if you are using C++ you probably don't even need this library. Just use std::unordered_map or whatever.
//
// # typeof
//
// The library makes use of __typeof__() which is not standard (at least before C23). But what we found out during the
// development is that it's supported by pretty much all the relevant C compilers. We checked GCC, Clang, MSVC, Chibicc,
// and TinyCC. If your compiler doesn't support it, you can't use this library. At least in a comfortable way.
//
// # Performance
//
// Don't expect the state of the art performance out of this Hash Table. It's just a basic Hash Table with Open Addressing and
// Quadratic Probing. But it is a Hash Table. All the operations are O(1) amortized as you would expect (except maybe
// ht_reset()). I use it in my projects and it works reasonably fast for a Hash Table. The goal of this library was to have a
// nice Generic Hash Table API in pure C. We may improve the performance later if needed.
#ifndef HT_H_
#define HT_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// The Hash Table.
//
// You can define it like this:
// ```c
// #include "ht.h"
//
// Ht(int, int) ht = {0}; // Zero-initialized Hash Table is a valid Hash Table!
// ```
//
// You should probably typedef it if you want to pass it to multiple places. Because anonymous structs
// are not particularly compatible with each other even if they have literally the same definition:
//
// ```c
// #include "ht.h"
//
// typedef Ht(const char*, int) Word_Count;
//
// void count_words(Word_Count *wc, const char *text);
// ```
#define Ht(Key, Value)                                           \
    struct {                                                     \
        /* .count - amount of unique items in the Hash Table. */ \
        size_t      count;                                       \
        /* .hasheq - key Hashing and Equality function.          \
         * NULL means direct byte representation of the key      \
         * will be hashed and compared.                          \
         */                                                      \
        Ht_Hasheq   hasheq;                                      \
        /* .impl_* - these fields are subject to change.         \
         * Do not access any impl_* fields if you just need      \
         * to iterate the Table. Use ht_foreach() instead.       \
         */                                                      \
        Key        *impl_key_ptr;                                \
        size_t      impl_filled_slots;                           \
        size_t      impl_capacity;                               \
        /* .default_value - ht_put() and friends will use        \
         * .default_value as the default value for all the newly \
         * inserted values. Leave it empty for zero-initialized  \
         * values by default.                                    \
         */                                                      \
        Value       default_value;                               \
    }

// By default the Hash Table compares and hashes the keys by their byte representation. For the C string types,
// you must specify .hasheq. Ht_Hasheq is a function that combines together Hashing and Equality operations on the key.
// We provide default implementation for the C string types:
//
// ```c
// #include "ht.h"
//
// Ht(const char *, int) ht = {
//     .hasheq = ht_cstr_hasheq,
// };
// ```
//
// This covers 90% of the cases. If you need to treat your keys in a different way, create
// your custom Ht_Hasheq function:
//
// ```c
// #include "ht.h"
// #include <string.h>
//
// typedef struct {
//     const char *data;
//     int count;
// } String_View;
//
// uintptr_t sv_hasheq(Ht_Op op, void const* a_, void const* b_, size_t n);
//
// Ht(String_View, int) ht = {
//     .hasheq = sv_hasheq,
// };
//
// uintptr_t sv_hasheq(Ht_Op op, void const* a_, void const* b_, size_t n)
// {
//     (void) n; // `n` is the size of the key in bytes. We pass it in case your hasheq
//               // doesn't know it for some reason. `ht_mem_hasheq` for example uses it.
//     String_View const* a = (String_View const*)a_;
//     String_View const* b = (String_View const*)b_;
//     switch (op) {
//     case HT_HASH: return ht_default_hash(a->data, a->count);
//     case HT_EQ:   return a->count == b->count && memcmp(a->data, b->data, a->count) == 0;
//     }
//     return 0;
// }
// ```
//
// If you need to specify .hasheq for an inner Ht, use the .default_value field:
//
// ```c
// #include "ht.h"
//
// Ht(const char *, Ht(const char *, int)) ht = {
//     .hasheq = ht_cstr_hasheq,
//     .default_value = {
//         .hasheq = ht_cstr_hasheq,
//     },
// };
// ```
typedef enum {
    HT_HASH,
    HT_EQ,
} Ht_Op;
typedef uintptr_t (*Ht_Hasheq)(Ht_Op op, void const *a, void const *b, size_t n);

#ifndef HT_PUBDEF
    #ifdef __cplusplus
        #define HT_PUBDEF extern "C"
    #else
        #define HT_PUBDEF extern
    #endif
#endif

// The default .hasheq implementation for C-strings.
HT_PUBDEF uintptr_t ht_cstr_hasheq(Ht_Op op, void const *a, void const *b, size_t n);
// The default .hasheq implementation for when .hasheq == NULL.
HT_PUBDEF uintptr_t ht_mem_hasheq(Ht_Op op, void const *a, void const *b, size_t n);

#ifdef NOB_H_
// If you are using nob.h we automatically provide hasheq for its String_View
// type. We detect nob.h presence by NOB_H_ being defined. It is also checked under
// HT_IMPLEMENTATION. So if you happen to compile ht.h as a separate translation unit do
// not forget to defined NOB_H_ manually to make sure ht_sv_hasheq implementation
// is provided:
// ```console
// $ gcc -DHT_IMPLEMENTATION -DNOB_H_ -x c -c ht.h
// ```
HT_PUBDEF uintptr_t ht_sv_hasheq(Ht_Op op, void const *a, void const *b, size_t n);
#endif // NOB_H_

// Value *ht_put(Ht(Key, Value) *ht, Key key)
//
// Inserts the key with the value initialized with ht->default_value.
// Returns the pointer to the inserted value. Operation is O(1) amortized.
//
// ```c
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// int main(void)
// {
//     Ht(const char *, int) ht = {
//         .hasheq = ht_cstr_hasheq,
//     };
//     *ht_put(&ht, "foo") = 69;
//     *ht_put(&ht, "bar") = 420;
//     *ht_put(&ht, "baz") = 1337;
//     return 0;
// }
// ```
#if !defined(__cplusplus)
    #define ht_put(ht, key_)                            \
        ((__typeof__((ht)->default_value)*) ht__put(    \
            (Ht__Abstract*)(ht),                        \
            (__typeof__(*(ht)->impl_key_ptr)[]){key_}, \
            ht__layout(ht)                              \
        ))
#else
    #define ht_put ht__put_cpp
#endif

// Value *ht_find(Ht(Key, Value) *ht, Key key)
//
// Tries to find a value by the key. If the value is found returns the pointer to the value,
// otherwise returns NULL. Operation is O(1) amortized.
//
// ```c
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// const char *words[] = {"foo", "bar", "foo", "baz", "aboba"};
// size_t words_count = sizeof(words)/sizeof(words[0]);
//
// int main(void)
// {
//     Ht(const char *, int) ht = { .hasheq = ht_cstr_hasheq };
//     for (size_t i = 0; i < words_count; ++i) {
//         int *count = ht_find(&ht, words[i]);
//         if (count) {
//             *count += 1;
//         } else {
//             *ht_put(&ht, words[i]) = 1;
//         }
//     }
// }
// ```
#if !defined(__cplusplus)
    #define ht_find(ht, key_)                          \
        ((__typeof__((ht)->default_value)*) ht__find(  \
            (Ht__Abstract*)(ht),                       \
            (__typeof__(*(ht)->impl_key_ptr)[]){key_}, \
            ht__layout(ht)                             \
        ))
#else
    #define ht_find ht__find_cpp
#endif // __cplusplus

// Value *ht_find_or_put(Ht(Key, Value) *ht, Key key)
//
// Tries to find a value by the key, if not found inserts the key with the value initialized as ht->default_value.
// Never fails. Always returns either the pointer to the found value or the newly added value. Operation is O(1)
// amortized.
//
// ```c
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// const char *words[] = {"foo", "bar", "foo", "baz", "aboba"};
// size_t words_count = sizeof(words)/sizeof(words[0]);
//
// int main(void)
// {
//     Ht(const char *, int) ht = { .hasheq = ht_cstr_hasheq };
//     for (size_t i = 0; i < words_count; ++i) {
//         *ht_find_or_put(&ht, words[i]) += 1;
//     }
//     return 0;
// }
// ```
#if !defined(__cplusplus)
    #define ht_find_or_put(ht, key_)                         \
        ((__typeof__((ht)->default_value)*) ht__find_or_put( \
            (Ht__Abstract*)(ht),                             \
            (__typeof__(*(ht)->impl_key_ptr)[]){key_},       \
            ht__layout(ht)                                   \
        ))
#else
    #define ht_find_or_put ht__find_or_put_cpp
#endif // __cplusplus

// void ht_delete(Ht(Key, Value) *ht, Value *value)
//
// Delete the element by the pointer to its value slot. You can
// get the value pointer via ht_find() or ht_foreach(). NULL is
// a valid value pointer and will be simply ignored. Operation is O(1).
//
// ```c
// #include <stdio.h>
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// int main(void)
// {
//     Ht(const char *, int) ht = { .hasheq = ht_cstr_hasheq };
//     // ...
//     int *count = ht_find(&ht, "foo");
//     if (count) {
//         ht_delete(&ht, count);
//         printf("`foo` has been deleted!\n");
//     } else {
//         printf("`foo` doesn't exist!\n");
//     }
// }
// ```
#define ht_delete(ht, value) ht__delete((Ht__Abstract*)(ht), (value), ht__layout(ht))

// bool ht_find_and_delete(Ht(Key, Value) *ht, Key key)
//
// Combines together ht_find() and ht_delete() enabling you to delete the elements
// by the keys. Returns true when the element was deleted, returns false when the
// element doesn’t exist. Operation is O(1) amortized.
//
// ```c
// #include <stdio.h>
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// int main(void)
// {
//     Ht(const char *, int) ht = { .hasheq = ht_cstr_hasheq };
//     // ...
//     if (ht_find_and_delete(&ht, "foo")) {
//         printf("`foo` has been deleted!\n");
//     } else {
//         printf("`foo` doesn't exist!\n");
//     }
//     return 0;
// }
// ```
#if !defined(__cplusplus)
    #define ht_find_and_delete(ht, key_)                \
        ht__find_and_delete(                            \
            (Ht__Abstract*)(ht),                        \
            (__typeof__(*(ht)->impl_key_ptr)[]){key_}, \
            ht__layout(ht)                              \
        )
#else
    #define ht_find_and_delete ht__find_and_delete_cpp
#endif // __cplusplus

// Key ht_key(Ht(Key, Value) *ht, Value *value)
//
// Returns the key of the element by its value pointer. Useful in conjunction with ht_foreach().
// Operation is O(1).
#define ht_key(ht, value_) \
    (*(ht__typeof((ht)->impl_key_ptr))ht__key(value_, ht__layout(ht)))

// A foreach macro that iterates the values of the Hash Table.
//
// ```c
// #include <stdio.h>
// #define HT_IMPLEMENTATION
// #include "ht.h"
//
// int main(void)
// {
//     Ht(const char *, int) ht = { .hasheq = ht_cstr_hasheq };
//     ht_foreach(value, &ht) {
//         printf("%s => %d\n", ht_key(&ht, value), *value);
//     }
//     return 0;
// }
// ```
#define ht_foreach(iter, ht)                           \
    for (ht__typeof((ht)->default_value)* iter = NULL; \
         ht__next((Ht__Abstract*)(ht), (void **)&iter, ht__layout(ht));)

// void ht_reset(Ht(Key, Value) *ht)
//
// Removes all the elements from the hash table, but does not deallocate any memory, making the hash table
// ready to be reused again. Operation is O(capacity) if ht->count > 0. When ht->count == 0 it's O(1).
#define ht_reset(ht) ht__reset((Ht__Abstract*)(ht), ht__layout(ht))

// void ht_free(Ht(Key, Value) *ht)
//
// Deallocates all the memory associated with the hash table and completely resets its state.
// Operation should be O(1) but it calls to HT_FREE() to free the allocated buffers. So include
// that into your estimates.
#define ht_free(ht) ht__free((Ht__Abstract*)(ht))

// The default hash function. It's the hash function that is used by default
// throughout the library if your .hasheq is set to NULL. You can redefine it.
#ifndef ht_default_hash
#define ht_default_hash ht_fnv1a_hash
#endif // ht_default_hash

// A bunch of predefined hash function. Nothing too fancy. Just the classics.
// http://www.cse.yorku.ca/~oz/hash.html#djb2
HT_PUBDEF uintptr_t ht_djb2_hash(void const *data, size_t size);
// http://www.isthe.com/chongo/tech/comp/fnv/index.html
HT_PUBDEF uintptr_t ht_fnv1a_hash(void const *data, size_t size);
HT_PUBDEF uintptr_t ht_fnv1_hash(void const *data, size_t size);
// http://www.cse.yorku.ca/~oz/hash.html#sdbm
HT_PUBDEF uintptr_t ht_sdbm_hash(void const *data, size_t size);
// http://www.cse.yorku.ca/~oz/hash.html#k&r1
HT_PUBDEF uintptr_t ht_lose_lose_hash(void const *data, size_t size); // please don't use this one
HT_PUBDEF uintptr_t ht_knuth_hash(void const *data, size_t size);
HT_PUBDEF uintptr_t ht_id_hash(void const *data, size_t size);

// Redefinable macros for the libc functionality.
// You can easily remove the dependancy on libc if you redefine them.

#if !defined(HT_ASSERT)
    #include <assert.h>
    #define HT_ASSERT assert
#endif

#if !defined(HT_FREE) && !defined(HT_MALLOC)
    #include <stdlib.h>
    #define HT_FREE   free
    #define HT_MALLOC malloc
#elif !defined(HT_FREE) || !defined(HT_MALLOC)
    #error "Both HT_FREE and HT_MALLOC must be defined together"
#endif

// PRIVATE NAMES! DO NOT USE DIRECTLY!
// Anything that starts with `ht__` regardless of case can be changed
// between releases without any notice.

#define HT__MIN_CAP 32
#define HT__LOAD_FACTOR_PERCENT 70

#if !defined(__cplusplus)
    #define HT__CLIT(name) (name)
#else
    #define HT__CLIT(name) name
#endif // __cplusplus

#if !defined(__cplusplus)
    #define ht__typeof __typeof__
#else
    #define ht__typeof decltype
#endif // __cplusplus

typedef struct {
    size_t slot_size;
    size_t key_size;
    size_t value_size;
    size_t key_offset;
    size_t hash_offset;
} Ht__Layout;

#define ht__layout(ht)                                                   \
    (HT__CLIT(Ht__Layout) {                                              \
        .slot_size     = ht__word_align(sizeof((ht)->default_value))     \
                          + ht__word_align(sizeof(*(ht)->impl_key_ptr))  \
                          + sizeof(uintptr_t),                           \
        .key_size      = sizeof(*(ht)->impl_key_ptr),                    \
        .value_size    = sizeof((ht)->default_value),                    \
        .key_offset    = ht__word_align(sizeof((ht)->default_value)),    \
        .hash_offset   = ht__word_align(sizeof((ht)->default_value))     \
                          + ht__word_align(sizeof(*(ht)->impl_key_ptr)), \
    })

#define ht__word_align(size)   (((size) + sizeof(uintptr_t) - 1)/sizeof(uintptr_t)*sizeof(uintptr_t))
#define ht__slot_key(slot, l)  ((uint8_t*)(slot) + (l).key_offset)
#define ht__slot_hash(slot, l) (uintptr_t*)((uint8_t*)(slot) + (l).hash_offset)
#define ht__slot_size(l)       (l).slot_size

typedef struct {
    size_t      count;
    Ht_Hasheq   hasheq;
    void       *impl_slots;
    size_t      impl_filled_slots;
    size_t      impl_capacity;
    uint8_t     default_value;
} Ht__Abstract;

enum {
    HT__EMPTY = 0,
    HT__DELETED,
    HT__FIRST_VALID_HASH,
};

static void *ht__put(Ht__Abstract *ht, void *key, Ht__Layout l);
static void *ht__find(Ht__Abstract *ht, void *key, Ht__Layout l);
static void *ht__find_or_put(Ht__Abstract *ht, void *key, Ht__Layout l);
static void ht__delete(Ht__Abstract *ht, void *slot, Ht__Layout l);
static bool ht__find_and_delete(Ht__Abstract *ht, void *key, Ht__Layout l);
static void *ht__key(void *slot, Ht__Layout l);
static bool ht__next(Ht__Abstract *ht, void **slot, Ht__Layout l);
static void ht__reset(Ht__Abstract *ht, Ht__Layout l);
static void ht__free(Ht__Abstract *ht);
static void *ht__find_slot(Ht__Abstract *ht, uintptr_t hash, Ht_Hasheq hasheq, void *key, Ht__Layout l);
static void *ht__put_no_expand(Ht__Abstract *ht, void *key, Ht__Layout l);
static void ht__expand(Ht__Abstract *ht, Ht__Layout l);
static size_t ht__strlen(const char *s);
static int ht__strcmp(const char *l, const char *r);
static void *ht__memcpy(void *dest, const void *src, size_t n);
static int ht__memcmp(const void *vl, const void *vr, size_t n);

#if defined(__cplusplus)
    static inline auto *ht__put_cpp(auto *ht, auto key)
    {
        decltype(*ht->impl_key_ptr) key_ = key;
        return (decltype(ht->default_value)*) ht__put((Ht__Abstract*)ht, &key_, ht__layout(ht));
    }

    static inline auto *ht__find_cpp(auto *ht, auto key)
    {
        decltype(*ht->impl_key_ptr) key_ = key;
        return (decltype(ht->default_value)*) ht__find((Ht__Abstract*)ht, &key_, ht__layout(ht));
    }

    static inline auto *ht__find_or_put_cpp(auto *ht, auto key)
    {
        decltype(*ht->impl_key_ptr) key_ = key;
        return (decltype(ht->default_value)*) ht__find_or_put((Ht__Abstract*)ht, &key_, ht__layout(ht));
    }

    static inline bool ht__find_and_delete_cpp(auto *ht, auto key)
    {
        decltype(*ht->impl_key_ptr) key_ = key;
        return ht__find_and_delete((Ht__Abstract*)ht, &key_, ht__layout(ht));
    }
#endif // __cplusplus

#endif // HT_H_

#ifdef HT_IMPLEMENTATION

static void *ht__put(Ht__Abstract *ht, void *key, Ht__Layout l)
{
    ht__expand(ht, l);
    return ht__put_no_expand(ht, key, l);
}

static void *ht__find(Ht__Abstract *ht, void *key, Ht__Layout l)
{
    if (ht->impl_capacity == 0) return NULL;

    Ht_Hasheq hasheq = ht->hasheq ? ht->hasheq : ht_mem_hasheq;
    uintptr_t hash = hasheq(HT_HASH, key, NULL, l.key_size);
    if (hash < HT__FIRST_VALID_HASH) hash += HT__FIRST_VALID_HASH;
    uint8_t *slot = (uint8_t*)ht__find_slot(ht, hash, hasheq, key, l);
    if (slot == NULL) return NULL;
    uintptr_t slot_hash = *ht__slot_hash(slot, l);
    HT_ASSERT(slot_hash != HT__DELETED);
    if (slot_hash == HT__EMPTY) return NULL;
    return slot;
}

static void *ht__find_or_put(Ht__Abstract *ht, void *key, Ht__Layout l)
{
    void *slot = ht__find(ht, key, l);
    if (slot) return slot;
    return ht__put(ht, key, l);
}

static void ht__delete(Ht__Abstract *ht, void *slot, Ht__Layout l)
{
    if (slot == NULL) return;
    *ht__slot_hash(slot, l) = HT__DELETED;
    ht->count -= 1;
}

static bool ht__find_and_delete(Ht__Abstract *ht, void *key, Ht__Layout l)
{
    void *slot = ht__find(ht, key, l);
    if (slot == NULL) return false;
    ht__delete(ht, slot, l);
    return true;
}

static void *ht__key(void *slot, Ht__Layout l)
{
    return ht__slot_key(slot, l);
}

static bool ht__next(Ht__Abstract *ht, void **slot, Ht__Layout l)
{
    uint8_t *slots_start = (uint8_t*)ht->impl_slots;
    uint8_t *slots_end   = slots_start + ht->impl_capacity*ht__slot_size(l);
    uint8_t *iter        = (uint8_t*)*slot;

    if (iter == NULL) {
        iter = slots_start;
    } else {
        iter += ht__slot_size(l);
    }

    while (iter < slots_end && *ht__slot_hash(iter, l) < HT__FIRST_VALID_HASH) {
        iter += ht__slot_size(l);
    }
    *slot = iter;
    return iter < slots_end;
}

static void ht__reset(Ht__Abstract *ht, Ht__Layout l)
{
    if (ht->count == 0) return; // Since ht_reset() is O(capacity) do not do it if ht is already empty
    uint8_t *slots_start = (uint8_t*)ht->impl_slots;
    uint8_t *slots_end = slots_start + ht->impl_capacity*ht__slot_size(l);
    for (uint8_t *iter = slots_start; iter < slots_end; iter += ht__slot_size(l)) {
        *ht__slot_hash(iter, l) = HT__EMPTY;
    }
    ht->impl_filled_slots = 0;
    ht->count = 0;
}

static void ht__free(Ht__Abstract *ht)
{
    HT_FREE(ht->impl_slots);
    ht->impl_slots        = NULL;
    ht->impl_filled_slots = 0;
    ht->impl_capacity     = 0;
    ht->count             = 0;
}

HT_PUBDEF uintptr_t ht_cstr_hasheq(Ht_Op op, void const* a_, void const* b_, size_t n)
{
    char const* const* a = (char const* const*)a_;
    char const* const* b = (char const* const*)b_;
    HT_ASSERT(n == sizeof(*a));
    switch (op) {
    case HT_HASH: return ht_default_hash(*a, ht__strlen(*a));
    case HT_EQ:   return ht__strcmp(*a, *b) == 0;
    }
    return 0;
}

HT_PUBDEF uintptr_t ht_mem_hasheq(Ht_Op op, void const* a_, void const *b_, size_t n)
{
    uint8_t const* a = (uint8_t const*)a_;
    uint8_t const* b = (uint8_t const*)b_;
    switch (op) {
    case HT_HASH: return ht_default_hash(a, n);
    case HT_EQ:   return ht__memcmp(a, b, n) == 0;
    }
    return 0;
}

#ifdef NOB_H_
HT_PUBDEF uintptr_t ht_sv_hasheq(Ht_Op op, void const *a_, void const *b_, size_t n)
{
    (void) n; // not used
    Nob_String_View const* a = (Nob_String_View const*)a_;
    Nob_String_View const* b = (Nob_String_View const*)b_;
    switch (op) {
    case HT_HASH: return ht_default_hash(a->data, a->count);
    case HT_EQ:   return nob_sv_eq(*a, *b);
    }
    return 0;
}
#endif // NOB_H_

HT_PUBDEF uintptr_t ht_djb2_hash(void const *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uintptr_t hash = 5381u;
    for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + (uintptr_t)bytes[i];
    }
    return hash;
}

HT_PUBDEF uintptr_t ht_fnv1a_hash(void const *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211u;
    }
    return (uintptr_t)hash;
}

HT_PUBDEF uintptr_t ht_fnv1_hash(void const *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037u;
    for (size_t i = 0; i < size; ++i) {
        hash *= 1099511628211u;
        hash ^= (uint64_t)bytes[i];
    }
    return (uintptr_t)hash;
}

HT_PUBDEF uintptr_t ht_sdbm_hash(void const *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uintptr_t hash = 0u;
    for(size_t i = 0; i < size; ++i) {
        hash = bytes[i] + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

HT_PUBDEF uintptr_t ht_knuth_hash(void const *data, size_t size)
{
    uint64_t hash = 0u;
    if (size > sizeof(hash)) size = sizeof(hash);
    ht__memcpy(&hash, data, size);
    hash *=  11400714819323198485u;
    hash >>= (sizeof(hash) - sizeof(uintptr_t))*8;
    return (uintptr_t)hash;
}

HT_PUBDEF uintptr_t ht_id_hash(void const *data, size_t size)
{
    uintptr_t hash = 0u;
    if (size > sizeof(hash)) size = sizeof(hash);
    ht__memcpy(&hash, data, size);
    return (uintptr_t)hash;
}

HT_PUBDEF uintptr_t ht_lose_lose_hash(void const *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t*)data;
    uintptr_t hash = 0;
    for (size_t i = 0; i < size; ++i) {
        hash += bytes[i];
    }
    return hash;
}

static void *ht__find_slot(Ht__Abstract *ht, uintptr_t hash, Ht_Hasheq hasheq, void *key, Ht__Layout l)
{
    HT_ASSERT(!(ht->impl_capacity & (ht->impl_capacity - 1)));
    HT_ASSERT(hash >= HT__FIRST_VALID_HASH);
    size_t mask  = ht->impl_capacity - 1;
    size_t index = hash & mask;
    size_t step  = 1;
    for (size_t i = 0; i < ht->impl_capacity; ++i) {
        uint8_t *slot = (uint8_t*)ht->impl_slots + index*ht__slot_size(l);
        uintptr_t slot_hash = *ht__slot_hash(slot, l);
        if (slot_hash == HT__EMPTY) return slot;
        if (slot_hash == hash && hasheq(HT_EQ, ht__slot_key(slot, l), key, l.key_size)) {
            return slot;
        }
        index = (index + step) & mask;
        step += 1;
    }

    return NULL;
}

static void *ht__put_no_expand(Ht__Abstract *ht, void *key, Ht__Layout l)
{
    Ht_Hasheq hasheq = ht->hasheq ? ht->hasheq : ht_mem_hasheq;
    uintptr_t hash = hasheq(HT_HASH, key, NULL, l.key_size);
    if (hash < HT__FIRST_VALID_HASH) hash += HT__FIRST_VALID_HASH;
    uint8_t *slot = (uint8_t*)ht__find_slot(ht, hash, hasheq, key, l);
    HT_ASSERT(slot != NULL); // Should be taken care of by ht__expand()

    uintptr_t *slot_hash = ht__slot_hash(slot, l);
    switch (*slot_hash) {
    case HT__EMPTY:
        *slot_hash = hash;
        ht->impl_filled_slots += 1;
        ht->count += 1;
        ht__memcpy(ht__slot_key(slot, l), key, l.key_size);
        ht__memcpy(slot, &ht->default_value, l.value_size);
        break;
    case HT__DELETED:
        HT_ASSERT(0 && "UNREACHABLE");
        break;
    default:
        ht__memcpy(slot, &ht->default_value, l.value_size);
    }
    return slot;
}

static void ht__expand(Ht__Abstract *ht, Ht__Layout l)
{
    if (ht->impl_capacity == 0 || ht->impl_filled_slots*100 >= HT__LOAD_FACTOR_PERCENT*ht->impl_capacity) {
        size_t   old_impl_capacity = ht->impl_capacity;
        uint8_t *old_impl_slots    = (uint8_t*)ht->impl_slots;

        ht->impl_capacity = 1;
        while (ht->impl_capacity && ht->impl_capacity < HT__MIN_CAP) {
            ht->impl_capacity <<= 1;
        }
        while (ht->impl_capacity && ht->count*100 >= HT__LOAD_FACTOR_PERCENT*ht->impl_capacity) {
            ht->impl_capacity <<= 1;
        }
        HT_ASSERT(ht->impl_capacity);
        ht->impl_filled_slots = 0;
        ht->count             = 0;
        ht->impl_slots        = HT_MALLOC(ht->impl_capacity*ht__slot_size(l));

        {
            uint8_t *slots_start = (uint8_t*)ht->impl_slots;
            uint8_t *slots_end = slots_start + ht->impl_capacity*ht__slot_size(l);
            for (uint8_t *iter = slots_start; iter < slots_end; iter += ht__slot_size(l)) {
                *ht__slot_hash(iter, l) = HT__EMPTY;
            }
        }

        if (old_impl_capacity) {
            uint8_t *old_slots_start = old_impl_slots;
            uint8_t *old_slots_end   = old_impl_slots + old_impl_capacity*ht__slot_size(l);
            for (uint8_t *old_slot = old_slots_start; old_slot < old_slots_end; old_slot += ht__slot_size(l)) {
                if (*ht__slot_hash(old_slot, l) >= HT__FIRST_VALID_HASH) {
                    void *new_slot = ht__put_no_expand(ht, ht__slot_key(old_slot, l), l);
                    ht__memcpy(new_slot, old_slot, l.value_size);
                }
            }
        }

        HT_FREE(old_impl_slots);
    }
}

static size_t ht__strlen(const char *s)
{
    const char *a = s;
    for (; *s; s++);
    return s-a;
}

static int ht__strcmp(const char *l, const char *r)
{
    for (; *l==*r && *l; l++, r++);
    return *(uint8_t *)l - *(uint8_t *)r;
}

static void *ht__memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (; n; n--) *d++ = *s++;
    return dest;
}

static int ht__memcmp(const void *vl, const void *vr, size_t n)
{
    const uint8_t *l=(const uint8_t *)vl, *r=(const uint8_t *)vr;
    for (; n && *l == *r; n--, l++, r++);
    return n ? *l-*r : 0;
}

#endif // HT_IMPLEMENTATION

/*
   Revision history:

      1.0.1 (2026-04-08) - Compress the internal representation of Ht by 1 machine word.
                         - Fix potential infinite loop in ht__expand when the size of the table reaches the limit of the machine word.
                         - Fix DJB2 implementation
      1.0.0 (2026-04-06) First release
                         - Initial implementation by @rexim
                         - Docs proofreading by @nashiora
                         - Collision bug reports by @laurentiuNiculae and @miezekatze64
                         - Performance review and bug reports by @kam1k4dze
*/

/*
   Version Conventions:

      We are following https://semver.org/ so the version has a format MAJOR.MINOR.PATCH:
      - Modifying comments does not update the version.
      - PATCH is incremented in case of a bug fix or refactoring without touching the API.
      - MINOR is incremented when new functions and/or types are added in a way that does
        not break any existing user code. We want to do this in the majority of the situation.
        If we want to delete a certain function or type in favor of another one we should
        just add the new function/type and deprecate the old one in a backward compatible way
        and let them co-exist for a while.
      - MAJOR update should be just a periodic cleanup of the DEPRECATED functions and types
        without really modifying any existing functionality.
      - Breaking backward compatibility in a MINOR release should be considered a bug and
        should be promptly fixed in the next PATCH release.

   API conventions:

      - All the user facing names should be prefixed with `ht_`, `HT_`, or `Ht_` depending on the case.
      - Internal (private) names should be prefixed with `ht__` (double underscore). The user code is discouraged
        from using such names since they are allowed to be broken in a backward incompatible way even in PATCH
        releases. (This is why they are internal)
*/

/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2026 Alexey Kutepov
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
