# Hash Table in C

It's just a Hash Table in C but with a nice Generic API.

## Quick Start

```c
#include <stdio.h>
#define HT_IMPLEMENTATION
#include "ht.h"

int main(void)
{
    Ht(const char *, int) ht = {
        .hasheq = ht_cstr_hasheq,
    };
    *ht_put(&ht, "foo") = 69;
    *ht_put(&ht, "bar") = 420;
    *ht_put(&ht, "baz") = 1337;
    ht_foreach(value, &ht) {
        printf("%s => %d\n", ht_key(&ht, value), *value);
    }
    ht_free(&ht);
    return 0;
}
```
