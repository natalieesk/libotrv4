#include <string.h>

#include "auth.h"
#include "constants.h"
#include "deserialize.h"
#include "dh.h"
#include "mpi.h"
#include "random.h"
#include "serialize.h"
#include "smp.h"
#include "str.h"
#include "tlv.h"

#include "debug.h"

void smp_context_init(smp_context_t smp) {
  smp->state = SMPSTATE_EXPECT1;
  smp->progress = 0;
  smp->msg1 = NULL;
  smp->secret = NULL;

  ec_scalar_destroy(smp->a2);
  ec_scalar_destroy(smp->a3);
  ec_scalar_destroy(smp->b3);

  ec_point_destroy(smp->G2);
  ec_point_destroy(smp->G3);
  ec_point_destroy(smp->G3a);
  ec_point_destroy(smp->G3b);
  ec_point_destroy(smp->Pb);
  ec_point_destroy(smp->Qb);
  ec_point_destroy(smp->Pa_Pb);
  ec_point_destroy(smp->Qa_Qb);
}

void smp_destroy(smp_context_t smp) {
  free(smp->secret);
  smp->secret = NULL;

  smp_msg_1_destroy(smp->msg1);
  free(smp->msg1);
  smp->msg1 = NULL;

  ec_scalar_destroy(smp->a2);
  ec_scalar_destroy(smp->a3);
  ec_scalar_destroy(smp->b3);

  ec_point_destroy(smp->G2);
  ec_point_destroy(smp->G3);
  ec_point_destroy(smp->G3a);
  ec_point_destroy(smp->G3b);
  ec_point_destroy(smp->Pb);
  ec_point_destroy(smp->Qb);
  ec_point_destroy(smp->Pa_Pb);
  ec_point_destroy(smp->Qa_Qb);
}

void generate_smp_secret(unsigned char **secret, otrv4_fingerprint_t our_fp,
                         otrv4_fingerprint_t their_fp, uint8_t *ssid,
                         const uint8_t *answer, size_t answerlen) {
  uint8_t version[1] = {0x01};

  uint8_t hash[HASH_BYTES];
  decaf_shake256_ctx_t hd;
  hash_init_with_dom(hd);
  hash_update(hd, version, 1);
  hash_update(hd, our_fp, sizeof(otrv4_fingerprint_t));
  hash_update(hd, their_fp, sizeof(otrv4_fingerprint_t));
  hash_update(hd, ssid, sizeof(ssid));
  hash_update(hd, answer, answerlen);

  hash_final(hd, hash, sizeof(hash));
  hash_destroy(hd);

  *secret = malloc(HASH_BYTES);
  if (!*secret)
    return;

  memcpy(*secret, hash, HASH_BYTES);
}

int hashToScalar(const unsigned char *buff, const size_t bufflen,
                 ec_scalar_t dst) {
  gcry_md_hd_t hd;
  char otr_marker[4] = "OTR4";
  unsigned char hash[HASH_BYTES];

  gcry_md_open(&hd, GCRY_MD_SHA3_512, GCRY_MD_FLAG_SECURE);
  gcry_md_write(hd, &otr_marker, 4);
  gcry_md_write(hd, buff, bufflen);
  memcpy(hash, gcry_md_read(hd, 0), HASH_BYTES);
  gcry_md_close(hd);

  if (deserialize_ec_scalar(dst, hash, ED448_SCALAR_BYTES))
    return 1;

  return 0;
}

void smp_msg_1_destroy(smp_msg_1_t *msg) {
  if (!msg)
    return;

  free(msg->question);
  msg->question = NULL;
  msg->q_len = 0;

  ec_point_destroy(msg->G2a);
  ec_point_destroy(msg->G3a);

  ec_scalar_destroy(msg->c2);
  ec_scalar_destroy(msg->c3);
  ec_scalar_destroy(msg->d2);
  ec_scalar_destroy(msg->d3);
}

otr4_err_t generate_smp_msg_1(smp_msg_1_t *dst, smp_context_t smp) {
  snizkpk_keypair_t pair_r2[1], pair_r3[1];
  unsigned char hash[ED448_POINT_BYTES + 1];
  ec_scalar_t a3c3, a2c2;

  dst->q_len = 0;
  dst->question = NULL;

  generate_keypair(dst->G2a, smp->a2);
  generate_keypair(dst->G3a, smp->a3);

  snizkpk_keypair_generate(pair_r2);
  snizkpk_keypair_generate(pair_r3);

  hash[0] = 0x01;
  if (serialize_ec_point(hash + 1, pair_r2->pub))
    return OTR4_ERROR;

  if (hashToScalar(hash, sizeof(hash), dst->c2))
    return OTR4_ERROR;

  decaf_448_scalar_mul(a2c2, smp->a2, dst->c2);
  decaf_448_scalar_sub(dst->d2, pair_r2->priv, a2c2);

  hash[0] = 0x02;
  if (serialize_ec_point(hash + 1, pair_r3->pub))
    return OTR4_ERROR;

  if (hashToScalar(hash, sizeof(hash), dst->c3))
    return OTR4_ERROR;

  decaf_448_scalar_mul(a3c3, smp->a3, dst->c3);
  decaf_448_scalar_sub(dst->d3, pair_r3->priv, a3c3);

  return OTR4_SUCCESS;
}

