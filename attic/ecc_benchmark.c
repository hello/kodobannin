// vi:noet:sw=4 ts=4

#include <string.h>

#include <nrf51.h>

#define uECC_CURVE uECC_secp160r1

#include "uECC.h"
#include "sha1.h"
#include "util.h"

#if uECC_CURVE == 4

static uint32_t private_0[NUM_ECC_DIGITS] = {0xA900C5AF, 0xFD4D7240, 0x108DE95B, 0x8FAF8AD1};
static EccPoint public_0 = {
	{0xE25543F6, 0x4F47E347, 0x81AD9E85, 0x8C6F3723},
    {0x1EF5C159, 0xB1A85205, 0xF72EE0F3, 0x040E28B4}};
//static uint32_t _1_private[NUM_ECC_DIGITS] = {0x32970B72, 0xB66AA511, 0x1AF0CAC8, 0x167374FD};
static EccPoint public_1 = {
	{0xF686BE87, 0xFFA3BAC7, 0x3E131BF4, 0x6A431448},
    {0x277C9196, 0xDF8A5581, 0x29AF0A89, 0xF8832A32}};

#elif uECC_CURVE == 6

static uint32_t private_0[NUM_ECC_DIGITS] = {0x24CC0981, 0x426997E1, 0x87A7A90E, 0x8FD5DA9D, 0x87D0CDAD, 0x489B0F3B};
static EccPoint public_0 = {
	{0x0D79B8D6, 0xC3D57787, 0x924EA25B, 0xB6E3554B, 0x9437990C, 0xF301C99B},
    {0x56F1A202, 0x90405844, 0xE621B15E, 0x9586D0CE, 0xEE7096EE, 0xA91DCEC7}};
//static uint32_t _1_private[NUM_ECC_DIGITS] = {0xA88AD66A, 0x9D7F0D80, 0xB9940FD8, 0x03C1BF9C, 0x085ECAE1, 0x3E8B7F8F};
static EccPoint public_1 = {
	{0xDDB31CB8, 0xF4373C76, 0x10D9DB46, 0xE1DD295D, 0x436B987A, 0x17E9B1C7},
    {0xE1A02F9D, 0xC002D249, 0x7CB5C6C0, 0x5B62BE4B, 0xE9E81017, 0xB9FC7041}};

#elif uECC_CURVE == 12

static uint32_t private_0[NUM_ECC_DIGITS] = {0x1340E83E, 0x6F0F2B9F, 0x2390C740, 0xA35B380C, 0x3740C400, 0x6DE75C8B, 0xE868AF31, 0x47D69CA5, 0x54B7A33D, 0x1C1F0DD5, 0xFF7FF7BA, 0x06323156};
static EccPoint public_0 = {
	{0xC9FABA69, 0xD44738AD, 0x48BA998B, 0xA3CA407E, 0xA311A05E, 0xACDE4CB5, 0xBF84C15A, 0xCC5FE2E6, 0xE89BAC48, 0xB316F3C2, 0xFE5E5B52, 0x83E44C8C},
    {0x22C61C1E, 0x72708801, 0xE27650C7, 0x6422CD70, 0xFB3BDF1C, 0x9B0D2F2F, 0x3F268B03, 0x273F1846, 0x982C7FBD, 0x42E9EE05, 0xE3584164, 0x0D52A006}};
//static uint32_t private_1[NUM_ECC_DIGITS] = {0x37AC8DF0, 0xD51ADAE3, 0x096693A1, 0x57D215F3, 0x393F3D20, 0x78A6F1EE, 0xA099EBDD, 0xA11D0E86, 0xF26CA64F, 0x4941C4D4, 0x880E2642, 0xF87DF6B2};
static EccPoint public_1 = {
    {0xE29797B0, 0xB4A757E3, 0xDF72978E, 0x689871FD, 0xF8163BAE, 0x3F8212ED, 0x89E268E7, 0x1FA840A3, 0xAF774A8A, 0x7ED709CD, 0x49BF74F6, 0x92F8A05C},
    {0x61EAA726, 0x07D8838C, 0x32EF9492, 0xF52938B7, 0xAD79F96C, 0xD31D2390, 0x7FBB2F0B, 0x10017AA0, 0x6D82DB7A, 0x5DE01886, 0x44F5FA69, 0xC0C90B50}};

#else
#error Need to define uECC_CURVE (e.g. -DuECC_CURVE=6)
#endif

#define NUM_ECC_DIGITS uECC_CURVE

void ecc_benchmark()
{
	int success;
	uint32_t start_ticks, stop_ticks;
	EccPoint public_key;
	uint32_t private_key;
	uint32_t shared_secret[NUM_ECC_DIGITS];
	uint8_t message_hash[NUM_ECC_DIGITS*sizeof(uint32_t)];
	uint32_t r[NUM_ECC_DIGITS], s[NUM_ECC_DIGITS];

	memset(message_hash, 0xAC, sizeof(message_hash));

	static const uint8_t ecc_curve = uECC_CURVE;
	DEBUG("ECC curve (number of digits): ", ecc_curve);

	static uint32_t RANDOMS[12] = { 398201, 5489012, 3217890, 3218902, 4789312, 4312789, 398201, 5489012, 3217890, 3218902, 4789312, 4312789 };

	{
		NRF_RTC1->TASKS_START = 1;

		start_ticks = NRF_RTC1->COUNTER;
		success = ecc_make_key(&public_key, &private_key, RANDOMS);
		stop_ticks = NRF_RTC1->COUNTER;

		if(success) {
			debug_print_ticks("ecc_make_key ticks: ", start_ticks, stop_ticks);
		} else {
			PRINTS("ecc_make_key failed\r\n");
			return;
		}
	}

	//

	{
		start_ticks = NRF_RTC1->COUNTER;
		ecc_valid_public_key(&public_key);
		stop_ticks = NRF_RTC1->COUNTER;

		debug_print_ticks("ecc_valid_public_key ticks: ", start_ticks, stop_ticks);
	}

	//


	{
		start_ticks = NRF_RTC1->COUNTER;
		ecdh_shared_secret(shared_secret, &public_1, private_0, RANDOMS);
		stop_ticks = NRF_RTC1->COUNTER;

		if(success) {
			debug_print_ticks("ecdh_shared_secret ticks: ", start_ticks, stop_ticks);
		} else {
			PRINTS("ecdh_shared_secret failed\r\n");
		}
	}

	//

	{
		static const char* const message = "Woot secret message!";
		sha1_calc(message, sizeof(message), message_hash);

		start_ticks = NRF_RTC1->COUNTER;
		success = ecdsa_sign(r, s, private_0, RANDOMS, (uint32_t*)message_hash);
		stop_ticks = NRF_RTC1->COUNTER;

		if(success) {
			debug_print_ticks("ecdsa_sign ticks: ", start_ticks, stop_ticks);
		} else {
			PRINTS("ecdsa_sign failed\r\n");
		}
	}

	//

	{
		start_ticks = NRF_RTC1->COUNTER;
		success = ecdsa_verify(&public_0, (uint32_t*)message_hash, r, s);
		stop_ticks = NRF_RTC1->COUNTER;

		if(success) {
			debug_print_ticks("ecdsa_verify ticks: ", start_ticks, stop_ticks);
		} else {
			PRINTS("ecdsa_verify failed\r\n");
		}
	}

	NRF_RTC1->TASKS_STOP = 1;
}
