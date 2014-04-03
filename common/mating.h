enum band_to_server_commands {
  INITIATE_MATE, // data: nonce1
  MATING_START, // data: nonce2 + band ID
};

enum server_to_band_commands {
  SEND_BAND_ID, // data: nonce1
  MATE, // data: nonce2 + user public key
};

enum server_to_app_commands {
  MATED, // data: band ID? or this could be in band_to_app_commands...
};


enum mate_destination {
  BAND,
  SERVER,
  APP,
};

struct comms {
  enum mate_destination destination;
  uint16_t blob_length; // always signed with hello public key
};