otr4_err_t smp_msg_1_asprintf(uint8_t **dst, size_t *len,
                              const smp_msg_1_t *msg) {
  size_t s = 0;

  s += 4;
  s += msg->q_len;
  s += 2 * ED448_POINT_BYTES;
  s += 4 * ED448_SCALAR_BYTES;

  *dst = malloc(s);
  if (!*dst)
    return OTR4_ERROR;

  uint8_t *cursor = *dst;

  cursor += serialize_data(cursor, (uint8_t *)msg->question, msg->q_len);

  if (serialize_ec_point(cursor, msg->G2a))
    return OTR4_ERROR;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->c2);
  cursor += serialize_ec_scalar(cursor, msg->d2);

  if (serialize_ec_point(cursor, msg->G3a))
    return OTR4_ERROR;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->c3);
  cursor += serialize_ec_scalar(cursor, msg->d3);

  *len = s;

  return OTR4_SUCCESS;
}

otr4_err_t generate_smp_msg_2(smp_msg_2_t *dst, const smp_msg_1_t *msg_1,
                              smp_context_t smp) {
  ec_scalar_t b2;
  snizkpk_keypair_t pair_r2[1], pair_r3[1], pair_r4[1], pair_r5[1];
  ec_scalar_t r6;
  unsigned char buff[ED448_POINT_BYTES + 1];
  ec_scalar_t temp_scalar;
  ec_point_t temp_point;

  generate_keypair(dst->G2b, b2);
  generate_keypair(dst->G3b, smp->b3);

  snizkpk_keypair_generate(pair_r2);
  snizkpk_keypair_generate(pair_r3);
  snizkpk_keypair_generate(pair_r4);
  snizkpk_keypair_generate(pair_r5);

  ed448_random_scalar(r6);

  /* c2 */
  buff[0] = 0x03;
  if (serialize_ec_point(buff + 1, pair_r2->pub))
    return OTR4_ERROR;

  if (hashToScalar(buff, ED448_POINT_BYTES + 1, dst->c2))
    return OTR4_ERROR;

  /* d2 = (r2 - b2 * c2 mod q). */
  decaf_448_scalar_mul(temp_scalar, b2, dst->c2);
  decaf_448_scalar_sub(dst->d2, pair_r2->priv, temp_scalar);

  /* c3 */
  buff[0] = 0x04;
  if (serialize_ec_point(buff + 1, pair_r3->pub))
    return OTR4_ERROR;

  if (hashToScalar(buff, ED448_POINT_BYTES + 1, dst->c3))
    return OTR4_ERROR;

  /* d3 = (r3 - b3 * c3 mod q). */
  decaf_448_scalar_mul(temp_scalar, smp->b3, dst->c3);
  decaf_448_scalar_sub(dst->d3, pair_r3->priv, temp_scalar);

  /* Compute G2 = (G2a * b2). */
  decaf_448_point_scalarmul(smp->G2, msg_1->G2a, b2);

  /* Compute G3 = (G3a * b3). */
  decaf_448_point_scalarmul(smp->G3, msg_1->G3a, smp->b3);
  ec_point_copy(smp->G3a, msg_1->G3a);

  /* Compute Pb = (G3 * r4). */
  decaf_448_point_scalarmul(dst->Pb, smp->G3, pair_r4->priv);
  ec_point_copy(smp->Pb, dst->Pb);

  /* Compute Qb = (G * r4 + G2 * hashToScalar(y)). */
  ec_scalar_t secret_as_scalar;
  if (hashToScalar(smp->secret, HASH_BYTES, secret_as_scalar))
    return OTR4_ERROR;

  decaf_448_point_scalarmul(dst->Qb, smp->G2, secret_as_scalar);
  decaf_448_point_add(dst->Qb, pair_r4->pub, dst->Qb);
  ec_point_copy(smp->Qb, dst->Qb);

  /* cp = HashToScalar(5 || G3 * r5 || G * r5 + G2 * r6) */
  unsigned char buff_cp[ED448_POINT_BYTES * 2 + 1];
  buff_cp[0] = 0x05;
  decaf_448_point_scalarmul(temp_point, smp->G3, pair_r5->priv);

  // TODO: test for better error generation here
  if (serialize_ec_point(buff_cp + 1, temp_point))
    return OTR4_ERROR;

  decaf_448_point_scalarmul(temp_point, smp->G2, r6);
  decaf_448_point_add(temp_point, pair_r5->pub, temp_point);

  // TODO: test for better error generation here
  if (serialize_ec_point(buff_cp + 1 + ED448_POINT_BYTES, temp_point))
    return OTR4_ERROR;

  if (hashToScalar(buff_cp, ED448_POINT_BYTES * 2 + 1, dst->cp))
    return OTR4_ERROR;

  /* d5 = (r5 - r4 * cp mod q). */
  decaf_448_scalar_mul(dst->d5, pair_r4->priv, dst->cp);
  decaf_448_scalar_sub(dst->d5, pair_r5->priv, dst->d5);

  /* d6 = (r6 - y * cp mod q). */
  decaf_448_scalar_mul(dst->d6, secret_as_scalar, dst->cp);
  decaf_448_scalar_sub(dst->d6, r6, dst->d6);

  return OTR4_SUCCESS;
}

