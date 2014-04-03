#if defined(CTYPESGEN) && CTYPESGEN
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define ECC_NUM_DIGITS 6
#endif
#include <stdint.h>
#include <util.h>
#else

// [TODO]: app commands?

// [TODO]: Band ID is sent only signed with HELLO_PUBLIC_KEY?

// packet : blob : payload


// Common stuff

/// Errors
enum mate_error {
  MATE_ERROR_OK,
  MATE_ERROR_IMPOSTER_BLOB,
  MATE_ERROR_IMPOSTER_PAYLOAD,
  MATE_ERROR_BAD_PACKET_SIZE,
  MATE_ERROR_BAD_SEQUENCE_NUMBER,
  MATE_ERROR_BAD_NONCE,
  MATE_ERROR_INAPPROPRIATE_COMMAND,
} PACKED;

/// All entities that are involved in communication.
enum mate_entity {
  MATE_ENTITY_SERVER,
  MATE_ENTITY_APP,
  MATE_ENTITY_BAND,
} PACKED;

/// Commands that the server and Band can send to each other.
/** Each enum is prefixed with the target (destination) for the
    command: MATE_COMMAND_SERVER if the command is from the Band to
    the server, or MATE_COMMAND_BAND if the command is from the server
    to the Band. Keep this enum in order of what you would expect the
    communications exchange would be. */
enum mate_command {
  MATE_COMMAND_BAND_INIT,
  MATE_COMMAND_SERVER_REQUEST_BAND_ID,
  MATE_COMMAND_BAND_SEND_ID,
  MATE_COMMAND_SERVER_RECEIVES_BAND_ID_SEND_USER_PUBLIC_KEY,
  MATE_COMMAND_BAND_RECEIVES_USER_PUBLIC_KEY,
  MATE_COMMAND_SERVER_BAND_MATED,
} PACKED;

#define BAND_ID_SIZE 16

union mate_command_data {
  // MATE_COMMAND_BAND_INIT
  // MATE_COMMAND_SERVER_REQUEST_BAND_ID
  // MATE_COMMAND_BAND_SEND_ID
  uint8_t band_id[BAND_ID_SIZE];
  // MATE_COMMAND_SERVER_RECEIVES_BAND_ID_SEND_USER_PUBLIC_KEY
  // MATE_COMMAND_BAND_RECEIVES_USER_PUBLIC_KEY
  uint8_t user_public_key[ECC_NUM_DIGITS];
  // MATE_COMMAND_SERVER_BAND_MATED: should we re-transmit the Band ID?
} PACKED;

/// For ECC signatures.
struct mate_ecc_signature {
  uint32_t r[ECC_NUM_DIGITS];
  uint32_t s[ECC_NUM_DIGITS];
} PACKED;

// [TODO]: maybe need a typedef for nonce, instead of uint32_t


// State

struct mate_band_state {
  uint8_t expected_sequence_number;
  enum mate_command expected_command;
  uint32_t latest_nonce;
  enum mate_entity  expected_source;
};

struct mate_server_state {
  uint8_t expected_sequence_number;
  enum mate_command expected_command;
  uint32_t encrypted_nonce;
  enum mate_entity expected_source;
};


// Payloads

/// Payloads sent between the Band and the server.
struct mate_payload {
  uint32_t band_encrypted_nonce; // Encrypted with band's public key: may need to be larger than 32 bits (possibly 128 bits, since that's the AES-128 block size).
  enum mate_command command;
  union mate_command_data data;
} PACKED;

/** This is expected to be signed with HELLO_PUBLIC_KEY via secp128r1 or secp192r1. */
struct mate_blob {
  enum mate_entity source;
  enum mate_entity target;
  uint8_t sequence_number;
  struct mate_ecc_signature mate_payload_signature;
  struct mate_payload mate_payload; //< May be encrypted/signed; use mate_blob_unwrap() to decrypt this.
} PACKED;

/** This is transmitted directly over-the-wire. */
struct mate_packet {
  struct mate_ecc_signature blob_signature;
  struct mate_blob blob;
} PACKED;



//
