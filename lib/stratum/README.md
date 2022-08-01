![](https://img.shields.io/badge/version-1.0.0-red)
![https://www.apache.org/licenses/LICENSE-2.0](https://img.shields.io/badge/license-apache--2.0-blue)

# Stratum
Easy to use thread-safe memory allocator, supports Linux, MacOS and Windows.

# 🛠️ How does it work?

```
                                +--+
+-------------------------------+  |
| POOL  | POOL  | POOL  | POOL  |  |
| HEADER| HEADER| HEADER| HEADER|  |
+-------+-------+-------+-------|  |
| BLOCK |       | BLOCK |       |  |
|       | BLOCK +-------+   B   |  |
+-------+       | BLOCK |   I   |  | A
| BLOCK +-------+-------+   G   |  | R
|       |       | BLOCK |       |  | E . . .
+-------+ BLOCK +-------+   B   |  | N
| BLOCK |       | BLOCK |   L   |  | A
|       +-------+-------+   O   |  |
+-------+       | BLOCK |   C   |  |
+-------+ BLOCK +-------+   K   |  |
| ARENA |       | BLOCK |       |  |
+-------------------------------+  |
                                +--+
    ^       ^       ^       ^
    +-------+-------+-------+--- < MEMORY PAGES (STRATUM_PAGE_SIZE)
```

Stratum allocates virtual pages from the operating system, when needed Stratum asks for a block of 64 pages to form an object called Arena. 

Each Arena has a default size of 256 KiB (assumes a page size of 4096 bytes) and each page in the arena represents a single pool, the pools are divided into classes, each class offers fixed size memory blocks.

This way, for example, if a call to Alloc requests a memory block of size 6, a block of size 8 will be returned because the smallest memory class is 8 bytes in size. In total there are 128 different classes (size 8, 16, 24, ... 1024).

If the required memory exceeds the maximum size of 1024, a std::malloc call will be made. In any case, Alloc guarantees that the return address is always aligned with STRATUM_QUANTUM (8 bytes).

# 🚀 Quick start
Stratum provides a default memory allocator, to use it just initialize it and request a block of memory:

```c++
#include <memory.h>

int main(int argc, char **argv) {
  stratum::Initialize();
  
  auto *buf = (unsigned char*) stratum::Alloc(12);
  ...
  ...
  stratum::Free(buf);
```

If you need it, it is also possible to instantiate an object:

```c++
  auto *obj = stratum::AllocObject<MyObject>(param1, param2);
  ...
  ...
  stratum::FreeObject(obj);
}
```

# 🤝 Contribute

All contributions are always welcome

# License
Stratum is primarily distributed under the terms of the Apache License (Version 2.0). 

See [LICENSE](LICENSE)