bool smp_msg_2_aprint(uint8_t **dst, size_t *len, const smp_msg_2_t *msg) {
  uint8_t *cursor;
  size_t s = 0;
  s += 4 * ED448_POINT_BYTES;
  s += 7 * ED448_SCALAR_BYTES;

  *dst = malloc(s);
  if (!*dst)
    return false;

  *len = s;
  cursor = *dst;

  if (serialize_ec_point(cursor, msg->G2b))
    return OTR4_ERROR;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->c2);
  cursor += serialize_ec_scalar(cursor, msg->d2);

  if (serialize_ec_point(cursor, msg->G3b))
    return false;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->c3);
  cursor += serialize_ec_scalar(cursor, msg->d3);

  if (serialize_ec_point(cursor, msg->Pb))
    return false;

  cursor += ED448_POINT_BYTES;
  if (serialize_ec_point(cursor, msg->Qb))
    return false;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->cp);
  cursor += serialize_ec_scalar(cursor, msg->d5);
  cursor += serialize_ec_scalar(cursor, msg->d6);

  return true;
}

bool smp_msg_1_deserialize(smp_msg_1_t *msg, const tlv_t *tlv) {
  const uint8_t *cursor = tlv->data;
  uint16_t len = tlv->len;
  size_t read = 0;

  msg->question = NULL;
  if (deserialize_data((uint8_t **)&msg->question, cursor, len, &read))
    return false;

  cursor += read;
  len -= read;

  if (deserialize_ec_point(msg->G2a, cursor))
    return false;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(msg->c2, cursor, len))
    return false;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d2, cursor, len))
    return false;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_point(msg->G3a, cursor))
    return false;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(msg->c3, cursor, len))
    return false;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d3, cursor, len))
    return false;

  return true;
}

bool smp_msg_1_validate(smp_msg_1_t *msg) {
  return ec_point_valid(msg->G2a) || !ec_point_valid(msg->G3a);
}

