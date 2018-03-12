#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "at_parse.h"			/* at_parse_*() */
#include "mutils.h"			/* ITEMS_OF() */


int ok = 0;
int faults = 0;

/* We call ast_log from pdu.c, so we'll fake an implementation here. */
void ast_log(int level, const char* fmt, ...)
{
    /* Silence compiler warnings */
    (void)level;
    (void)fmt;
}

#/* */
void test_parse_cnum()
{
	static const struct test_case {
		const char	* input;
		const char	* result;
	} cases[] = {
		{ "+CNUM: \"*Subscriber Number\",\"+79139131234\",145", "+79139131234" },
		{ "+CNUM: \"Subscriber Number\",\"\",145", "" },
		{ "+CNUM: \"Subscriber Number\",,145", "" },
		{ "+CNUM: \"\",\"+79139131234\",145", "+79139131234" },
		{ "+CNUM: ,\"\",145", "" },
		{ "+CNUM: ,,145", "" },
		{ "+CNUM: \"\",+79139131234\",145", "+79139131234" },
		{ "+CNUM: \"\",+79139131234,145", "+79139131234" },
	};
	unsigned idx = 0;
	char * input;
	const char * res;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cnum", input);
		res = at_parse_cnum(input);
		if(strcmp(res, cases[idx].result) == 0) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = \"%s\"\t%s\n", res, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cops()
{
	static const struct test_case {
		const char	* input;
		const char	* result;
	} cases[] = {
		{ "+COPS: 0,0,\"TELE2\",0", "TELE2" },
		{ "+COPS: 0,0,\"TELE2,0", "TELE2" },
		{ "+COPS: 0,0,TELE2,0", "TELE2" },
	};
	unsigned idx = 0;
	char * input;
	const char * res;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cops", input);
		res = at_parse_cops(input);
		if(strcmp(res, cases[idx].result) == 0) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = \"%s\"\t%s\n", res, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_creg()
{
	struct result {
		int	res;
		int	gsm_reg;
		int	gsm_reg_status;
		char 	* lac;
		char	* ci;
	};
	static const struct test_case {
		const char	* input;
		struct result 	result;
	} cases[] = {
		{ "+CREG: 2,1,9110,7E6", { 0, 1, 1, "9110", "7E6"} },
		{ "+CREG: 2,1,XXXX,AAAA", { 0, 1, 1, "XXXX", "AAAA"} },
	};
	unsigned idx = 0;
	char * input;
	struct result result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_creg", input);
		result.res = at_parse_creg(input, strlen(input), &result.gsm_reg, &result.gsm_reg_status, &result.lac, &result.ci);
		if(result.res == cases[idx].result.res
			&& result.gsm_reg == cases[idx].result.gsm_reg
			&& result.gsm_reg_status == cases[idx].result.gsm_reg_status
			&& strcmp(result.lac, cases[idx].result.lac) == 0
			&& strcmp(result.ci, cases[idx].result.ci) == 0)
		{
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d (%d,%d,\"%s\",\"%s\")\t%s\n", result.res, result.gsm_reg, result.gsm_reg_status, result.lac, result.ci, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cmti()
{
	static const struct test_case {
		const char	* input;
		int		result;
	} cases[] = {
		{ "+CMTI: \"ME\",41", 41 },
		{ "+CMTI: 0,111", 111 },
		{ "+CMTI: ", -1 },
	};
	unsigned idx = 0;
	char * input;
	int result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cmti", input);
		result = at_parse_cmti(input);
		if(result == cases[idx].result) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d\t%s\n", result, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

int safe_strcmp(const char *a, const char *b)
{
	if (a == NULL && b == NULL) {
		return 0;
	}
	if (a == NULL) {
		return -1;
	}
	if (b == NULL) {
		return 1;
	}
	return strcmp(a, b);
}

#/* */
void test_parse_cmgr()
{
	struct result {
		const char	* res;
		char		* str;
		char 		* oa;
		str_encoding_t	oa_enc;
		char		* msg;
		str_encoding_t	msg_enc;
		char            * msg_utf8;
	};
	static const struct test_case {
		const char	* input;
		struct result 	result;
	} cases[] = {
		{ "+CMGR: \"REC READ\",\"+79139131234\",,\"10/12/05,22:00:04+12\"\r\n041F04400438043204350442",
			{
				NULL,
				"\"REC READ\",\"+79139131234",
				"+79139131234",
				STR_ENCODING_7BIT,
				"041F04400438043204350442",
				STR_ENCODING_UNKNOWN,
				NULL
			}
		},
		{ "+CMGR: \"REC READ\",\"002B00370039003500330037003600310032003000350032\",,\"10/12/05,22:00:04+12\"\r\n041F04400438043204350442",
			{
				NULL,
				"\"REC READ\",\"002B00370039003500330037003600310032003000350032",
				"002B00370039003500330037003600310032003000350032",
				STR_ENCODING_UNKNOWN,
				"041F04400438043204350442",
				STR_ENCODING_UNKNOWN,
				NULL
			}
		},
		{ "+CMGR: 0,,106\r\n07911111111100F3040B911111111111F200000121702214952163B1582C168BC562B1984C2693C96432994C369BCD66B3D96C369BD168341A8D46A3D168B55AAD56ABD56AB59ACD66B3D96C369BCD76BBDD6EB7DBED76BBE170381C0E87C3E170B95C2E97CBE572B91C0C0683C16030180C",
			{
				NULL,
				"B1582C168BC562B1984C2693C96432994C369BCD66B3D96C369BD168341A8D46A3D168B55AAD56ABD56AB59ACD66B3D96C369BCD76BBDD6EB7DBED76BBE170381C0E87C3E170B95C2E97CBE572B91C0C0683C16030180C",
				"+11111111112",
				STR_ENCODING_7BIT,
				"B1582C168BC562B1984C2693C96432994C369BCD66B3D96C369BD168341A8D46A3D168B55AAD56ABD56AB59ACD66B3D96C369BCD76BBDD6EB7DBED76BBE170381C0E87C3E170B95C2E97CBE572B91C0C0683C16030180C",
				STR_ENCODING_7BIT_HEX_PAD_0,
				"111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999000000000"
			}
		},
		{ "+CMGR: 0,,159\r\n07919740430900F3440B912222222220F20008012180004390218C0500030003010031003100310031003100310031003100310031003200320032003200320032003200320032003200330033003300330033003300330033003300330034003400340034003400340034003400340034003500350035003500350035003500350035003500360036003600360036003600360036003600360037003700370037003700370037",
			{
				NULL,
				"0031003100310031003100310031003100310031003200320032003200320032003200320032003200330033003300330033003300330033003300330034003400340034003400340034003400340034003500350035003500350035003500350035003500360036003600360036003600360036003600360037003700370037003700370037",
				"+22222222022",
				STR_ENCODING_7BIT,
				"0031003100310031003100310031003100310031003200320032003200320032003200320032003200330033003300330033003300330033003300330034003400340034003400340034003400340034003500350035003500350035003500350035003500360036003600360036003600360036003600360037003700370037003700370037",
				STR_ENCODING_UCS2_HEX,
				"1111111111222222222233333333334444444444555555555566666666667777777"
			}
		},
		{ "+CMGR: 0,,159\r\n07913306000000F0440B913306000000F0000061011012939280A0050003CA020182E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3",
			{
				NULL,
				"82E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3",
				"+33600000000",
				STR_ENCODING_7BIT,
				"82E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3E13028180E87C3A060381C0E8382E170380C0A86C3",
				STR_ENCODING_7BIT_HEX_PAD_1,
				"Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaaaa Aaa"
			}
		},
		{ "+CMGR: 0,,43\r\n07913306000000F0640B913306000000F00000610110129303801B050003CA0202C26150301C0E8741C170381C0605C3E17018",
			{
				NULL,
				"C26150301C0E8741C170381C0605C3E17018",
				"+33600000000",
				STR_ENCODING_7BIT,
				"C26150301C0E8741C170381C0605C3E17018",
				STR_ENCODING_7BIT_HEX_PAD_1,
				"aa Aaaaa Aaaaa Aaaaa"
			}
		},
		{ "+CMGR: 0,,158\r\n07916407970970F6400A912222222222000041903021825180A0050003000301A9E5391D14060941439015240409414290102404094142901024040941429010240409414290106405594142901564055941429012A40429AD4AABD22A7481AC56101264455A915624C80AB282AC20A1D06A0559415610D20A4282AC2024C80AB282AC202BC80AB282AC2E9012B4042D414A90D2055282942E90D20502819420254809528294",
			{
				NULL,
				"A9E5391D14060941439015240409414290102404094142901024040941429010240409414290106405594142901564055941429012A40429AD4AABD22A7481AC56101264455A915624C80AB282AC20A1D06A0559415610D20A4282AC2024C80AB282AC202BC80AB282AC2E9012B4042D414A90D2055282942E90D20502819420254809528294",
				"+2222222222",
				STR_ENCODING_7BIT,
				"A9E5391D14060941439015240409414290102404094142901024040941429010240409414290106405594142901564055941429012A40429AD4AABD22A7481AC56101264455A915624C80AB282AC20A1D06A0559415610D20A4282AC2024C80AB282AC202BC80AB282AC2E9012B4042D414A90D2055282942E90D20502819420254809528294",
				STR_ENCODING_7BIT_HEX_PAD_1,
				"Test a B C V B B B B B B B B B B B B B B B B V V B V V V B J J JVJVJVB. VV H VHVHVH V V V BBVV V V HV H V H V V V V V V V. J K K J J. J J. J.   J J J J J"
			}
		},
		{ "+CMGR: 0,,55\r\n07912933035011804409D055F3DB5D060000411120712071022A080701030003990202A09976D7E9E5390B640FB3D364103DCD668364B3562CD692C1623417",
			{
				NULL,
				"A09976D7E9E5390B640FB3D364103DCD668364B3562CD692C1623417",
				"Ufone", /* 55F3DB5D062 */
				STR_ENCODING_7BIT_HEX_PAD_0,
				"A09976D7E9E5390B640FB3D364103DCD668364B3562CD692C1623417",
				STR_ENCODING_7BIT_HEX_PAD_5,
				"Minutes, valid till 23-11-2014."
			}
		},
		{ "+CMGR: 0,,137\r\n07919333851805320409D034186C360300F0713032810105408849A7F1099A36A720D9EC059BB140319C2E06D38186EF39FD0D1AA3D3E176981E06155D20184B467381926CD0585E26A7E96F1001547481683816ACE60241CB7250DA6D7E83E67550D95E76D3EB61761AF486EBD36F771A14A6D3D3F632A80C12BFDDF539485E9EA7C9F534688C4E87DB61100D968BD95C",
			{
				NULL,
				"49A7F1099A36A720D9EC059BB140319C2E06D38186EF39FD0D1AA3D3E176981E06155D20184B467381926CD0585E26A7E96F1001547481683816ACE60241CB7250DA6D7E83E67550D95E76D3EB61761AF486EBD36F771A14A6D3D3F632A80C12BFDDF539485E9EA7C9F534688C4E87DB61100D968BD95C",
				"40033", /* 09D034186C3603 */
				STR_ENCODING_7BIT_HEX_PAD_0,
				"49A7F1099A36A720D9EC059BB140319C2E06D38186EF39FD0D1AA3D3E176981E06155D20184B467381926CD0585E26A7E96F1001547481683816ACE60241CB7250DA6D7E83E67550D95E76D3EB61761AF486EBD36F771A14A6D3D3F632A80C12BFDDF539485E9EA7C9F534688C4E87DB61100D968BD95C",
				STR_ENCODING_7BIT_HEX_PAD_0,
				"INFO SMS 23/03, 18:10: Costo chiamata E. 0,24. Il credito è E. 48,05. Per info su eventuali opzioni attive e bonus residui chiama 40916."
			}
		},
	};

	unsigned idx = 0;
	char * input;
	struct result result;
	char oa[200];
	const char * msg;

	result.oa = oa;
	for (; idx < ITEMS_OF(cases); ++idx) {
		char buf[4096];
		int failidx = 0;
		result.str = input = strdup(cases[idx].input);
		result.msg_utf8 = buf;

		fprintf(stderr, "/* %u */ %s(\"%s\")...", idx, "at_parse_cmgr", input);
		result.res = at_parse_cmgr(
			&result.str, strlen(result.str), result.oa, sizeof(oa), &result.oa_enc,
			&result.msg, &result.msg_enc);

		/* convert to utf8 representation */
		if (!result.res && result.oa_enc != STR_ENCODING_UNKNOWN) {
			char tmp_oa[200];
			if (str_recode(RECODE_DECODE, result.oa_enc,
					result.oa, strlen(result.oa),
					tmp_oa, sizeof(tmp_oa)) >= 0) {
				strcpy(result.oa, tmp_oa);
			}
		}
		result.msg_utf8 = NULL;
		if (!result.res && result.msg_enc != STR_ENCODING_UNKNOWN) {
			if (str_recode(RECODE_DECODE, result.msg_enc,
					result.str, strlen(result.str),
					buf, sizeof(buf)) >= 0) {
				result.msg_utf8 = buf;
			}
		}

		if (++failidx && safe_strcmp(result.res, cases[idx].result.res) == 0 &&
		    ++failidx && safe_strcmp(result.str, cases[idx].result.str) == 0 &&
		    ++failidx && safe_strcmp(result.oa, cases[idx].result.oa) == 0 &&
		    ++failidx && result.oa_enc == cases[idx].result.oa_enc &&
		    ++failidx && safe_strcmp(result.msg, cases[idx].result.msg) == 0 &&
		    ++failidx && result.msg_enc == cases[idx].result.msg_enc &&
		    ++failidx && safe_strcmp(result.msg_utf8, cases[idx].result.msg_utf8) == 0)
		{
			msg = "OK";
			ok++;
			failidx = 0;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = '%s' ('%s','%s',%d,'%s',%d) [fail@%d]\n[text=%s]\t%s\n",
			result.res, result.str, result.oa, result.oa_enc,
			result.msg, result.msg_enc, failidx, result.msg_utf8, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cusd()
{
	struct result {
		int	res;
		int	type;
		char 	* cusd;
		int	dcs;
	};
	static const struct test_case {
		const char	* input;
		struct result 	result;
	} cases[] = {
		{ "+CUSD: 0,\"CF2135487D2E4130572D0682BB1A\",0", { 0, 0, "CF2135487D2E4130572D0682BB1A", 0} },
		{ "+CUSD: 1,\"CF2135487D2E4130572D0682BB1A\",1", { 0, 1, "CF2135487D2E4130572D0682BB1A", 1} },
		{ "+CUSD: 5", { 0, 5, "", -1} },
	};
	unsigned idx = 0;
	char * input;
	struct result result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cusd", input);
		result.res = at_parse_cusd(input, &result.type, &result.cusd, &result.dcs);
		if(result.res == cases[idx].result.res
			&& result.type == cases[idx].result.type
			&& result.dcs == cases[idx].result.dcs
			&& strcmp(result.cusd, cases[idx].result.cusd) == 0)
		{
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d (%d,\"%s\",%d)\t%s\n", result.res, result.type, result.cusd, result.dcs, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cpin()
{
}

#/* */
void test_parse_csq()
{
}

#/* */
void test_parse_rssi()
{
}

#/* */
void test_parse_mode()
{
}

#/* */
void test_parse_csca()
{
}

#/* */
void test_parse_clcc()
{
	struct result {
		int		res;

		unsigned	index;
		unsigned	dir;
		unsigned	stat;
		unsigned	mode;
		unsigned	mpty;
		char		* number;
		unsigned	toa;
	};
	static const struct test_case {
		const char	* input;
		struct result	result;
	} cases[] = {
		{ "+CLCC: 1,1,4,0,0,\"\",145", { 0, 1, 1, 4, 0, 0, "", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+79139131234\",145", { 0, 1, 1, 4, 0, 0, "+79139131234", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+7913913ABCA\",145", { 0, 1, 1, 4, 0, 0, "+7913913ABCA", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+7913913ABCA\"", { -1, 0, 0, 0, 0, 0, "", 0} },
	};
	unsigned idx = 0;
	char * input;
	struct result result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_clcc", input);
		result.res = at_parse_clcc(
			input, &result.index, &result.dir, &result.stat, &result.mode,
			&result.mpty, &result.number, &result.toa);
		if(result.res == cases[idx].result.res
			&& result.index == cases[idx].result.index
			&& result.dir == cases[idx].result.dir
			&& result.stat == cases[idx].result.stat
			&& result.mode == cases[idx].result.mode
			&& result.mpty == cases[idx].result.mpty
			&& strcmp(result.number, cases[idx].result.number) == 0
			&& result.toa == cases[idx].result.toa)
		{
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d (%d,%d,%d,%d,%d,\"%s\",%d)\t%s\n",
			result.res, result.index, result.dir, result.stat, result.mode,
			result.mpty, result.number, result.toa, msg);
		free(input);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_ccwa()
{
}

#/* */
int main()
{
	test_parse_cnum();
	test_parse_cops();
	test_parse_creg();
	test_parse_cmti();
	test_parse_cmgr();
	test_parse_cusd();
	test_parse_cpin();
	test_parse_csq();
	test_parse_rssi();
	test_parse_mode();
	test_parse_csca();
	test_parse_clcc();
	test_parse_ccwa();
	
	fprintf(stderr, "done %d tests: %d OK %d FAILS\n", ok + faults, ok, faults);

	if (faults) {
		return 1;
	}
	return 0;
}
