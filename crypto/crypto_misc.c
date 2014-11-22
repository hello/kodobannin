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
#include "nrf51.h"
#include "nrf_error.h"
#include <nrf_soc.h>
#include <nrf_sdm.h>

#define ENTROPY_POOL_SIZE 16
static uint8_t entropy[ENTROPY_POOL_SIZE];
/**
 * If no /dev/urandom, then initialise the RNG with something interesting.
 */
void RNG_custom_init(const uint8_t *seed_buf, int size)
{
    for(int i = 0; i < sizeof(entropy); i++){
        entropy[i] = seed_buf[i];
    }
}

/**
 * Set a series of bytes with a random number. Individual bytes can be 0
 */
void get_random(int num_rand_bytes, uint8_t *rand_data)
{   
    uint8_t pool_size;
    uint8_t radio_entropy[ENTROPY_POOL_SIZE] = {0};
    uint8_t sd;
    if(NRF_SUCCESS == sd_softdevice_is_enabled(&sd) && sd){
        if(NRF_SUCCESS == sd_rand_application_bytes_available_get(&pool_size)){
            sd_rand_application_vector_get(radio_entropy, (pool_size < ENTROPY_POOL_SIZE)?pool_size:ENTROPY_POOL_SIZE);
        }
    }else{
        //no random number generator
        NRF_RNG->TASKS_START = 1;
        for(int i = 0; i < ENTROPY_POOL_SIZE; i++){
            while(NRF_RNG->EVENTS_VALRDY == 0){};
            radio_entropy[i] = NRF_RNG->VALUE;
        }
        NRF_RNG->TASKS_STOP = 1;
    }
    for(int i = 0; i < ENTROPY_POOL_SIZE; i++){
        entropy[i] ^= radio_entropy[i];
    }
    RC4_CTX rc4;
    RC4_setup(&rc4, entropy, ENTROPY_POOL_SIZE);
    RC4_crypt(&rc4, rand_data, rand_data, num_rand_bytes);
    RC4_crypt(&rc4, entropy, entropy, ENTROPY_POOL_SIZE);
}

/**
 * Set a series of bytes with a random number. Individual bytes are not zero.
 */
void get_random_NZ(int num_rand_bytes, uint8_t *rand_data)
{
    int i;
    get_random(num_rand_bytes, rand_data);
    for(i = 0; i < num_rand_bytes; i++){
        while(rand_data[i] == 0){
            get_random(1, &rand_data[i]);
        }
    }
}


