/*
 * Copyright (c) 2007, Cameron Rich
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the axTLS project nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Some misc. routines to help things out
 */

#include <stdint.h>

#include "crypto.h"


/**
 * If no /dev/urandom, then initialise the RNG with something interesting.
 */
void RNG_custom_init(const uint8_t *seed_buf, int size)
{
}

/**
 * Set a series of bytes with a random number. Individual bytes can be 0
 */
void get_random(int num_rand_bytes, uint8_t *rand_data)
{   
 /* nothing else to use, so use a custom RNG */
    /* The method we use when we've got nothing better. Use RC4, time 
       and a couple of random seeds to generate a random sequence */
    int i;
    for(i = 0; i < num_rand_bytes; i++){
        rand_data[i] = i;
    }

}

/**
 * Set a series of bytes with a random number. Individual bytes are not zero.
 */
void get_random_NZ(int num_rand_bytes, uint8_t *rand_data)
{
    for(i = 0; i < num_rand_bytes; i++){
        rand_data[i] = (i%128)+1;
    }
}


