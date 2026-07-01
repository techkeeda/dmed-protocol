/*
 * encrypt.h
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#ifndef ENCRYPT_H_
#define ENCRYPT_H_

#include "btclient.h"

void encryption_init(void);
GByteArray* byte_array_get_random(size_t size);
GByteArray* generate_encryption_key(GByteArray *rand_bytes);
GByteArray* encrypt_data(GString *msg);
GByteArray* decrypt_data(GByteArray *cipher);

#endif /* ENCRYPT_H_ */
