#include <glib.h>
#include <string.h>

#include "../dake.h"
#include "../serialize.h"

void
test_dake_pre_key_new() {
  dake_pre_key_t *pre_key = dake_pre_key_new("handler@service.net");

  g_assert_cmpint(pre_key->protocol_version, ==, 4);
  g_assert_cmpint(pre_key->message_type, ==, 15);
  g_assert_cmpint(pre_key->sender_instance_tag, >, 0);
  g_assert_cmpint(pre_key->receiver_instance_tag, ==, 0);
  // TODO: How to assert a pointer was set without using nonnull?
  // Comparing to 0 fires a warning on making a int from a pointer
  // even when NULL is a representation of 0
  // g_assert_cmpint(pre_key->Y, >, 0);
  // g_assert_cmpint(pre_key->B, >, 0);

  dake_pre_key_free(pre_key);
}

void
test_dake_pre_key_serializes() {
  dake_pre_key_t *pre_key = dake_pre_key_new("handler@service.net");
  uint8_t serialized[1000] = { 0 };
  
  dake_pre_key_serialize(serialized, pre_key);

  char expected[] = {
    0x0, 0x04, // version
    0x0f,      //message type
    0x0, 0x0, 0x0, 0x1, // sender instance tag
    0x0, 0x0, 0x0, 0x0, // receiver instance tag
    // user profile will be verified but not in expected
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,// Y
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x38,                   // B
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
  };

  int comp = memcmp(serialized, expected, 11); //sizeof(expected));
  g_assert_cmpint(comp, ==, 0);

  uint8_t user_profile_serialized[340] = {0};
  int user_profile_len = user_profile_serialize(user_profile_serialized, pre_key->sender_profile);
  g_assert_cmpint(memcmp(serialized+11, user_profile_serialized, user_profile_len), ==, 0);

  uint8_t serialized_y[56] = {0};
  serialize_ed448_point(serialized_y, pre_key->Y);
  g_assert_cmpint(memcmp(serialized+11+user_profile_len, serialized_y, 56), ==, 0);

  uint8_t serialized_b[60] = {0};
  serialize_mpi(serialized_b, pre_key->B, 56);
  g_assert_cmpint(memcmp(serialized+11+user_profile_len+56, serialized_b, 60), ==, 0);

  dake_pre_key_free(pre_key);
}
int
main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/dake_pre_key_new", test_dake_pre_key_new);
  g_test_add_func("/dake_pre_key_serializes", test_dake_pre_key_serializes);

  return g_test_run();
}
