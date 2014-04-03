static struct mate_band_state _mate_state;

void mate_error(enum mate_error error_code)
{
  // [TODO]
}

void mate_init()
{
  _mate_state.expected_sequence_number = 0;
  _mate_state.expected_command = MATE_COMMAND_BAND_INIT;
  _mate_state.expected_source = MATE_ENTITY_SERVER;
}

static
enum mate_error _validate_packet(mate_packet* packet)
{
  uint8_t blob_sha1[SHA1_DIGEST_LENGTH];
  sha1_calc(AS_BYTES(&packet->blob), sizeof(blob), blob_sha1);

  int blob_verified = ecdsa_verify(HELLO_PUBLIC_KEY, blob_sha1, packet->r, packet->s);
  if(blob_verified) {
    return MATE_ERROR_OK;
  } else {
    return MATE_ERROR_IMPOSTER_BLOB;
  }
}

void mate_packet_unwrap(mate_packet* packet)
{
  switch(blob->mate_payload->command) {
  case MATE_COMMAND_BAND_RECEIVES_USER_PUBLIC_KEY:
    decrypt(blob->BAND_PRIVATE_KEY, &blob->mate_payload, sizeof(blob->mate_payload));
    break;
  case MATE_COMMAND_SERVER_BAND_MATED:
    decrypt(blob->BAND_PRIVATE_KEY, &blob->mate_payload, sizeof(blob->mate_payload));
    break;
  default:
    break;
  }
}

void mate_send_blob(enum mate_entity entity, enum mate_command command, union mate_command_data* command_data)
{
  struct mate_payload payload;

  payload.source = MATE_ENTITY_BAND;
  payload.sequence_number = _mate_state.expected_sequence_number;

  _mate_state.latest_nonce = cryptographic_random();

  payload.payload.band_encrypted_nonce = encrypt(BAND_PUBLIC_KEY, _mate_state.latest_nonce, sizeof(_mate_state.latest_nonce));
  payload.payload.command = command;
  if(command_data) {
    memcpy(payload.payload.data, command_data, sizeof(command_data));
  }

  struct mate_blob blob;
  blob.target = entity;
  blob.encrypted_payload = encrypt(HELLO_PUBLIC_KEY, payload, sizeof(payload)+command_data_size);

  // send blob via BLE here
}

// Assume that we call this function when we receive some data from the app. bytes is expected to be a pointer to the bytes from the BLE stack; size is the total number of bytes received. Both arguments are "trusted", in the sense that we trust the BLE stack to supply the correct information for both parameters.
void mate_received_blob(char* bytes, unsigned size)
{
  if(size != sizeof(struct mate_packet)) {
    mate_error(MATE_ERROR_BAD_PACKET_SIZE);
  }

  struct* mate_packet packet = (struct mate_packet*)bytes;
  enum mate_error err = _validate_packet(packet);
  if(err) {
    mate_error(err);
    return;
  }

  struct mate_blob* blob = &(packet->blob);

  if(blob->target != MATE_ENTITY_BAND) {
    mate_error(MATE_ERROR_INCORRECT_ENTITY);
  }

  mate_blob_unwrap(blob);

  uint8_t encrypted_mate_payload_sha1[SHA1_DIGEST_LENGTH];
  sha1_calc(blob->mate_payload, sizeof(blob->mate_payload), encrypted_mate_payload_sha1);

  int mate_payload_verified = ecdsa_verify(MATE_PUBLIC_KEY, encrypted_mate_payload_sha1, blob->r, blob->s);
  if(!mate_payload_verified) {
    mate_error(MATE_ERROR_IMPOSTER_MATE_PAYLOAD);
  }

  decrypt(MATE_PRIVATE_KEY, blob->mate_payload, sizeof(blob->mate_payload));

  // blob->mate_payload should now be decrypted

  struct mate_payload* mate_payload = &(blob->mate_payload);

  if(mate_payload->sequence_number != _mate_state.expected_sequence_number) {
    mate_error(MATE_ERROR_BAD_SEQUENCE_NUMBER);
  }

  expected_sequence_number++;

  if(mate_payload->source != _mate_state.expected_source) {
    mate_error(MATE_ERROR_BAD_SOURCE);
  }

  uint8_t encrypted_payload_sha1[SHA1_DIGEST_LENGTH];
  sha1_calc(mate_payload->payload, sizeof(payload), encrypted_payload_sha1);

  int payload_verified = ecdsa_verify(BAND_PUBLIC_KEY, encrypted_payload_sha1, mate_payload->r, mate_payload->s);
  if(!payload_verified) {
    mate_error(MATE_ERROR_IMPOSTER_PAYLOAD);
  }

  struct payload* payload = &(mate_payload->payload);

  uint32_t nonce = decrypt(BAND_PRIVATE_KEY, payload->band_encrypted_nonce, sizeof(payload->band_encrypted_nonce));
  if(nonce != _mate_state.latest_nonce) {
    mate_error(MATE_ERROR_BAD_NONCE);
  }

  switch(payload->command) {
  case MATE_COMMAND_BAND_INIT:
    _mate_state.expected_command = MATE_COMMAND_BAND_SEND_ID;
    mate_send_blob(MATE_ENTITY_SERVER, MATE_COMMAND_SERVER_REQUEST_BAND_ID, NULL);
    break;
  case MATE_COMMAND_BAND_SEND_ID:
    _mate_state.expected_command = MATE_COMMAND_BAND_RECEIVES_USER_PUBLIC_KEY;
    mate_send_blob(MATE_ENTITY_SERVER, MATE_COMMAND_SERVER_RECEIVES_BAND_ID_SEND_USER_PUBLIC_KEY, BAND_ID);
    break;
  case MATE_COMMAND_BAND_RECEIVES_USER_PUBLIC_KEY:
    memcpy(user_public_key, payload->data, sizeof(payload->data->user_public_key));
    mate_send_blob(MATE_ENTITY_SERVER, MATE_COMMAND_SERVER_BAND_MATED);
    break;
  default:
    mate_error(MATE_ERROR_INAPPROPRIATE_COMMAND);
    break;
  }

}
