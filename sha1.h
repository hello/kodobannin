#pragma once

#define SHA1_DIGEST_LENGTH 20

void sha1_calc(const void* src, const int bytelength, unsigned char* hash);
