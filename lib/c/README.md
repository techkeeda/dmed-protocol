# DMED C Library

Single-header, zero-allocation C library for DMED beacon encode/decode.

## Features
- **Single header** — just `#include "dmed.h"`
- **Zero heap allocation** — all stack-based, safe for embedded
- **~2KB compiled** — fits on any microcontroller
- **No dependencies** — only `<stdint.h>`, `<stddef.h>`, `<string.h>`

## Install

Copy `include/dmed.h` into your project.

## Usage

```c
#define DMED_IMPLEMENTATION  // in exactly ONE .c file
#include "dmed.h"

// Encode beacon
dmed_beacon_t beacon = {
    .version = 1,
    .flags = DMED_FLAG_MULTI,
    .service_type = DMED_ST_IOT_DEVICE,
    .instance_id = 0xA1B2C3D4,
};
uint8_t buf[9];
int len = dmed_beacon_encode(&beacon, buf, sizeof(buf));

// Decode beacon
dmed_beacon_t decoded;
dmed_beacon_decode(buf, len, &decoded);
```

## Build test

```bash
cd src
gcc -o dmed_test dmed_test.c -I../include && ./dmed_test
```
