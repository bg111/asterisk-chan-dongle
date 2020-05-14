#include <stdio.h>
#include <string.h>

#include "pdu.h"
#include "gsm7_luts.h"

int ok = 0;
int faults = 0;

/* We call ast_log from pdu.c, so we'll fake an implementation here. */
void ast_log(int level, const char* fmt, ...)
{
    /* Silence compiler warnings */
    (void)level;
    (void)fmt;
}

typedef struct result
{
	const char *dst;
	const char *in;
	const char *out[16];
} result_t;
void test_pdu_build()
{
	result_t res[] = {
		{
			"+46708251358",
			"{",
			{
				"0031000B916407281553F800000B021B14",
				NULL
			}
		}, {
			"+49800123456",
			"Hello. This is a really long SMS. It contains more than 160 characters and thus cannot be encoded using a single SMS. It must be split up into multiplé SMS that are concatenated when they are received. Nevertheless, this SMS contains only GSM7 characters!",
			{
				"0071000B919408103254F600000B9F050003010201906536FBED0251D1E939283D078541F27298CDCE83D86FF719346D4E5DA0241D347EBBE9E1B47B0E6ABFE565101D1D7683623618688C0ECBC3637A593E0785DD64101D5D9F83C661B7FB4D0789CBA0B27BFC2697C9A0FA3CED3E83C2A079DA7D669741D3E6D4054AD241EDFA9C0E12974173383B4D07D5E1A0B49BFE06B5EB6C7A1ACE2E8000",
				"0071000B919408103254F600000B6E050003010202A6CD29888E0ED341617919347EBBC7617AD91DA697C9A03BBAEC06D1D1E53C282C2F83E4E571396D2F935D2067D95E96D3D16576793E6781E8E8F41C346D4E41E3B79B1E4EBBE7A0B79B9D071DA7CD1B688C0ECBC3637A593E0F01",
				NULL
			}
		}, {
			"+46708251358",
			"hello world😋",
			{
				"0031000B916407281553F800080B1A00680065006C006C006F00200077006F0072006C0064D83DDE0B",
				NULL
			}
		}, {
			"+46708251358",
			"1234567890123456789012345678901234567890123456789012345678901234567890hello😋",
			{
				"0071000B916407281553F800080B8C0500030102010031003200330034003500360037003800390030003100320033003400350036003700380039003000310032003300340035003600370038003900300031003200330034003500360037003800390030003100320033003400350036003700380039003000310032003300340035003600370038003900300031003200330034003500360037",
				"0071000B916407281553F800080B1A05000301020200380039003000680065006C006C006FD83DDE0B",
				NULL
			}
		}
	};

	uint16_t ucs2[256];
	pdu_part_t pdus[255];
	char hexbuf[PDU_LENGTH * 2 + 1];
	for (int i = 0; i < sizeof(res) / sizeof(result_t); ++i) {
		int ret = utf8_to_ucs2(res[i].in, strlen(res[i].in), ucs2, sizeof(ucs2));
		if (ret < 0) {
			fprintf(stderr, "Check %d unsuccessful: UTF-8-to-UCS-2 returns failure code %d\n", i, ret);
		}
		int cnt = pdu_build_mult(pdus, "", res[i].dst, ucs2, ret, 60, 1, 1);
		if (cnt <= 0) {
			fprintf(stderr, "Check %d unsuccessful: PDU-Build returns failure code %d\n", i, cnt);
			++faults;
			continue;
		}
		for (int j = 0; j < cnt; ++j) {
			hexify(pdus[j].buffer, pdus[j].length, hexbuf);
			if (res[i].out[j] && strcmp(res[i].out[j], hexbuf) == 0) {
				++ok;
			} else {
				++faults;
				fprintf(stderr, "Check %d unsuccessful: Expected %s; Got %s\n", i, res[i].out[j], hexbuf);
			}
		}
		if (res[i].out[cnt]) fprintf(stderr, "Check %d unsuccessful: Expected %s; Got %s\n", i, res[i].out[cnt], NULL);
	}
}


#/* */
int main()
{
	test_pdu_build();
	
	fprintf(stderr, "done %d tests: %d OK %d FAILS\n", ok + faults, ok, faults);

	if (faults) {
		return 1;
	}
	return 0;
}