int smp_msg_2_deserialize(smp_msg_2_t *msg, const tlv_t *tlv) {
  const uint8_t *cursor = tlv->data;
  uint16_t len = tlv->len;

  if (deserialize_ec_point(msg->G2b, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(msg->c2, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d2, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_point(msg->G3b, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(msg->c3, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d3, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_point(msg->Pb, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_point(msg->Qb, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(msg->cp, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d5, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(msg->d6, cursor, len))
    return 1;
  len -= ED448_SCALAR_BYTES;

  return len;
}

bool smp_msg_2_valid_points(smp_msg_2_t *msg) {
  return ec_point_valid(msg->G2b) && ec_point_valid(msg->G3b) &&
         ec_point_valid(msg->Pb) && ec_point_valid(msg->Qb);
}

bool smp_msg_2_valid_zkp(smp_msg_2_t *msg, const smp_context_t smp) {
  uint8_t hash[ED448_POINT_BYTES + 1];
  ec_scalar_t temp_scalar;
  ec_point_t Gb_c, G_d, point_cp;
  bool ok;

  /* Check that c2 = HashToScalar(3 || G * d2 + G2b * c2). */
  decaf_448_point_scalarmul(Gb_c, msg->G2b, msg->c2);
  decaf_448_point_scalarmul(G_d, decaf_448_point_base, msg->d2);
  decaf_448_point_add(G_d, G_d, Gb_c);

  hash[0] = 0x03;
  if (serialize_ec_point(hash + 1, G_d))
    return false;

  if (hashToScalar(hash, ED448_POINT_BYTES + 1, temp_scalar))
    return false;

  ok = ec_scalar_eq(temp_scalar, msg->c2);

  /* Check that c3 = HashToScalar(4 || G * d3 + G3b * c3). */
  decaf_448_point_scalarmul(Gb_c, msg->G3b, msg->c3);
  decaf_448_point_scalarmul(G_d, decaf_448_point_base, msg->d3);
  decaf_448_point_add(G_d, G_d, Gb_c);

  hash[0] = 0x04;
  if (serialize_ec_point(hash + 1, G_d))
    return false;

  if (hashToScalar(hash, ED448_POINT_BYTES + 1, temp_scalar))
    return false;

  ok &= ec_scalar_eq(temp_scalar, msg->c3);

  /* Check that cp = HashToScalar(5 || G3 * d5 + Pb * cp || G * d5 + G2 * d6 +
   Qb * cp) */
  uint8_t buff[2 * ED448_POINT_BYTES + 1];
  decaf_448_point_scalarmul(point_cp, msg->Pb, msg->cp);
  decaf_448_point_scalarmul(G_d, smp->G3, msg->d5);
  decaf_448_point_add(G_d, G_d, point_cp);

  buff[0] = 0x05;
  if (serialize_ec_point(buff + 1, G_d))
    return false;

  decaf_448_point_scalarmul(point_cp, msg->Qb, msg->cp);
  decaf_448_point_scalarmul(G_d, smp->G2, msg->d6);
  decaf_448_point_add(G_d, G_d, point_cp);

  decaf_448_point_scalarmul(point_cp, decaf_448_point_base, msg->d5);
  decaf_448_point_add(G_d, G_d, point_cp);

  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, G_d))
    return false;

  if (hashToScalar(buff, sizeof(buff), temp_scalar))
    return false;

  return ok & ec_scalar_eq(temp_scalar, msg->cp);
}

otr4_err_t generate_smp_msg_3(smp_msg_3_t *dst, const smp_msg_2_t *msg_2,
                              smp_context_t smp) {
  snizkpk_keypair_t pair_r4[1], pair_r5[1], pair_r7[1];
  ec_scalar_t r6, secret_as_scalar;
  ec_point_t temp_point;
  uint8_t buff[1 + 2 * ED448_POINT_BYTES];

  ed448_random_scalar(r6);

  snizkpk_keypair_generate(pair_r4);
  snizkpk_keypair_generate(pair_r5);
  snizkpk_keypair_generate(pair_r7);

  /* Pa = (G3 * r4) */
  decaf_448_point_scalarmul(dst->Pa, smp->G3, pair_r4->priv);
  decaf_448_point_sub(smp->Pa_Pb, dst->Pa, msg_2->Pb);

  if (hashToScalar(smp->secret, HASH_BYTES, secret_as_scalar))
    return OTR4_ERROR;

  /* Qa = (G * r4 + G2 * HashToScalar(x)) */
  decaf_448_point_scalarmul(dst->Qa, smp->G2, secret_as_scalar);
  decaf_448_point_add(dst->Qa, pair_r4->pub, dst->Qa);

  /* cp = HashToScalar(6 || G3 * r5 || G * r5 + G2 * r6) */
  decaf_448_point_scalarmul(temp_point, smp->G3, pair_r5->priv);

  buff[0] = 0x06;
  if (serialize_ec_point(buff + 1, temp_point))
    return OTR4_ERROR;

  decaf_448_point_scalarmul(temp_point, smp->G2, r6);
  decaf_448_point_add(temp_point, pair_r5->pub, temp_point);

  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, temp_point))
    return OTR4_ERROR;

  if (hashToScalar(buff, sizeof(buff), dst->cp))
    return OTR4_ERROR;

  /* d5 = (r5 - r4 * cp mod q). */
  decaf_448_scalar_mul(dst->d5, pair_r4->priv, dst->cp);
  decaf_448_scalar_sub(dst->d5, pair_r5->priv, dst->d5);

  /* d6 = (r6 - HashToScalar(x) * cp mod q.) */
  decaf_448_scalar_mul(dst->d6, secret_as_scalar, dst->cp);
  decaf_448_scalar_sub(dst->d6, r6, dst->d6);

  /* Ra = ((Qa - Qb) * a3) */
  decaf_448_point_sub(smp->Qa_Qb, dst->Qa, msg_2->Qb);
  decaf_448_point_scalarmul(dst->Ra, smp->Qa_Qb, smp->a3);

  /* cr = HashToScalar(7 || G * r7 || (Qa - Qb) * r7) */
  buff[0] = 0x07;
  if (serialize_ec_point(buff + 1, pair_r7->pub))
    return OTR4_ERROR;

  decaf_448_point_scalarmul(temp_point, smp->Qa_Qb, pair_r7->priv);
  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, temp_point))
    return OTR4_ERROR;

  if (hashToScalar(buff, sizeof(buff), dst->cr))
    return OTR4_ERROR;

  /* d7 = (r7 - a3 * cr mod q). */
  decaf_448_scalar_mul(dst->d7, smp->a3, dst->cr);
  decaf_448_scalar_sub(dst->d7, pair_r7->priv, dst->d7);

  return OTR4_SUCCESS;
}

bool smp_msg_3_aprint(uint8_t **dst, size_t *len, const smp_msg_3_t *msg) {
  uint8_t *cursor;
  size_t s = 0;

  s += 3 * ED448_POINT_BYTES;
  s += 5 * ED448_SCALAR_BYTES;

  *dst = malloc(s);
  if (!*dst)
    return false;

  *len = s;
  cursor = *dst;

  if (serialize_ec_point(cursor, msg->Pa))
    return false;

  cursor += ED448_POINT_BYTES;

  if (serialize_ec_point(cursor, msg->Qa))
    return false;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->cp);
  cursor += serialize_ec_scalar(cursor, msg->d5);
  cursor += serialize_ec_scalar(cursor, msg->d6);

  if (serialize_ec_point(cursor, msg->Ra))
    return false;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->cr);
  cursor += serialize_ec_scalar(cursor, msg->d7);

  return true;
}

