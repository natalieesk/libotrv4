#ifndef SMP_H
#define SMP_H

#include "fingerprint.h"
#include "str.h"
#include "tlv.h"

#define SMP_VERSION 0x01
#define SMP_MIN_SECRET_BYTES (1 + 64 * 2 + 8)

typedef enum {
  SMPSTATE_EXPECT1,
  SMPSTATE_EXPECT2,
  SMPSTATE_EXPECT3,
  SMPSTATE_EXPECT4
} smp_state_t;

typedef struct {
  uint32_t q_len;
  char *question;
  ec_point_t G2a;
  ec_scalar_t c2;
  ec_scalar_t d2;
  ec_point_t G3a;
  ec_scalar_t c3;
  ec_scalar_t d3;
} smp_msg_1_t;

typedef struct {
  smp_state_t state;
  unsigned char *secret;
  ec_scalar_t a2, a3, b3;
  ec_point_t G2, G3;
  ec_point_t G3a, G3b;
  ec_point_t Pb, Qb;
  ec_point_t Pa_Pb, Qa_Qb;

  uint8_t progress;
  smp_msg_1_t *msg1;
} smp_context_t[1];

typedef struct {
  ec_point_t G2b;
  ec_scalar_t c2;
  ec_scalar_t d2;
  ec_point_t G3b;
  ec_scalar_t c3;
  ec_scalar_t d3;
  ec_point_t Pb;
  ec_point_t Qb;
  ec_scalar_t cp;
  ec_scalar_t d5;
  ec_scalar_t d6;
} smp_msg_2_t;

typedef struct {
  ec_point_t Pa, Qa;
  ec_scalar_t cp, d5, d6;
  ec_point_t Ra;
  ec_scalar_t cr, d7;
} smp_msg_3_t;

typedef struct {
  ec_point_t Rb;
  ec_scalar_t cr, d7;
} smp_msg_4_t;

void smp_context_init(smp_context_t smp);
void smp_destroy(smp_context_t smp);

void generate_smp_secret(unsigned char **secret, otrv4_fingerprint_t our_fp,
                         otrv4_fingerprint_t their_fp, uint8_t *ssid,
                         const uint8_t *answer, size_t answerlen);

void smp_msg_1_destroy(smp_msg_1_t *msg);
otr4_err_t generate_smp_msg_1(smp_msg_1_t *dst, smp_context_t smp);

otr4_err_t smp_msg_1_asprintf(uint8_t **dst, size_t *len,
                              const smp_msg_1_t *msg);

void smp_msg_2_destroy(smp_msg_2_t *msg);

otr4_err_t generate_smp_msg_2(smp_msg_2_t *dst, const smp_msg_1_t *msg_1,
                              smp_context_t smp);

bool smp_msg_2_valid_points(smp_msg_2_t *msg);

bool smp_msg_2_valid_zkp(smp_msg_2_t *msg, const smp_context_t smp);

bool smp_msg_2_validate_zkp(smp_msg_2_t *msg, const smp_context_t smp);

void smp_msg_3_destroy(smp_msg_3_t *msg);

otr4_err_t generate_smp_msg_3(smp_msg_3_t *dst, const smp_msg_2_t *msg_2,
                              smp_context_t smp);

bool generate_smp_msg_4(smp_msg_4_t *dst, const smp_msg_3_t *msg_3,
                        smp_context_t smp);

// TODO: export only what is needed
bool smp_msg_1_deserialize(smp_msg_1_t *dst, const tlv_t *tlv);
int smp_msg_2_deserialize(smp_msg_2_t *dst, const tlv_t *tlv);
bool smp_msg_2_aprint(uint8_t **dst, size_t *len, const smp_msg_2_t *msg);
bool smp_msg_3_aprint(uint8_t **dst, size_t *len, const smp_msg_3_t *msg);
int smp_msg_3_deserialize(smp_msg_3_t *dst, const tlv_t *tlv);
bool smp_msg_3_validate_zkp(smp_msg_3_t *msg, const smp_context_t smp);
bool smp_msg_4_aprint(uint8_t **dst, size_t *len, smp_msg_4_t *msg);
int smp_msg_4_deserialize(smp_msg_4_t *dst, const tlv_t *tlv);
bool smp_msg_4_validate_zkp(smp_msg_4_t *msg, const smp_context_t smp);

#endif
