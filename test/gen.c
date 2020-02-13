#include <stdio.h>
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

static const char *res1 = "0031000B916407281553F800000B021B14";
static const char *res2[] = {
	"0071000B919408103254F600000BA006080400010201C8329BFD7681A8E8F41C949E83C2207939CC66E741ECB7FB0C9A36A72E50920E1ABFDDF470DA3D07B5DFF232888E0EBB41311B0C344687E5E131BD2C9F83C26E32888EAECF41E3B0DBFDA683C46550D93D7E93CB64507D9E769F4161D03CED3EB3CBA06973EA0225E9A0767D4E0789CBA0399C9DA683EA7050DA4D7F83DA75363D0D671740",
	"0071000B919408103254F600000B6F06080400010202D3E614444787E9A0B0BC0C1ABFDDE330BDEC0ED3CB64D01D5D7683E8E8721E14969741F2F2B89CB697C92E90B36C2FCBE9E832BB3C9FB34074747A0E9A36A7A0F1DB4D0FA7DD73D0DBCDCE838ED3E60D344687E5E131BD2C9F8700"
};
static const char *res3 = "0031000B916407281553F800080B1A00680065006C006C006F00200077006F0072006C0064D83DDE0B";
static const char *res4[] = {
	"0071000B916407281553F800080B8B06080400010201003100320033003400350036003700380039003000310032003300340035003600370038003900300031003200330034003500360037003800390030003100320033003400350036003700380039003000310032003300340035003600370038003900300031003200330034003500360037003800390030003100320033003400350036",
	"0071000B916407281553F800080B1D06080400010202003700380039003000680065006C006C006FD83DDE0B"
};

int idx;

int pdu_send_cb1(const char *buf, unsigned len, void *s)
{
	if (strcmp(buf, res1)) {
		fprintf(stderr, "Check 1 unsuccessful; Expected %s, got %s\n", res1, buf);
		++faults;
	} else {
		++ok;
	}
	return 0;
}
int pdu_send_cb2(const char *buf, unsigned len, void *s)
{
	if (strcmp(buf, res2[idx])) {
		fprintf(stderr, "Check 2 unsuccessful; Expected %s, got %s\n", res2[idx], buf);
		++faults;
	} else {
		++ok;
	}
	++idx;
	return 0;
}
int pdu_send_cb3(const char *buf, unsigned len, void *s)
{
	if (strcmp(buf, res3)) {
		fprintf(stderr, "Check 3 unsuccessful; Expected %s, got %s\n", res3, buf);
		++faults;
	} else {
		++ok;
	}
	return 0;
}
int pdu_send_cb4(const char *buf, unsigned len, void *s)
{
	if (strcmp(buf, res4[idx])) {
		fprintf(stderr, "Check 4 unsuccessful; Expected %s, got %s\n", res4[idx], buf);
		++faults;
	} else {
		++ok;
	}
	++idx;
	return 0;
}
void test_pdu_build()
{
// 		printf("%s\n", LUT_GSM7_LS[0][0]);
	pdu_build_mult(pdu_send_cb1, "", "+46708251358", "{", 60, 1, 1, NULL);
	idx = 0;
	pdu_build_mult(pdu_send_cb2, "", "+49800123456", "Hello. This is a really long SMS. It contains more than 160 characters and thus cannot be encoded using a single SMS. It must be split up into multiplé SMS that are concatenated when they are received. Nevertheless, this SMS contains only GSM7 characters!", 60, 1, 1, NULL);
	pdu_build_mult(pdu_send_cb3, "", "+46708251358", "hello world😋", 60, 1, 1, NULL);
	idx = 0;
	pdu_build_mult(pdu_send_cb4, "", "+46708251358", "1234567890123456789012345678901234567890123456789012345678901234567890hello😋", 60, 1, 1, NULL);
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
