// vi:noet:sw=4 ts=4

#include <app_error.h>
#include <app_timer.h>
#include <stdlib.h>
#include <stdint.h>
#include <nrf_soc.h>
#include <string.h>
#include <simple_uart.h>
#include "nrf51.h"
#include "nrf_ecb.h"
#include <nrf_sdm.h>

#include "util.h"

static void
_ctr_inc_ctr(nrf_ecb_hal_data_t * ecb){
	uint64_t * ctr = (uint64_t*)(ecb->cleartext);
	ctr[1]++;
}
const uint8_t *
get_aes128_key(void){
	static uint8_t key[16] = {0};
	memcpy(key, NRF_FICR->ER, sizeof(key));
	return key;
}
uint32_t
aes128_ctr_decrypt_inplace(uint8_t * message, uint32_t message_size, const uint8_t * key, const uint8_t * nonce){
	aes128_ctr_encrypt_inplace(message, message_size, key, nonce);
	
}
uint32_t
aes128_ctr_encrypt_inplace(uint8_t * message, uint32_t message_size, const uint8_t * key, const uint8_t * nonce){
	nrf_ecb_hal_data_t ecb;
	uint32_t * write_ptr = (uint32_t*)message;
	uint32_t * read_ptr = (uint32_t*)ecb.ciphertext;
	uint32_t written = 0;
	uint8_t has_sd;
	sd_softdevice_is_enabled(&has_sd);
	//set key
	memcpy(ecb.key, key, AES128_BLOCK_SIZE);
	//set counter
	memcpy(ecb.cleartext, (uint8_t*)nonce, 8);
	memset(ecb.cleartext+8, 0, 8);
	if(!has_sd){
		nrf_ecb_init();
		nrf_ecb_set_key(key);
	}
	while(message_size >= AES128_BLOCK_SIZE){
		//do one operation
		if(!has_sd){
			if(!nrf_ecb_crypt(read_ptr, write_ptr)){
				return 1;
			}
		}else{
			if(NRF_SUCCESS != sd_ecb_block_encrypt(&ecb)){
				return 1;
			}
		}
		//output = input xor aes output
		write_ptr[0] = write_ptr[0] ^ read_ptr[0];
		write_ptr[1] = write_ptr[1] ^ read_ptr[1];
		write_ptr[2] = write_ptr[2] ^ read_ptr[2];
		write_ptr[3] = write_ptr[3] ^ read_ptr[3];
		write_ptr += 4;
		written += 16;
		message_size -= AES128_BLOCK_SIZE;
		_ctr_inc_ctr(&ecb);
	}
	if(message_size > 0){
		if(!has_sd){
			if(!nrf_ecb_crypt(read_ptr, write_ptr)){
				return 1;
			}
		}else{
			if(NRF_SUCCESS != sd_ecb_block_encrypt(&ecb)){
				return 1;
			}
		}
		read_ptr[0] = write_ptr[0] ^ read_ptr[0];
		read_ptr[1] = write_ptr[1] ^ read_ptr[1];
		read_ptr[2] = write_ptr[2] ^ read_ptr[2];
		read_ptr[3] = write_ptr[3] ^ read_ptr[3];
		memcpy((uint8_t*)write_ptr, (uint8_t*)read_ptr, message_size);
		written += message_size;
	}
	PRINTS("AES: ");
	PRINT_HEX(&written, 2);
	PRINTS(" bytes\r\n");
	return 0;
}
void *
memcpy(void *s1, const void *s2, size_t n)
{
	size_t i;
	if (!s1 || !s2)
		return NULL;

	void *orig = s1;

	for (i = 0; i < n; ++i, ++s1, ++s2)
		*(char *)s1 = *(char *)s2;

	return orig;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
	const char* a = s1;
	const char* b = s2;

	if (n == 0)
		return 0;

	while (n-- >0)
		if(*a++ != *b++)
			return 1;

	return 0;
}

void *
memset(void *b, int c, size_t len)
{
	void *orig = b;
	char val = (char)c;

	while (len-- > 0)
		*(char *)b++ = val;

	return orig;
}

uint8_t
memsum(void *start, unsigned len)
{
	uint8_t *p = start;
	uint8_t i = 0;

	while(len-- > 0) {
		i += *p++;
	}

	return ~i;
}

size_t
strlen(const char *a)
{
	size_t count = 0;

	while (*a++)
		++count;

	return count;
}

inline int puts(const char *str) {
    simple_uart_putstring((const uint8_t *)str);
    return 0;
}

const uint8_t hex[] = "0123456789ABCDEF";

int nrf_atoi(char *p){
	int n = 0, f = 0;
	for(;; p++){
		switch(*p){
			case ' ':
			case '\t':
				continue;
			case '-':
				f++;
			case '+':
				p++;
		}
		break;
	}
	while(*p >= '0' && *p <= '9'){
		n = n * 10 + *p++ - '0';
	}
	return (f ? -n : n);
}

#if defined(DEBUG_SERIAL) || defined(PLATFORM_HAS_SERIAL_CROSS_CONNECT)
void
serial_print_hex(uint8_t *ptr, uint32_t len) {
	while(len-- >0) {
		simple_uart_put(hex[0xF&(*ptr>>4)]);
		simple_uart_put(hex[0xF&*ptr++]);
		simple_uart_put(' ');
	}
}

void
binary_to_hex(uint8_t *ptr, uint32_t len, uint8_t* out) {
	while(len-- >0) {
		*(out++) = hex[0xF&(*ptr>>4)];
		*(out++) = hex[0xF&*ptr++];
	}
}

void
debug_print_ticks(const char* const message, uint32_t start_ticks, uint32_t stop_ticks)
{
	uint32_t diff_ticks;

	uint32_t err = app_timer_cnt_diff_compute(stop_ticks, start_ticks, &diff_ticks);
	APP_ERROR_CHECK(err);

	DEBUG(message, diff_ticks);
}
#else
void
serial_print_hex(uint8_t *ptr, uint32_t len)
{
}

void
debug_print_ticks(const char* const message, uint32_t start_ticks, uint32_t stop_ticks)
{
}
#endif