int smp_msg_3_deserialize(smp_msg_3_t *dst, const tlv_t *tlv) {
  const uint8_t *cursor = tlv->data;
  uint16_t len = tlv->len;

  if (deserialize_ec_point(dst->Pa, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_point(dst->Qa, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(dst->cp, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(dst->d5, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(dst->d6, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_point(dst->Ra, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(dst->cr, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(dst->d7, cursor, len))
    return 1;

  len -= ED448_SCALAR_BYTES;

  return len;
}

bool smp_msg_3_validate_points(smp_msg_3_t *msg) {
  return ec_point_valid(msg->Pa) && ec_point_valid(msg->Qa) &&
         ec_point_valid(msg->Ra);
}

bool smp_msg_3_validate_zkp(smp_msg_3_t *msg, const smp_context_t smp) {
  uint8_t buff[1 + 2 * ED448_POINT_BYTES];
  ec_point_t temp_point, temp_point_2;
  ec_scalar_t temp_scalar;
  bool ok;

  /* cp = HashToScalar(6 || G3 * d5 + Pa * cp || G * d5 + G2 * d6 + Qa * cp) */
  buff[0] = 0x06;
  decaf_448_point_scalarmul(temp_point, msg->Pa, msg->cp);
  decaf_448_point_scalarmul(temp_point_2, smp->G3, msg->d5);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);
  if (serialize_ec_point(buff + 1, temp_point))
    return false;

  decaf_448_point_scalarmul(temp_point, msg->Qa, msg->cp);
  decaf_448_point_scalarmul(temp_point_2, smp->G2, msg->d6);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);
  decaf_448_point_scalarmul(temp_point_2, decaf_448_point_base, msg->d5);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);
  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, temp_point))
    return false;

  if (hashToScalar(buff, sizeof(buff), temp_scalar))
    return false;

  ok = ec_scalar_eq(temp_scalar, msg->cp);

  /* cr = HashToScalar(7 || G * d7 + G3a * cr || (Qa - Qb) * d7 + Ra * cr) */
  decaf_448_point_scalarmul(temp_point, smp->G3a, msg->cr);
  decaf_448_point_scalarmul(temp_point_2, decaf_448_point_base, msg->d7);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);

  buff[0] = 0x07;
  if (serialize_ec_point(buff + 1, temp_point))
    return false;

  decaf_448_point_scalarmul(temp_point, msg->Ra, msg->cr);
  decaf_448_point_sub(temp_point_2, msg->Qa, smp->Qb);
  decaf_448_point_scalarmul(temp_point_2, temp_point_2, msg->d7);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);

  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, temp_point))
    return false;

  if (hashToScalar(buff, sizeof(buff), temp_scalar))
    return false;

  return ok & ec_scalar_eq(temp_scalar, msg->cr);
}

