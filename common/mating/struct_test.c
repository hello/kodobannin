#include <stdint.h>

struct mate_target_payload {
  int a;
  int b;
};

union mate_target_payload_bytes {
  struct mate_target_payload target_payload;
  char encrypted_target_payload[sizeof(struct mate_target_payload)];
};

int main(int argc, char** argv)
{
  struct mate_target_payload p;
  p.a;

  union mate_target_payload_bytes u;
  u.target_payload;
  u.encrypted_target_payload;

  return 0;
}
