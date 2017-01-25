#include "dh.h"
#include "random.h"

static const char* DH3072_MODULUS_S = "0x"
  "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
  "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
  "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
  "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
  "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
  "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
  "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
  "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
  "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
  "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
  "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
  "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
  "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
  "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
  "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
  "43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF";

static gcry_mpi_t DH3072_MODULUS = NULL;

static const char *DH3072_GENERATOR_S = "0x02";
static gcry_mpi_t DH3072_GENERATOR = NULL;

void
dh_init(void) {
  gcry_mpi_scan(&DH3072_MODULUS, GCRYMPI_FMT_HEX,
    (const unsigned char *)DH3072_MODULUS_S, 0, NULL);

  gcry_mpi_scan(&DH3072_GENERATOR, GCRYMPI_FMT_HEX,
    (const unsigned char *)DH3072_GENERATOR_S, 0, NULL);
}

bool
dh_gen_keypair(dh_keypair_t keypair) {
  uint8_t *secbuf = malloc(DH_KEY_SIZE);
  if (secbuf == NULL) {
      return false;
  }

  random_bytes(secbuf, DH_KEY_SIZE);
  gcry_error_t err = gcry_mpi_scan(&keypair->priv, GCRYMPI_FMT_USG, secbuf, DH_KEY_SIZE, NULL);
  gcry_free(secbuf);

  if (err) {
    return false;
  }

  keypair->pub = gcry_mpi_new(DH3072_MOD_LEN_BITS);
  gcry_mpi_powm(keypair->pub, DH3072_GENERATOR, keypair->priv, DH3072_MODULUS);

  return true;
}

void
dh_keypair_destroy(dh_keypair_t keypair) {
  gcry_mpi_release(keypair->priv);
  gcry_mpi_release(keypair->pub);
}

bool
dh_shared_secret(
    uint8_t *shared,
    size_t shared_bytes,
    const dh_private_key_t our_priv,
    const dh_public_key_t their_pub
) {
  gcry_mpi_t secret = gcry_mpi_new(DH3072_MOD_LEN_BITS);
  gcry_mpi_powm(secret, their_pub, our_priv, DH3072_MODULUS);
  gcry_error_t err = gcry_mpi_print(GCRYMPI_FMT_USG, shared, shared_bytes, NULL, secret);
  gcry_mpi_release(secret);

  if (err) {
      return false;
  }

  return true;
}

size_t
dh_mpi_serialize(uint8_t *dst, size_t dst_len, const dh_mpi_t src) {
  size_t nwritten = 0;
  gcry_mpi_print(GCRYMPI_FMT_USG, dst, dst_len, &nwritten, src); //TODO: fail?
  return nwritten;
}