bool generate_smp_msg_4(smp_msg_4_t *dst, const smp_msg_3_t *msg_3,
                        smp_context_t smp) {
  uint8_t buff[1 + 2 * ED448_POINT_BYTES];
  ec_point_t Qa_Qb;
  snizkpk_keypair_t pair_r7[1];
  snizkpk_keypair_generate(pair_r7);

  /* Rb = ((Qa - Qb) * b3) */
  decaf_448_point_sub(Qa_Qb, msg_3->Qa, smp->Qb);
  decaf_448_point_scalarmul(dst->Rb, Qa_Qb, smp->b3);

  /* cr = HashToScalar(8 || G * r7 || (Qa - Qb) * r7) */
  buff[0] = 0x08;
  if (serialize_ec_point(buff + 1, pair_r7->pub))
    return false;

  decaf_448_point_scalarmul(Qa_Qb, Qa_Qb, pair_r7->priv);
  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, Qa_Qb))
    return false;

  if (hashToScalar(buff, sizeof(buff), dst->cr))
    return false;

  /* d7 = (r7 - b3 * cr mod q). */
  decaf_448_scalar_mul(dst->d7, smp->b3, dst->cr);
  decaf_448_scalar_sub(dst->d7, pair_r7->priv, dst->d7);

  return true;
}

bool smp_msg_4_aprint(uint8_t **dst, size_t *len, smp_msg_4_t *msg) {
  size_t s = 0;

  s += ED448_POINT_BYTES;
  s += 2 * ED448_SCALAR_BYTES;

  *dst = malloc(s);
  if (!*dst)
    return false;

  uint8_t *cursor = *dst;

  if (serialize_ec_point(cursor, msg->Rb))
    return false;

  cursor += ED448_POINT_BYTES;
  cursor += serialize_ec_scalar(cursor, msg->cr);
  cursor += serialize_ec_scalar(cursor, msg->d7);

  *len = s;

  return true;
}

int smp_msg_4_deserialize(smp_msg_4_t *dst, const tlv_t *tlv) {
  uint8_t *cursor = tlv->data;
  size_t len = tlv->len;

  if (deserialize_ec_point(dst->Rb, cursor))
    return 1;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  if (deserialize_ec_scalar(dst->cr, cursor, len))
    return 1;

  cursor += ED448_SCALAR_BYTES;
  len -= ED448_SCALAR_BYTES;

  if (deserialize_ec_scalar(dst->d7, cursor, len))
    return 1;

  len -= ED448_SCALAR_BYTES;

  return len;
}

bool smp_msg_4_validate_zkp(smp_msg_4_t *msg, const smp_context_t smp) {
  uint8_t buff[1 + 2 * ED448_POINT_BYTES];
  ec_point_t temp_point, temp_point_2;
  ec_scalar_t temp_scalar;

  /* cr = HashToScalar(8 || G * d7 + G3 * cr || (Qa - Qb) * d7 + Rb * cr). */
  decaf_448_point_scalarmul(temp_point, smp->G3, msg->cr);
  decaf_448_point_scalarmul(temp_point_2, decaf_448_point_base, msg->d7);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);

  buff[0] = 0x08;
  if (serialize_ec_point(buff + 1, temp_point))
    return false;

  decaf_448_point_scalarmul(temp_point, msg->Rb, msg->cr);
  decaf_448_point_scalarmul(temp_point_2, smp->Qa_Qb, msg->d7);
  decaf_448_point_add(temp_point, temp_point, temp_point_2);
  if (serialize_ec_point(buff + 1 + ED448_POINT_BYTES, temp_point))
    return false;

  if (hashToScalar(buff, sizeof(buff), temp_scalar))
    return false;

  return ec_scalar_eq(msg->cr, temp_scalar) == 0;
}

static void smp_msg_1_copy(smp_msg_1_t *dst, const smp_msg_1_t *src) {
  // TODO: Maybe we dont need to copy everything
  dst->q_len = src->q_len;
  if (src->question)
    dst->question = otrv4_strdup(src->question);
  else
    dst->question = NULL;

  ec_point_copy(dst->G2a, src->G2a);
  ec_scalar_copy(dst->c2, src->c2);
  ec_scalar_copy(dst->d2, src->d2);
  ec_point_copy(dst->G3a, src->G3a);
  ec_scalar_copy(dst->c3, src->c3);
  ec_scalar_copy(dst->d3, src->d3);
}

