#include "alloc.h"
#include "token.h"
int main() {
    __builtin_trap();
}
// the "home" of the normally-inlined small utility functions defined in
// various project headers

extern inline void destroy_token(Token);
extern inline void destroy_token_sequence(struct token_sequence);

[[gnu::returns_nonnull]]
extern inline void *checked_calloc(size_t, size_t);
[[gnu::returns_nonnull]]
extern inline void *checked_malloc(size_t);
