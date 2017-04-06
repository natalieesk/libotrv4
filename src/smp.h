#ifndef SMP_H
#define SMP_H

#include "tlv.h"

typedef enum {
    SMPSTATE_EXPECT1,
    SMPSTATE_EXPECT2,
    SMPSTATE_EXPECT3
} smp_state_t;

tlv_t *generate_smp_msg_1(void);
tlv_t *generate_smp_msg_2(void);

#endif