static otr4_smp_event_t receive_smp_msg_1(const tlv_t *tlv, smp_context_t smp) {
  smp_msg_1_t msg_1[1];

  if (SMPSTATE_EXPECT1 != smp->state)
    return OTRV4_SMPEVENT_ABORT;

  do {
    if (!smp_msg_1_deserialize(msg_1, tlv))
      continue;

    if (!smp_msg_1_validate(msg_1))
      continue;

    smp->msg1 = malloc(sizeof(smp_msg_1_t));
    if (!smp->msg1)
      continue;

    smp_msg_1_copy(smp->msg1, msg_1);
    smp_msg_1_destroy(msg_1);
    return OTRV4_SMPEVENT_NONE;
  } while (0);

  smp_msg_1_destroy(msg_1);
  return OTRV4_SMPEVENT_ERROR;
}

void smp_msg_2_destroy(smp_msg_2_t *msg) {
  ec_point_destroy(msg->G2b);
  ec_point_destroy(msg->G3b);
  ec_point_destroy(msg->Pb);
  ec_point_destroy(msg->Qb);
  ec_scalar_destroy(msg->c3);
  ec_scalar_destroy(msg->d3);
  ec_scalar_destroy(msg->c2);
  ec_scalar_destroy(msg->d2);
  ec_scalar_destroy(msg->cp);
  ec_scalar_destroy(msg->d5);
  ec_scalar_destroy(msg->d6);
}

// TODO:
static otr4_smp_event_t reply_with_smp_msg_2(tlv_t **to_send,
                                             smp_context_t smp) {
  smp_msg_2_t msg_2[1];
  uint8_t *buff;
  size_t bufflen;

  *to_send = NULL;

  // TODO: what to do is something wrong happen?
  generate_smp_msg_2(msg_2, smp->msg1, smp);
  if (!smp_msg_2_aprint(&buff, &bufflen, msg_2))
    return OTRV4_SMPEVENT_ERROR;

  smp_msg_2_destroy(msg_2);

  *to_send = otrv4_tlv_new(OTRV4_TLV_SMP_MSG_2, bufflen, buff);
  free(buff);

  if (!to_send)
    return OTRV4_SMPEVENT_ERROR;

  smp->state = SMPSTATE_EXPECT3;
  smp->progress = 50;
  return OTRV4_SMPEVENT_NONE;
}

static otr4_smp_event_t receive_smp_msg_2(smp_msg_2_t *msg_2, const tlv_t *tlv,
                                          smp_context_t smp) {
  if (SMPSTATE_EXPECT2 != smp->state)
    return OTRV4_SMPEVENT_ERROR;

  if (smp_msg_2_deserialize(msg_2, tlv) != 0)
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_2_valid_points(msg_2))
    return OTRV4_SMPEVENT_ERROR;

  decaf_448_point_scalarmul(smp->G2, msg_2->G2b, smp->a2);
  decaf_448_point_scalarmul(smp->G3, msg_2->G3b, smp->a3);

  if (!smp_msg_2_valid_zkp(msg_2, smp))
    return OTRV4_SMPEVENT_ERROR;

  return OTRV4_SMPEVENT_NONE;
}

void smp_msg_3_destroy(smp_msg_3_t *msg) {
  ec_point_destroy(msg->Pa);
  ec_point_destroy(msg->Qa);
  ec_point_destroy(msg->Ra);
  ec_scalar_destroy(msg->cp);
  ec_scalar_destroy(msg->d5);
  ec_scalar_destroy(msg->d6);
  ec_scalar_destroy(msg->cr);
  ec_scalar_destroy(msg->d7);
}

void smp_msg_4_destroy(smp_msg_4_t *msg) {
  ec_scalar_destroy(msg->cr);
  ec_scalar_destroy(msg->d7);

  ec_point_destroy(msg->Rb);
}

static otr4_smp_event_t reply_with_smp_msg_3(tlv_t **to_send,
                                             const smp_msg_2_t *msg_2,
                                             smp_context_t smp) {
  smp_msg_3_t msg_3[1];
  uint8_t *buff = NULL;
  size_t bufflen = 0;

  if (generate_smp_msg_3(msg_3, msg_2, smp))
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_3_aprint(&buff, &bufflen, msg_3))
    return OTRV4_SMPEVENT_ERROR;

  smp_msg_3_destroy(msg_3);

  *to_send = otrv4_tlv_new(OTRV4_TLV_SMP_MSG_3, bufflen, buff);
  free(buff);

  if (!to_send)
    return OTRV4_SMPEVENT_ERROR;

  smp->state = SMPSTATE_EXPECT4;
  smp->progress = 50;
  return OTRV4_SMPEVENT_NONE;
}

