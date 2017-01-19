#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dake.h"
#include "str.h"
#include "serialize.h"

dake_pre_key_t *
dake_pre_key_new(const char *sender) {
  dake_pre_key_t *pre_key = malloc(sizeof(dake_pre_key_t));
  if (pre_key == NULL) {
    fprintf(stderr, "Failed to allocate memory. Chao!\n");
    exit(EXIT_FAILURE);
  }

  pre_key->protocol_version = 4;
  pre_key->message_type = 0x0f;
  pre_key->sender_instance_tag = 1; // TODO: actually compute this value.
  pre_key->receiver_instance_tag = 0;
  pre_key->sender_profile = user_profile_get_or_create_for(sender);
  pre_key->Y = ed448_point_new();
  memset(pre_key->B, 0, 56);

  return pre_key;
}

void
dake_pre_key_free(dake_pre_key_t *pre_key) {
  user_profile_free(pre_key->sender_profile);
  pre_key->sender_profile = NULL;

  ed448_point_free(pre_key->Y);
  pre_key->Y = NULL;

  free(pre_key);
}

void
dake_pre_key_serialize(uint8_t *target, const dake_pre_key_t *pre_key) {
  target += serialize_uint16(target, pre_key->protocol_version);
  target += serialize_uint8(target, pre_key->message_type);
  target += serialize_uint32(target, pre_key->sender_instance_tag);
  target += serialize_uint32(target, pre_key->receiver_instance_tag);
  target += user_profile_serialize(target, pre_key->sender_profile);
  target += serialize_ed448_point(target, pre_key->Y);
  target += serialize_mpi(target, pre_key->B, 56);
}

dake_pre_key_t *
dake_compute_pre_key() {
  return  dake_pre_key_new("handler@service.net");
}