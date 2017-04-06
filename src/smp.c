#include "smp.h"
#include "tlv.h"

tlv_t * generate_smp_msg_1(void)
{
	uint8_t *data = NULL;
	size_t len = 0;

	return otrv4_tlv_new(OTRV4_TLV_SMP_MSG_2, len, data);
}

tlv_t * generate_smp_msg_2(void)
{
	uint8_t *data = NULL;
	size_t len = 0;

	return otrv4_tlv_new(OTRV4_TLV_SMP_MSG_2, len, data);
}