static otr4_smp_event_t receive_smp_msg_3(smp_msg_3_t *msg_3, const tlv_t *tlv,
                                          smp_context_t smp) {
  if (SMPSTATE_EXPECT3 != smp->state)
    return OTRV4_SMPEVENT_ERROR;

  if (smp_msg_3_deserialize(msg_3, tlv) != 0)
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_3_validate_points(msg_3))
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_3_validate_zkp(msg_3, smp))
    return OTRV4_SMPEVENT_ERROR;

  smp->progress = 75;
  return OTRV4_SMPEVENT_NONE;
}

static bool smp_is_valid_for_msg_3(const smp_msg_3_t *msg, smp_context_t smp) {
  ec_point_t Rab, Pa_Pb;
  /* Compute Rab = (Ra * b3) */
  decaf_448_point_scalarmul(Rab, msg->Ra, smp->b3);
  /* Pa - Pb == Rab */
  decaf_448_point_sub(Pa_Pb, msg->Pa, smp->Pb);
  return DECAF_TRUE == decaf_448_point_eq(Pa_Pb, Rab);
}

static otr4_smp_event_t reply_with_smp_msg_4(tlv_t **to_send,
                                             const smp_msg_3_t *msg_3,
                                             smp_context_t smp) {
  smp_msg_4_t msg_4[1];
  uint8_t *buff = NULL;
  size_t bufflen = 0;

  if (!generate_smp_msg_4(msg_4, msg_3, smp))
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_4_aprint(&buff, &bufflen, msg_4))
    return OTRV4_SMPEVENT_ERROR;

  *to_send = otrv4_tlv_new(OTRV4_TLV_SMP_MSG_4, bufflen, buff);
  free(buff);

  /* Validates SMP */
  smp->progress = 100;
  smp->state = SMPSTATE_EXPECT1;
  if (!smp_is_valid_for_msg_3(msg_3, smp))
    return OTRV4_SMPEVENT_FAILURE;

  return OTRV4_SMPEVENT_SUCCESS;
}

static bool smp_is_valid_for_msg_4(smp_msg_4_t *msg, smp_context_t smp) {
  ec_point_t Rab;
  /* Compute Rab = Rb * a3. */
  decaf_448_point_scalarmul(Rab, msg->Rb, smp->a3);
  /* Pa - Pb == Rab */
  return DECAF_TRUE == decaf_448_point_eq(smp->Pa_Pb, Rab);
}

static otr4_smp_event_t receive_smp_msg_4(smp_msg_4_t *msg_4, const tlv_t *tlv,
                                          smp_context_t smp) {
  if (SMPSTATE_EXPECT4 != smp->state)
    return OTRV4_SMPEVENT_ERROR;

  if (smp_msg_4_deserialize(msg_4, tlv) != 0)
    return OTRV4_SMPEVENT_ERROR;

  if (!ec_point_valid(msg_4->Rb))
    return OTRV4_SMPEVENT_ERROR;

  if (!smp_msg_4_validate_zkp(msg_4, smp))
    return OTRV4_SMPEVENT_ERROR;

  smp->progress = 100;
  smp->state = SMPSTATE_EXPECT1;
  if (!smp_is_valid_for_msg_4(msg_4, smp))
    return OTRV4_SMPEVENT_FAILURE;

  return OTRV4_SMPEVENT_SUCCESS;
}

static otr4_smp_event_t process_smp_msg1(const tlv_t *tlv, smp_context_t smp) {
  otr4_smp_event_t event = receive_smp_msg_1(tlv, smp);

  if (!event) {
    smp->progress = 25;
    event = OTRV4_SMPEVENT_ASK_FOR_ANSWER;
  }

  return event;
}

static otr4_smp_event_t process_smp_msg2(tlv_t **smp_reply, const tlv_t *tlv,
                                         smp_context_t smp) {
  smp_msg_2_t msg_2[1];
  otr4_smp_event_t event = receive_smp_msg_2(msg_2, tlv, smp);

  if (!event)
    event = reply_with_smp_msg_3(smp_reply, msg_2, smp);

  smp_msg_2_destroy(msg_2);
  return event;
}

static otr4_smp_event_t process_smp_msg3(tlv_t **smp_reply, const tlv_t *tlv,
                                         smp_context_t smp) {
  smp_msg_3_t msg_3[1];
  otr4_smp_event_t event = receive_smp_msg_3(msg_3, tlv, smp);

  if (!event)
    event = reply_with_smp_msg_4(smp_reply, msg_3, smp);

  smp_msg_3_destroy(msg_3);
  return event;
}

static otr4_smp_event_t process_smp_msg4(const tlv_t *tlv, smp_context_t smp) {
  smp_msg_4_t msg_4[1];

  otr4_smp_event_t event = receive_smp_msg_4(msg_4, tlv, smp);

  smp_msg_4_destroy(msg_4);

  return event;
}
