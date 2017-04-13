#include "../otrv4.h"
#include "../smp.h"
#include "../tlv.h"

void test_smp_state_machine(void)
{
	OTR4_INIT;

        otrv4_keypair_t alice_keypair[1], bob_keypair[1];
        otrv4_keypair_generate(alice_keypair);
        otrv4_keypair_generate(bob_keypair);
	otrv4_policy_t policy = {.allows = OTRV4_ALLOW_V4};

	otrv4_t *alice_otr = otrv4_new(alice_keypair, policy);
	otrv4_t *bob_otr = otrv4_new(bob_keypair, policy);

	g_assert_cmpint(alice_otr->smp->state, ==, SMPSTATE_EXPECT1);

	tlv_t *smp_msg_1 = otrv4_smp_initiate(alice_otr, "");
	otrv4_assert(!smp_msg_1);

	//Should be in ecrypted state before perform SMP
	do_ake_fixture(alice_otr, bob_otr);

	smp_msg_1 = otrv4_smp_initiate(alice_otr, "answer");
	otrv4_assert(smp_msg_1);
	g_assert_cmpint(alice_otr->smp->state, ==, SMPSTATE_EXPECT2);

	tlv_t *smp_msg_2 = otrv4_process_smp(bob_otr, smp_msg_1);
	otrv4_assert(smp_msg_2);
	g_assert_cmpint(smp_msg_2->type, ==, OTRV4_TLV_SMP_MSG_2);
	g_assert_cmpint(bob_otr->smp->state, ==, SMPSTATE_EXPECT3);

	otrv4_destroy(alice_otr);
	otrv4_destroy(bob_otr);
	otrv4_keypair_destroy(alice_keypair);
	otrv4_keypair_destroy(bob_keypair);

	OTR4_FREE;
};

void test_generate_smp_secret(void)
{
	smp_context_t smp;
	otrv4_fingerprint_t our = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,

		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,

		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	};

	otrv4_fingerprint_t their = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,

		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,

		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	uint8_t ssid[8] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	};

	generate_smp_secret(smp, our, their, ssid, "the-answer");
	otrv4_assert(smp->x);

	unsigned char expected_secret[148] = {
		0x57, 0x24, 0xca, 0x11, 0x9f, 0xc2, 0x09, 0x3f,
		0x63, 0xf7, 0x76, 0x1a, 0x18, 0x28, 0xb3, 0x86,
		0x26, 0xce, 0xaa, 0xbd, 0xab, 0x52, 0x16, 0xdb,
		0x0c, 0x14, 0x20, 0xb5, 0x34, 0x38, 0x41, 0x42,
		0x7c, 0x47, 0xa2, 0xcf, 0xc8, 0x98, 0xc5, 0xc1,
		0x53, 0xad, 0xe9, 0xb0, 0x75, 0x15, 0xa4, 0x22,
		0xc9, 0x1f, 0xcc, 0xc1, 0xac, 0xf0, 0x43, 0x33,
		0xea, 0x64, 0x3d, 0x17, 0x6f, 0x27, 0xae, 0xdf,
		0x37, 0xfb, 0x0e, 0xb1, 0x68, 0xd7, 0xa7, 0x45,
		0x95, 0x66, 0xc6, 0x00, 0x1b, 0x03, 0x9e, 0x66,
		0x00, 0xf8, 0x0b, 0x99, 0xca, 0x04, 0xf5, 0x5e,
		0x0c, 0x46, 0x1a, 0x8d, 0x28, 0x7f, 0xc3, 0xda,
		0x62, 0x25, 0x92, 0x80, 0xb9, 0x45, 0x89, 0x1b,
		0x87, 0x45, 0x25, 0xd6, 0x85, 0x57, 0xab, 0x6c,
		0x2f, 0x85, 0xc2, 0x64, 0xed, 0x02, 0xf5, 0xf9,
		0xa8, 0x51, 0x7f, 0x15, 0xf8, 0x26, 0xdb, 0x7f,
		0x41, 0xfa, 0xc6, 0x01, 0x29, 0x7a, 0xf5, 0xab,
		0x79, 0xe9, 0x5c, 0x1f, 0x82, 0xa6, 0x71, 0x8a,
		0xfd, 0xe3, 0x36, 0xf6,
	};

	otrv4_assert_cmpmem(smp->x, expected_secret, 148);
}