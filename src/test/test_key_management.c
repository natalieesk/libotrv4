#include "../key_management.h"

void test_derive_ratchet_keys() {
  shared_secret_t shared = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ratchet_t ratchet;
  otrv4_assert(derive_ratchet_keys(&ratchet, shared) == OTR4_SUCCESS);

  root_key_t expected_root_key;

  gcry_md_hd_t sha3_512;
  uint8_t magic[3] = {0x01, 0x02, 0x03};
  gcry_md_open(&sha3_512, GCRY_MD_SHA3_512, GCRY_MD_FLAG_SECURE);
  gcry_md_write(sha3_512, &magic[0], 1);
  gcry_md_write(sha3_512, shared, sizeof(shared_secret_t));
  memcpy(expected_root_key, gcry_md_read(sha3_512, 0), sizeof(root_key_t));
  gcry_md_close(sha3_512);

  otrv4_assert_cmpmem(expected_root_key, ratchet.root_key, sizeof(root_key_t));
}

void test_key_manager_destroy() {
  OTR4_INIT;

  key_manager_t *manager = malloc(sizeof(key_manager_t));
  key_manager_init(manager);

  shared_secret_t shared = {
      // TODO: is there a simpler way to write this?
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  otrv4_assert(key_manager_new_ratchet(manager, shared) == OTR4_SUCCESS);
  otrv4_assert(key_manager_new_ratchet(manager, shared) ==
               OTR4_SUCCESS); // ran a second time to fill manager->previous

  otrv4_assert(dh_keypair_generate(manager->our_dh) == OTR4_SUCCESS);

  manager->their_dh = dh_mpi_copy(manager->our_dh->pub);

  otrv4_assert(manager->current);
  otrv4_assert(manager->previous);
  otrv4_assert(manager->our_dh->priv);
  otrv4_assert(manager->our_dh->pub);
  otrv4_assert(manager->their_dh);
  // TODO: destroy mix_key too?
  // TODO: test destroy ecdh keys?

  key_manager_destroy(manager);

  otrv4_assert(!manager->current);
  otrv4_assert(!manager->previous);
  otrv4_assert(!manager->our_dh->priv);
  otrv4_assert(!manager->our_dh->pub);
  otrv4_assert(!manager->their_dh);

  dh_free();
  free(manager);
}
