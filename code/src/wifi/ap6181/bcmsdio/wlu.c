/*
 * Common code for wl command-line swiss-army-knife utility
 *u
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlu.c,v 1.1084.2.43 2011-02-10 22:46:22 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <typedefs.h>
#include <epivers.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <proto/802.1d.h>
#include <proto/802.11e.h>
#include <proto/wpa.h>
#include <proto/bcmip.h>
#include <proto/bt_amp_hci.h>
#include <wlioctl.h>
#include <sdiovar.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include "wlu.h"
#include <bcmcdc.h>
#include <miniopt.h>
#include "wl_drv.h"
#include "wlm.h"

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
cmd_func_t wl_int;
static cmd_func_t wl_rssi;
static cmd_func_t wl_radio, wl_version, wl_list, wl_band, wl_bandlist, wl_phylist;
static cmd_func_t wl_hostip, wl_arp_stats;

int wlu_get(void *wl, int cmd, void *buf, int len);
int wlu_set(void *wl, int cmd, void *buf, int len);
static int wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num);
static int wl_parse_chanspec_list(char* list_str, chanspec_t *chanspec_list, int chanspec_num);
static int wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed);
int wl_set_scan(void *wl);
int wl_get_scan(void *wl, int opc, char *buf, uint buf_len);
int wl_get_iscan(void *wl, char *buf, uint buf_len);
static uint wl_iovar_mkbuf(const char *name, char *data, uint datalen, char *buf, uint buflen, int *perr);
static int wlu_iovar_getbuf(void* wl, const char *iovar, void *param, int paramlen, void *bufptr, int buflen);
static int wlu_iovar_setbuf(void* wl, const char *iovar, void *param, int paramlen, void *bufptr, int buflen);
int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len);
int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
int wlu_iovar_setint(void *wl, const char *iovar, int val);

/* dword align allocation */
static union {
	char bufdata[WLC_IOCTL_MAXLEN];
	uint32 alignme;
} bufstruct_wlu;
static char *buf = (char*) &bufstruct_wlu.bufdata;

/* integer output format, default to signed integer */
static uint8 int_fmt;

/* The below macros handle endian mis-matches between wl utility and wl driver. */
static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define htod16(i) (g_swap?bcmswap16(i):(uint16)(i))
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh16(i) (g_swap?bcmswap16(i):(uint16)(i))
#define htodchanspec(i) (g_swap?htod16(i):i)
#define dtohchanspec(i) (g_swap?dtoh16(i):i)
#define htodenum(i) (g_swap?((sizeof(i) == 4) ? htod32(i) : ((sizeof(i) == 2) ? htod16(i) : i)):i)
#define dtohenum(i) (g_swap?((sizeof(i) == 4) ? dtoh32(i) : ((sizeof(i) == 2) ? htod16(i) : i)):i)

/*
 * Country names and abbreviations from ISO 3166
 */
typedef struct {
	const char *name;   /* Long name */
	const char *abbrev; /* Abbreviation */
} cntry_name_t;
cntry_name_t cntry_names[]; /* At end of this file */

#define WL_SCAN_PARAMS_SSID_MAX 10
#define SCAN_USAGE  "" \
"\tDefault to an active scan across all channels for any SSID.\n" \
"\tOptional arg: SSIDs, list of [up to 10] SSIDs to scan (comma or space separated).\n" \
"\tOptions:\n" \
"\t-s S, --ssid=S\t\tSSIDs to scan\n" \
"\t-t ST, --scan_type=ST\t[active|passive|prohibit] scan type\n" \
"\t--bss_type=BT\t\t[bss/infra|ibss/adhoc] bss type to scan\n" \
"\t-b MAC, --bssid=MAC\tparticular BSSID MAC address to scan, xx:xx:xx:xx:xx:xx\n" \
"\t-n N, --nprobes=N\tnumber of probes per scanned channel\n" \
"\t-a N, --active=N\tdwell time per channel for active scanning\n" \
"\t-p N, --passive=N\tdwell time per channel for passive scanning\n" \
"\t-h N, --home=N\t\tdwell time for the home channel between channel scans\n" \
"\t-c L, --channels=L\tcomma or space separated list of channels to scan" \

void wlu_init(void)
{
	/* Init global variables at run-time, not as part of the declaration.
	 * This is required to support init/de-init of the driver. Initialization
	 * of globals as part of the declaration results in non-deterministic
	 * behaviour since the value of the globals may be different on the
	 * first time that the driver is initialized vs subsequent initializations.
	 */
	int_fmt = INT_FMT_DEC;
}

/* Common routine to check for an option arg specifying the configuration index.
 * Takes the syntax -C num, --cfg=num, --config=num, or --configuration=num
 * Returns -1 if there is a command line parsing error.
 * Returns 0 if no error, and sets *consumed to the number of argv strings
 * used. Sets *bsscfg_idx to the index to use. Will set *bsscfg_idx to zero if there
 * was no config arg.
 */
static int wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed)
{
	miniopt_t mo;
	int opt_err;

	*bsscfg_idx = 0;
	*consumed = 0;

	miniopt_init(&mo, fn_name, NULL, FALSE);

	/* process the first option */
	opt_err = miniopt(&mo, argv);

	/* check for no args or end of options */
	if (opt_err == -1)
		return 0;

	/* check for no options, just a positional arg encountered */
	if (mo.positional)
		return 0;

	/* check for error parsing options */
	if (opt_err == 1)
		return -1;

	/* check for -C, --cfg=X, --config=X, --configuration=X */
	if (mo.opt == 'C' ||
		!strcmp(mo.key, "cfg") ||
		!strcmp(mo.key, "config") ||
		!strcmp(mo.key, "configuration")) {
		if (!mo.good_int) {
			osl_ext_printf("%s: could not parse \"%s\" as an integer for the configuartion index\n",
			fn_name, mo.valstr);
			return -1;
		}
		*bsscfg_idx = mo.val;
		*consumed = mo.consumed;
	}

	return 0;
}

int wl_int(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get == -1)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;

		val = dtoh32(val);
	} else {
		if (cmd->set == -1)
			return -1;
		if (!bcmstricmp(*argv, "on"))
			val = 1;
		else if (!bcmstricmp(*argv, "off"))
			val = 0;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}

		val = htod32(val);
		ret = wlu_set(wl, cmd->set, &val, sizeof(int));
	}

	return ret;
}

/* Command may or may not take a MAC address */
static int wl_rssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	scb_val_t scb_val;
	int32 rssi;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &rssi, sizeof(rssi))) < 0)
			return ret;
		osl_ext_printf("%d\n", dtoh32(rssi));
		return 0;
	} else {
		if (!wl_ether_atoe(*argv, &scb_val.ea))
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &scb_val, sizeof(scb_val))) < 0)
			return ret;
		osl_ext_printf("%d\n", dtoh32(scb_val.val));
		return 0;
	}
}

int wl_get_rssi(int *rssi)
{
	int ret;
	cmd_t cmd = { "rssi", wl_rssi, WLC_GET_RSSI, -1,
				  "Get the current RSSI val, for an AP you must specify the mac addr of the STA" };

	if ((ret = wlu_get(NULL, cmd.get, rssi, sizeof(rssi))) < 0)
		return ret;

	return 0;
}
	
int wl_check_scan_status(void)
{
    int ret;
	channel_info_t ci;

	memset(&ci, 0, sizeof(ci));
	if ((ret = wlu_get(NULL, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t))) < 0)
		return ret;

    return ci.scan_channel;
}

int wl_get_channel(int *ch)
{
	int ret;
	channel_info_t ci;

	memset(&ci, 0, sizeof(ci));
	if ((ret = wlu_get(NULL, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t))) < 0)
		return ret;

	*ch = dtoh32(ci.hw_channel);
	return 0;
}

int wl_ether_atoe(const char *a, struct ether_addr *n)
{
	char *c = NULL;
	int i = 0;

	memset((void*)n, 0, ETHER_ADDR_LEN);
	for (;;) {
		n->octet[i++] = (uint8)strtoul(a, &c, 16);
		if (!*c++ || i == ETHER_ADDR_LEN)
			break;
		a = c;
	}
	return (i == ETHER_ADDR_LEN);
}

char * wl_ether_etoa(const struct ether_addr *n)
{
	static char etoa_buf[ETHER_ADDR_LEN * 3];
	char *c = etoa_buf;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", n->octet[i] & 0xff);
	}
	return etoa_buf;
}

int
wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len)
{
	int i, c;
	char *p = ssid_buf;

	if (ssid_len > 32) ssid_len = 32;

	for (i = 0; i < ssid_len; i++) {
		c = (int)ssid[i];
		if (c == '\\') {
			*p++ = '\\';
			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
}

/* WLC_GET_RADIO and WLC_SET_RADIO in driver operate on radio_disabled which
 * is opposite of "wl radio [1|0]".  So invert for user.
 * In addition, display WL_RADIO_SW_DISABLE and WL_RADIO_HW_DISABLE bits.
 */
static int
wl_radio(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;
		val = dtoh32(val);
		osl_ext_printf("0x%04x\n", val);
		return 0;
	} else {
		if (cmd->set < 0)
			return -1;
		if (!bcmstricmp(*argv, "on"))
			val = WL_RADIO_SW_DISABLE << 16;
		else if (!bcmstricmp(*argv, "off"))
			val = WL_RADIO_SW_DISABLE << 16 | WL_RADIO_SW_DISABLE;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}

			/* raw bits setting, add the mask if not provided */
			if ((val >> 16) == 0) {
				val |= val << 16;
			}
		}
		val = htod32(val);
		return wlu_set(wl, cmd->set, &val, sizeof(int));
	}
}

/* wl scan
 * -s --ssid=ssid_list
 * -t T --scan_type=T : [active|passive]
 * --bss_type=T : [infra|bss|adhoc|ibss]
 * -b --bssid=
 * -n --nprobes=
 * -a --active=
 * -p --passive=
 * -h --home=
 * -c --channels=
 * ssid_list
 */

/* Parse a comma-separated list from list_str into ssid array, starting
 * at index idx.  Max specifies size of the ssid array.  Parses ssids
 * and returns updated idx; if idx >= max not all fit, the excess have
 * not been copied.  Returns -1 on empty string, or on ssid too long.
 */
static int wl_parse_ssid_list(char* list_str, wlc_ssid_t* ssid, int idx, int max)
{
	char *str, *ptr;

	if (list_str == NULL)
		return -1;

	for (str = list_str; str != NULL; str = ptr) {
		if ((ptr = strchr(str, ',')) != NULL)
			*ptr++ = '\0';

		if (strlen(str) > DOT11_MAX_SSID_LEN) {
			osl_ext_printf("ssid <%s> exceeds %d\n", str, DOT11_MAX_SSID_LEN);
			return -1;
		}
		if (strlen(str) == 0)
			ssid[idx].SSID_len = 0;

		if (idx < max) {
			strcpy((char*)ssid[idx].SSID, str);
			ssid[idx].SSID_len = strlen(str);
		}
		idx++;
	}

	return idx;
}

static int wl_scan_prep(void *wl, cmd_t *cmd, char **argv, wl_scan_params_t *params, int *params_size)
{
	int val = 0;
	char key[64];
	int keylen;
	char *p, *eq, *valstr, *endptr = NULL;
	char opt;
	bool positional_param;
	bool good_int;
	bool opt_end;
	int err = 0;
	int i;

	int nchan = 0;
	int nssid = 0;
	wlc_ssid_t ssids[WL_SCAN_PARAMS_SSID_MAX];

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	memcpy((void*)&params->bssid, (const void*)&ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0;
    params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = 100;
	params->home_time = -1;
	params->channel_num = 0;
	memset(ssids, 0, WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t));

	/* skip the command name */
	argv++;

	opt_end = FALSE;
	while ((p = *argv) != NULL) {
		argv++;
		positional_param = FALSE;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;
		good_int = FALSE;

		if (opt_end) {
			positional_param = TRUE;
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				osl_ext_printf(
				"wl_scan: missing \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63) keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				osl_ext_printf(
				"wl_scan: missing value after \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				osl_ext_printf(
				"wl_scan: only single char options, error on param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			if (*argv == NULL) {
				osl_ext_printf(
				"wl_scan: missing value parameter after \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			positional_param = TRUE;
			valstr = p;
		}

		/* parse valstr as int just in case */
		if (valstr) {
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				/* not all the value string was parsed by strtol */
				good_int = TRUE;
			}
		}

		if (opt == 's' || !strcmp(key, "ssid") || positional_param) {
			nssid = wl_parse_ssid_list(valstr, ssids, nssid, WL_SCAN_PARAMS_SSID_MAX);
			if (nssid < 0) {
				err = -1;
				goto exit;
			}
		}
		if (opt == 't' || !strcmp(key, "scan_type")) {
			if (!strcmp(valstr, "active")) {
				params->scan_type = 0;
			} else if (!strcmp(valstr, "passive")) {
				params->scan_type = WL_SCANFLAGS_PASSIVE;
			} else if (!strcmp(valstr, "prohibit")) {
				params->scan_type = WL_SCANFLAGS_PROHIBITED;
			} else {
				osl_ext_printf(
				"scan_type value should be \"active\" "
				"or \"passive\", but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (!strcmp(key, "bss_type")) {
			if (!strcmp(valstr, "bss") || !strcmp(valstr, "infra")) {
				params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
			} else if (!strcmp(valstr, "ibss") || !strcmp(valstr, "adhoc")) {
				params->bss_type = DOT11_BSSTYPE_INDEPENDENT;
			} else if (!strcmp(valstr, "any")) {
				params->bss_type = DOT11_BSSTYPE_ANY;
			} else {
				osl_ext_printf(
				"bss_type value should be "
				"\"bss\", \"ibss\", or \"any\", but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 'b' || !strcmp(key, "bssid")) {
			if (!wl_ether_atoe(valstr, &params->bssid)) {
				osl_ext_printf(
				"could not parse \"%s\" as an ethernet MAC address\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 'n' || !strcmp(key, "nprobes")) {
			if (!good_int) {
				osl_ext_printf(
				"could not parse \"%s\" as an int for value nprobes\n", valstr);
				err = -1;
				goto exit;
			}
			params->nprobes = val;
		}
		if (opt == 'a' || !strcmp(key, "active")) {
			if (!good_int) {
				osl_ext_printf(
				"could not parse \"%s\" as an int for active dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->active_time = val;
		}
		if (opt == 'p' || !strcmp(key, "passive")) {
			if (!good_int) {
				osl_ext_printf(
				"could not parse \"%s\" as an int for passive dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->passive_time = val;
		}
		if (opt == 'h' || !strcmp(key, "home")) {
			if (!good_int) {
				osl_ext_printf(
				"could not parse \"%s\" as an int for home channel dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->home_time = val;
		}
		if (opt == 'c' || !strcmp(key, "channels")) {
			nchan = wl_parse_channel_list(valstr, params->channel_list,
				WL_NUMCHANNELS);
			if (nchan == -1) {
				osl_ext_printf("error parsing channel list arg\n");
				err = -1;
				goto exit;
			}
		}
	}

	if (nssid > WL_SCAN_PARAMS_SSID_MAX) {
		osl_ext_printf("ssid count %d exceeds max of %d\n",
			nssid, WL_SCAN_PARAMS_SSID_MAX);
		err = -1;
		goto exit;
	}

	params->nprobes = htod32(params->nprobes);
	params->active_time = htod32(params->active_time);
	params->passive_time = htod32(params->passive_time);
	params->home_time = htod32(params->home_time);

	for (i = 0; i < nchan; i++) {
		params->channel_list[i] = htodchanspec(params->channel_list[i]);
	}

	for (i = 0; i < nssid; i++) {
		ssids[i].SSID_len = htod32(ssids[i].SSID_len);
	}

	/* For a single ssid, use the single fixed field */
	if (nssid == 1) {
		nssid = 0;
		memcpy(&params->ssid, &ssids[0], sizeof(ssids[0]));
	}

	/* Copy ssid array if applicable */
	if (nssid > 0) {
		i = OFFSETOF(wl_scan_params_t, channel_list) + nchan * sizeof(uint16);
		i = ROUNDUP(i, sizeof(uint32));
		if (i + nssid * sizeof(wlc_ssid_t) > (uint)*params_size) {
			osl_ext_printf("additional ssids exceed params_size\n");
			err = -1;
			goto exit;
		}

		p = (char*)params + i;
		memcpy(p, ssids, nssid * sizeof(wlc_ssid_t));
		p += nssid * sizeof(wlc_ssid_t);
	} else {
		p = (char*)params->channel_list + nchan * sizeof(uint16);
	}

	params->channel_num = htod32((nssid << WL_SCAN_PARAMS_NSSID_SHIFT) |
		(nchan & WL_SCAN_PARAMS_COUNT_MASK));
	*params_size = p - (char*)params + nssid * sizeof(wlc_ssid_t);
exit:
	return err;
}


static int wl_scan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);
	wl_scan_params_t *params;
    unsigned char scan_buff[WL_SCAN_PARAMS_FIXED_SIZE +
                        WL_NUMCHANNELS * sizeof(uint16) +
                        WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t) + 4];
    int err = 0;

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_scan_params_t *)scan_buff;
	if (params == NULL) return -1;

	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, params, &params_size);
	if (!err) err = wlu_set(wl, cmd->set, params, params_size);
	return err;
}

static int wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str)
			return -1;
		
		str = endptr + strspn(endptr, " ,");
		if (num == channel_num)
			return -1;

		channel_list[num++] = (uint16)val;
	}

	return num;
}

static int wl_parse_chanspec_list(char *list_str, chanspec_t *chanspec_list, int chanspec_num)
{
	int num = 0;
	chanspec_t chanspec;
	char *next, str[8];
	size_t len;

	if ((next = list_str) == NULL)
		return BCME_ERROR;

	while ((len = strcspn(next, " ,")) > 0) {
		if (len >= sizeof(str)) {
			osl_ext_printf("string \"%s\" before ',' or ' ' is too long\n", next);
			return BCME_ERROR;
		}
		strncpy(str, next, len);
		str[len] = 0;
		chanspec = wf_chspec_aton(str);
		if (chanspec == 0) {
			osl_ext_printf("could not parse chanspec starting at "
					"\"%s\" in list:\n%s\n", str, list_str);
			return BCME_ERROR;
		}
		if (num == chanspec_num) {
			osl_ext_printf("too many chanspecs (more than %d) in chanspec list:\n%s\n",
				chanspec_num, list_str);
			return BCME_ERROR;
		}
		chanspec_list[num++] = chanspec;
		next += len;
		next += strspn(next, " ,");
	}

	return num;
}

int wl_set_scan(void *wl)
{
	char *argv[5] = {"scan", NULL, NULL, NULL, NULL};
	cmd_t cmd = { "scan", wl_scan, -1, WLC_SCAN, "Initiate a scan.\n" SCAN_USAGE };

	return wl_scan(wl, &cmd, argv);
}

int wl_set_ap_scan(char *ssid)
{
    char *argv[5];
	cmd_t cmd = { "scan", wl_scan, -1, WLC_SCAN, "Initiate a scan.\n" SCAN_USAGE };

    if(ssid == NULL) return -1;

    argv[0] = "scan";
    argv[1] = "-s";
    argv[2] = ssid;
    argv[3] = NULL;
	return wl_scan(NULL, &cmd, argv);
}

int wl_get_scan(void *wl, int opc, char *scan_buf, uint buf_len)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_buf;
	int ret;

	list->buflen = htod32(buf_len);
	ret = wlu_get(wl, opc, scan_buf, buf_len);
	if (ret < 0)
		return ret;
	ret = 0;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
		list->version != LEGACY2_WL_BSS_INFO_VERSION &&
		list->version != LEGACY_WL_BSS_INFO_VERSION) {
		osl_ext_printf("Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}

	return ret;
}

int wl_get_iscan(void *wl, char *scan_buf, uint buf_len)
{
	wl_iscan_results_t list;
	wl_scan_results_t *results;
	int ret;

	memset(&list, '\0', sizeof(list));
	list.results.buflen = htod32(buf_len);
	ret = wlu_iovar_getbuf(wl, "iscanresults", &list, WL_ISCAN_RESULTS_FIXED_SIZE,
		scan_buf, WLC_IOCTL_MAXLEN);

	if (ret < 0)
		return ret;

	ret = 0;

	results = &((wl_iscan_results_t*)scan_buf)->results;
	results->buflen = dtoh32(results->buflen);
	results->version = dtoh32(results->version);
	results->count = dtoh32(results->count);
	if (results->buflen == 0) {
		osl_ext_printf("wl_get_iscan buflen 0\n");
		results->version = 0;
		results->count = 0;
	} else if (results->version != WL_BSS_INFO_VERSION &&
		results->version != LEGACY2_WL_BSS_INFO_VERSION &&
		results->version != LEGACY_WL_BSS_INFO_VERSION) {
		osl_ext_printf("Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			results->version, WL_BSS_INFO_VERSION);
		results->buflen = 0;
		results->count = 0;
	}

	return ret;
}

static cntry_name_t * wlc_cntry_name_to_country(char *long_name)
{
	cntry_name_t *cntry;
	for (cntry = cntry_names; cntry->name &&
		bcmstricmp(long_name, cntry->name); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static cntry_name_t * wlc_cntry_abbrev_to_country(const char *abbrev)
{
	cntry_name_t *cntry;
	if (!*abbrev || strlen(abbrev) > 3 || strlen(abbrev) < 2)
		return (NULL);
	for (cntry = cntry_names; cntry->name &&
		bcmstrnicmp(abbrev, cntry->abbrev, strlen(abbrev)); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static int wl_parse_country_spec(const char *spec, char *ccode, int *regrev)
{
	char *revstr;
	char *endptr = NULL;
	int ccode_len;
	int rev = -1;

	revstr = strchr(spec, '/');

	if (revstr) {
		rev = strtol(revstr + 1, &endptr, 10);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			osl_ext_printf(
				"Could not parse \"%s\" as a regulatory revision "
				"in the country string \"%s\"\n",
				revstr + 1, spec);
			return USAGE_ERROR;
		}
	}

	if (revstr)
		ccode_len = (int)(uintptr)(revstr - spec);
	else
		ccode_len = (int)strlen(spec);

	if (ccode_len > 3) {
		osl_ext_printf(
			"Could not parse a 2-3 char country code "
			"in the country string \"%s\"\n",
			spec);
		return USAGE_ERROR;
	}

	memcpy(ccode, spec, ccode_len);
	ccode[ccode_len] = '\0';
	*regrev = rev;

	return 0;
}

int wl_country(void *wl, cmd_t *cmd, char **argv)
{
	cntry_name_t *cntry;
	wl_country_t cspec = {{0}, 0, {0}};
	int argc = 0;
	int err;
	int bcmerr = 1;

	/* skip the command name */
	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	/* check arg list count */
	if (argc > 2) {
		osl_ext_printf("Too many arguments (%d) for command %s\n", argc, cmd->name);
		return USAGE_ERROR;
	}

	buf[0] = 0;
	if (argc == 0) {
		const char* name = "<unknown>";

		/* first try the country iovar */
		err = wlu_iovar_get(wl, "country", &cspec, sizeof(cspec));

		if (!err) {
			cntry = wlc_cntry_abbrev_to_country(cspec.country_abbrev);
			if (cntry)
				name = cntry->name;

			osl_ext_printf("%s (%s/%d) %s\n",
				cspec.country_abbrev, cspec.ccode, cspec.rev, name);

			return 0;
		}

		/* if there was an error other than BCME_UNSUPPORTED, fail now */
		wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (bcmerr != BCME_UNSUPPORTED)
			return err;

		/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl */
		if ((err = wlu_get(wl, cmd->get, &buf[0], WLC_IOCTL_SMLEN)))
			return err;
		if (strlen(buf) == 0) {
			osl_ext_printf("No country set\n");
			return 0;

		}
		cntry = wlc_cntry_abbrev_to_country(buf);
		if (cntry != NULL)
			name = cntry->name;

		osl_ext_printf("%s () %s\n", buf, name);
		return 0;
	}

	if (!bcmstricmp(*argv, "list")) {
		uint i;
		const char* abbrev;
		wl_country_list_t *cl = (wl_country_list_t *)buf;

		cl->buflen = WLC_IOCTL_MAXLEN;
		cl->count = 0;

		/* band may follow */
		if (*++argv) {
			cl->band_set = TRUE;
			if (!bcmstricmp(*argv, "a"))
				cl->band = WLC_BAND_5G;
			else if (!bcmstricmp(*argv, "b") || !bcmstricmp(*argv, "g"))
				cl->band = WLC_BAND_2G;
			else {
				osl_ext_printf("unsupported band: %s\n", *argv);
				return -1;
			}
		} else {
			cl->band_set = FALSE;
		}

		cl->buflen = htod32(cl->buflen);
		cl->band_set = htod32(cl->band_set);
		cl->band = htod32(cl->band);
		cl->count = htod32(cl->count);
		err = wlu_get(wl, WLC_GET_COUNTRY_LIST, buf, WLC_IOCTL_MAXLEN);
		if (err < 0)
			return err;

		osl_ext_printf("Supported countries: country code and long name\n");
		for (i = 0; i < dtoh32(cl->count); i++) {
			abbrev = &cl->country_abbrev[i*WLC_CNTRY_BUF_SZ];
			cntry = wlc_cntry_abbrev_to_country(abbrev);
			osl_ext_printf("%s\t%s\n", abbrev, cntry ? cntry->name : "");
		}
		return 0;
	}

	memset(&cspec, 0, sizeof(cspec));
	cspec.rev = -1;

	if (argc == 1) {
		/* check for the first arg being a country name, e.g. "United States",
		 * or country spec, "US/1", or just a country code, "US"
		 */
		if ((cntry = wlc_cntry_name_to_country(argv[0])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
			err = 0;
		} else {
			/* parse a country spec, e.g. "US/1", or a country code.
			 * cspec.rev will be -1 if not specified.
			 */
			err = wl_parse_country_spec(argv[0], cspec.country_abbrev, &cspec.rev);
		}

		if (err) {
			osl_ext_printf(
				"Argument \"%s\" could not be parsed as a country name, "
				"country code, or country code and regulatory revision.\n",
				argv[0]);
			return USAGE_ERROR;
		}

		/* if the arg was a country spec, then fill out ccdoe and rev,
		 * and leave country_abbrev defaulted to the ccode
		 */
		if (cspec.rev != -1)
			memcpy(cspec.ccode, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);
	} else {
		/* for two args, the first needs to be a country code or country spec */
		err = wl_parse_country_spec(argv[0], cspec.ccode, &cspec.rev);
		if (err) {
			osl_ext_printf(
				"Argument 1 \"%s\" could not be parsed as a country code, or "
				"country code and regulatory revision.\n",
				argv[0]);
			return USAGE_ERROR;
		}

		/* the second arg needs to be a country name or country code */
		if ((cntry = wlc_cntry_name_to_country(argv[1])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
		} else {
			int rev;
			err = wl_parse_country_spec(argv[1], cspec.country_abbrev, &rev);
			if (rev != -1) {
				osl_ext_printf(
					"Argument \"%s\" had a revision. Arg 2 must be "
					"a country name or country code without a revision\n",
					argv[1]);
				return USAGE_ERROR;
			}
		}

		if (err) {
			osl_ext_printf(
				"Argument 2 \"%s\" could not be parsed as "
				"a country name or country code\n",
				argv[1]);
			return USAGE_ERROR;
		}
	}

	/* first try the country iovar */
	if (cspec.rev == -1 && cspec.ccode[0] == '\0')
		err = wlu_iovar_set(wl, "country", &cspec, WLC_CNTRY_BUF_SZ);
	else
		err = wlu_iovar_set(wl, "country", &cspec, sizeof(cspec));

	if (err == 0)
		return 0;

	/* if there was an error other than BCME_UNSUPPORTED, fail now */
	wlu_iovar_getint(wl, "bcmerror", &bcmerr);
	if (bcmerr != BCME_UNSUPPORTED)
		return err;

	/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl if possible */

	if (cspec.rev != -1 || cspec.ccode[0] != '\0') {
		osl_ext_printf(
			"Driver does not support full country spec interface, "
			"only a country name or code may be sepcified\n");
		return err;
	}

	/* use the legacy ioctl */
	err = wlu_set(wl, WLC_SET_COUNTRY, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);

	return err;
}

static uint wl_iovar_mkbuf(const char *name, char *data, uint datalen, char *iovar_buf, uint buflen, int *perr)
{
	uint iovar_len;
	char *p;

	iovar_len = strlen(name) + 1;

	/* check for overflow */
	if ((iovar_len + datalen) > buflen) {
		*perr = BCME_BUFTOOSHORT;
		return 0;
	}

	/* copy data to the buffer past the end of the iovar name string */
	if (datalen > 0)
		memmove(&iovar_buf[iovar_len], data, datalen);

	/* copy the name to the beginning of the buffer */
	strcpy(iovar_buf, name);

	/* wl command line automatically converts iovar names to lower case for
	 * ease of use
	 */
	p = iovar_buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}

	*perr = 0;
	return (iovar_len + datalen);
}


/*
 * get named iovar providing both parameter and i/o buffers
 * iovar name is converted to lower case
 */
static int wlu_iovar_getbuf(void* wl, const char *iovar, void *param, int paramlen, void *bufptr, int buflen)
{
	int err;

	wl_iovar_mkbuf(iovar, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_get(wl, WLC_GET_VAR, bufptr, buflen);
}

/*
 * set named iovar providing both parameter and i/o buffers
 * iovar name is converted to lower case
 */
static int wlu_iovar_setbuf(void* wl, const char *iovar, void *param, int paramlen, void *bufptr, int buflen)
{
	int err;
	int iolen;

	iolen = wl_iovar_mkbuf(iovar, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_set(wl, WLC_SET_VAR, bufptr, iolen);
}

/*
 * get named iovar without parameters into a given buffer
 * iovar name is converted to lower case
 */
int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len)
{
	char smbuf[WLC_IOCTL_SMLEN];
	int err;

	/* use the return buffer if it is bigger than what we have on the stack */
	if (len > (int)sizeof(smbuf)) {
		err = wlu_iovar_getbuf(wl, iovar, NULL, 0, outbuf, len);
	} else {
		memset(smbuf, 0, sizeof(smbuf));
		err = wlu_iovar_getbuf(wl, iovar, NULL, 0, smbuf, sizeof(smbuf));
		if (err == 0)
			memcpy(outbuf, smbuf, len);
	}

	return err;
}

/*
 * set named iovar given the parameter buffer
 * iovar name is converted to lower case
 */
int
wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen)
{
	char smbuf[ETHER_MAX_LEN];

	memset(smbuf, 0, sizeof(smbuf));

	return wlu_iovar_setbuf(wl, iovar, param, paramlen, smbuf, sizeof(smbuf));
}

/*
 * get named iovar as an integer value
 * iovar name is converted to lower case
 */
int
wlu_iovar_getint(void *wl, const char *iovar, int *pval)
{
	int ret;

	ret = wlu_iovar_get(wl, iovar, pval, sizeof(int));
	if (ret >= 0)
	{
		*pval = dtoh32(*pval);
	}
	return ret;
}

/*
 * set named iovar given an integer parameter
 * iovar name is converted to lower case
 */
int wlu_iovar_setint(void *wl, const char *iovar, int val)
{
	val = htod32(val);
	return wlu_iovar_set(wl, iovar, &val, sizeof(int));
}

static const char ac_names[AC_COUNT][6] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};

#define PRVAL(name) pbuf += sprintf(pbuf, "%s %d ", #name, dtoh32(cnt.name))
#define PRNL()      pbuf += sprintf(pbuf, "\n")

static int
wl_phy_rssiant(void *wl, cmd_t *cmd, char **argv)
{
	uint32 antindex;
	int buflen, err;
	char *param;
	int16 antrssi;

	if (!*++argv) {
		osl_ext_printf(" Usage: %s antenna_index[0-3]\n", cmd->name);
		return -1;
	}

	antindex = htod32(atoi(*argv));

	strcpy(buf, "phy_rssiant");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&antindex, sizeof(antindex));

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)))
		return err;

	antindex = dtoh32(antindex);
	antrssi = dtoh16(*(int16 *)buf);
	osl_ext_printf("\nphy_rssiant ant%d = %d\n", antindex, antrssi);

	return (0);
}

int wlu_get(void *wl, int cmd, void *cmdbuf, int len)
{
    int ret = 0;
	wl_drv_ioctl_t ioc;

	memset(&ioc, 0, sizeof(ioc));
	ioc.w.cmd = cmd;
	ioc.w.buf = cmdbuf;
	ioc.w.len = len;
	ioc.w.set = FALSE;

	if (wl_drv_ioctl(NULL, &ioc) < 0) {
		ret = IOCTL_ERROR;
	}

	return (ret);
}

/* now IOCTL SET commands shall call wlu_set() instead of wl_set() so that the commands
 * can be batched when needed
 */
int wlu_set(void *wl, int cmd, void *cmdbuf, int len)
{
    int ret = 0;
	wl_drv_ioctl_t ioc;

	memset(&ioc, 0, sizeof(ioc));
	ioc.w.cmd = cmd;
	ioc.w.buf = cmdbuf;
	ioc.w.len = len;
	ioc.w.set = TRUE;

	if (wl_drv_ioctl(NULL, &ioc) < 0) {
		ret = IOCTL_ERROR;
	}

	return (ret);
}

/* this is the batched command packet size. now for remoteWL, we set it to 512 bytes */
#define MEMBLOCK (512 - 32) /* allow 32 bytes for overhead (header, alignment, etc) */

cntry_name_t cntry_names[] = {

{"AFGHANISTAN",     "AF"},
{"ALBANIA",     "AL"},
{"ALGERIA",     "DZ"},
{"AMERICAN SAMOA",  "AS"},
{"ANDORRA",     "AD"},
{"ANGOLA",      "AO"},
{"ANGUILLA",        "AI"},
{"ANTARCTICA",      "AQ"},
{"ANTIGUA AND BARBUDA", "AG"},
{"ARGENTINA",       "AR"},
{"ARMENIA",     "AM"},
{"ARUBA",       "AW"},
{"ASCENSION ISLAND",    "AC"},
{"AUSTRALIA",       "AU"},
{"AUSTRIA",     "AT"},
{"AZERBAIJAN",      "AZ"},
{"BAHAMAS",     "BS"},
{"BAHRAIN",     "BH"},
{"BANGLADESH",      "BD"},
{"BARBADOS",        "BB"},
{"BELARUS",     "BY"},
{"BELGIUM",     "BE"},
{"BELIZE",      "BZ"},
{"BENIN",       "BJ"},
{"BERMUDA",     "BM"},
{"BHUTAN",      "BT"},
{"BOLIVIA",     "BO"},
{"BOSNIA AND HERZEGOVINA",      "BA"},
{"BOTSWANA",        "BW"},
{"BOUVET ISLAND",   "BV"},
{"BRAZIL",      "BR"},
{"BRITISH INDIAN OCEAN TERRITORY",      "IO"},
{"BRUNEI DARUSSALAM",   "BN"},
{"BULGARIA",        "BG"},
{"BURKINA FASO",    "BF"},
{"BURUNDI",     "BI"},
{"CAMBODIA",        "KH"},
{"CAMEROON",        "CM"},
{"CANADA",      "CA"},
{"CAPE VERDE",      "CV"},
{"CAYMAN ISLANDS",  "KY"},
{"CENTRAL AFRICAN REPUBLIC",        "CF"},
{"CHAD",        "TD"},
{"CHILE",       "CL"},
{"CHINA",       "CN"},
{"CHRISTMAS ISLAND",    "CX"},
{"CLIPPERTON ISLAND",   "CP"},
{"COCOS (KEELING) ISLANDS",     "CC"},
{"COLOMBIA",        "CO"},
{"COMOROS",     "KM"},
{"CONGO",       "CG"},
{"CONGO, THE DEMOCRATIC REPUBLIC OF THE",       "CD"},
{"COOK ISLANDS",    "CK"},
{"COSTA RICA",      "CR"},
{"COTE D'IVOIRE",   "CI"},
{"CROATIA",     "HR"},
{"CUBA",        "CU"},
{"CYPRUS",      "CY"},
{"CZECH REPUBLIC",  "CZ"},
{"DENMARK",     "DK"},
{"DJIBOUTI",        "DJ"},
{"DOMINICA",        "DM"},
{"DOMINICAN REPUBLIC",  "DO"},
{"ECUADOR",     "EC"},
{"EGYPT",       "EG"},
{"EL SALVADOR",     "SV"},
{"EQUATORIAL GUINEA",   "GQ"},
{"ERITREA",     "ER"},
{"ESTONIA",     "EE"},
{"ETHIOPIA",        "ET"},
{"FALKLAND ISLANDS (MALVINAS)",     "FK"},
{"FAROE ISLANDS",   "FO"},
{"FIJI",        "FJ"},
{"FINLAND",     "FI"},
{"FRANCE",      "FR"},
{"FRENCH GUIANA",   "GF"},
{"FRENCH POLYNESIA",    "PF"},
{"FRENCH SOUTHERN TERRITORIES",     "TF"},
{"GABON",       "GA"},
{"GAMBIA",      "GM"},
{"GEORGIA",     "GE"},
{"GERMANY",     "DE"},
{"GHANA",       "GH"},
{"GIBRALTAR",       "GI"},
{"GREECE",      "GR"},
{"GREENLAND",       "GL"},
{"GRENADA",     "GD"},
{"GUADELOUPE",      "GP"},
{"GUAM",        "GU"},
{"GUATEMALA",       "GT"},
{"GUERNSEY",        "GG"},
{"GUINEA",      "GN"},
{"GUINEA-BISSAU",   "GW"},
{"GUYANA",      "GY"},
{"HAITI",       "HT"},
{"HEARD ISLAND AND MCDONALD ISLANDS",       "HM"},
{"HOLY SEE (VATICAN CITY STATE)",       "VA"},
{"HONDURAS",        "HN"},
{"HONG KONG",       "HK"},
{"HUNGARY",     "HU"},
{"ICELAND",     "IS"},
{"INDIA",       "IN"},
{"INDONESIA",       "ID"},
{"IRAN, ISLAMIC REPUBLIC OF",       "IR"},
{"IRAQ",        "IQ"},
{"IRELAND",     "IE"},
{"ISRAEL",      "IL"},
{"ITALY",       "IT"},
{"JAMAICA",     "JM"},
{"JAPAN",       "JP"},
{"JERSEY",      "JE"},
{"JORDAN",      "JO"},
{"KAZAKHSTAN",      "KZ"},
{"KENYA",       "KE"},
{"KIRIBATI",        "KI"},
{"KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF",      "KP"},
{"KOREA, REPUBLIC OF",  "KR"},
{"KUWAIT",      "KW"},
{"KYRGYZSTAN",      "KG"},
{"LAO PEOPLE'S DEMOCRATIC REPUBLIC",        "LA"},
{"LATVIA",      "LV"},
{"LEBANON",     "LB"},
{"LESOTHO",     "LS"},
{"LIBERIA",     "LR"},
{"LIBYAN ARAB JAMAHIRIYA",      "LY"},
{"LIECHTENSTEIN",   "LI"},
{"LITHUANIA",       "LT"},
{"LUXEMBOURG",      "LU"},
{"MACAO",       "MO"},
{"MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF",      "MK"},
{"MADAGASCAR",      "MG"},
{"MALAWI",      "MW"},
{"MALAYSIA",        "MY"},
{"MALDIVES",        "MV"},
{"MALI",        "ML"},
{"MALTA",       "MT"},
{"MAN, ISLE OF",    "IM"},
{"MARSHALL ISLANDS",    "MH"},
{"MARTINIQUE",      "MQ"},
{"MAURITANIA",      "MR"},
{"MAURITIUS",       "MU"},
{"MAYOTTE",     "YT"},
{"MEXICO",      "MX"},
{"MICRONESIA, FEDERATED STATES OF",     "FM"},
{"MOLDOVA, REPUBLIC OF",        "MD"},
{"MONACO",      "MC"},
{"MONGOLIA",        "MN"},
{"MONTENEGRO",      "ME"},
{"MONTSERRAT",      "MS"},
{"MOROCCO",     "MA"},
{"MOZAMBIQUE",      "MZ"},
{"MYANMAR",     "MM"},
{"NAMIBIA",     "NA"},
{"NAURU",       "NR"},
{"NEPAL",       "NP"},
{"NETHERLANDS",     "NL"},
{"NETHERLANDS ANTILLES",        "AN"},
{"NEW CALEDONIA",   "NC"},
{"NEW ZEALAND",     "NZ"},
{"NICARAGUA",       "NI"},
{"NIGER",       "NE"},
{"NIGERIA",     "NG"},
{"NIUE",        "NU"},
{"NORFOLK ISLAND",      "NF"},
{"NORTHERN MARIANA ISLANDS",        "MP"},
{"NORWAY",      "NO"},
{"OMAN",        "OM"},
{"PAKISTAN",        "PK"},
{"PALAU",       "PW"},
{"PALESTINIAN TERRITORY, OCCUPIED",     "PS"},
{"PANAMA",      "PA"},
{"PAPUA NEW GUINEA",    "PG"},
{"PARAGUAY",        "PY"},
{"PERU",        "PE"},
{"PHILIPPINES",     "PH"},
{"PITCAIRN",        "PN"},
{"POLAND",      "PL"},
{"PORTUGAL",        "PT"},
{"PUERTO RICO",     "PR"},
{"QATAR",       "QA"},
{"REUNION",     "RE"},
{"ROMANIA",     "RO"},
{"RUSSIAN FEDERATION",  "RU"},
{"RWANDA",      "RW"},
{"SAINT HELENA",    "SH"},
{"SAINT KITTS AND NEVIS",       "KN"},
{"SAINT LUCIA",     "LC"},
{"SAINT PIERRE AND MIQUELON",       "PM"},
{"SAINT VINCENT AND THE GRENADINES",        "VC"},
{"SAMOA",       "WS"},
{"SAN MARINO",      "SM"},
{"SAO TOME AND PRINCIPE",       "ST"},
{"SAUDI ARABIA",    "SA"},
{"SENEGAL",     "SN"},
{"SERBIA",      "RS"},
{"SEYCHELLES",      "SC"},
{"SIERRA LEONE",    "SL"},
{"SINGAPORE",       "SG"},
{"SLOVAKIA",        "SK"},
{"SLOVENIA",        "SI"},
{"SOLOMON ISLANDS", "SB"},
{"SOMALIA",     "SO"},
{"SOUTH AFRICA",    "ZA"},
{"SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS",        "GS"},
{"SPAIN",       "ES"},
{"SRI LANKA",       "LK"},
{"SUDAN",       "SD"},
{"SURINAME",        "SR"},
{"SVALBARD AND JAN MAYEN",      "SJ"},
{"SWAZILAND",       "SZ"},
{"SWEDEN",      "SE"},
{"SWITZERLAND",     "CH"},
{"SYRIAN ARAB REPUBLIC",        "SY"},
{"TAIWAN, PROVINCE OF CHINA",       "TW"},
{"TAJIKISTAN",      "TJ"},
{"TANZANIA, UNITED REPUBLIC OF",        "TZ"},
{"THAILAND",        "TH"},
{"TIMOR-LESTE (EAST TIMOR)",        "TL"},
{"TOGO",        "TG"},
{"TOKELAU",     "TK"},
{"TONGA",       "TO"},
{"TRINIDAD AND TOBAGO", "TT"},
{"TRISTAN DA CUNHA",    "TA"},
{"TUNISIA",     "TN"},
{"TURKEY",      "TR"},
{"TURKMENISTAN",    "TM"},
{"TURKS AND CAICOS ISLANDS",        "TC"},
{"TUVALU",      "TV"},
{"UGANDA",      "UG"},
{"UKRAINE",     "UA"},
{"UNITED ARAB EMIRATES",        "AE"},
{"UNITED KINGDOM",  "GB"},
{"UNITED STATES",   "US"},
{"UNITED STATES MINOR OUTLYING ISLANDS",        "UM"},
{"URUGUAY",     "UY"},
{"UZBEKISTAN",      "UZ"},
{"VANUATU",     "VU"},
{"VENEZUELA",       "VE"},
{"VIET NAM",        "VN"},
{"VIRGIN ISLANDS, BRITISH",     "VG"},
{"VIRGIN ISLANDS, U.S.",        "VI"},
{"WALLIS AND FUTUNA",   "WF"},
{"WESTERN SAHARA",  "EH"},
{"YEMEN",       "YE"},
{"YUGOSLAVIA",      "YU"},
{"ZAMBIA",      "ZM"},
{"ZIMBABWE",        "ZW"},
{"RADAR CHANNELS",  "RDR"},
{"ALL CHANNELS",    "ALL"},
{NULL,          NULL}
};

void miniopt_init(miniopt_t *t, const char* name, const char* flags, bool longflags)
{
	static const char *null_flags = "";

	memset(t, 0, sizeof(miniopt_t));
	t->name = name;
	if (flags == NULL)
		t->flags = null_flags;
	else
		t->flags = flags;
	t->longflags = longflags;
}

int miniopt(miniopt_t *t, char **argv)
{
	int keylen;
	char *p, *eq, *valstr, *endptr = NULL;
	int err = 0;

	t->consumed = 0;
	t->positional = FALSE;
	memset(t->key, 0, MINIOPT_MAXKEY);
	t->opt = '\0';
	t->valstr = NULL;
	t->good_int = FALSE;
	valstr = NULL;

	if (*argv == NULL) {
		err = -1;
		goto exit;
	}

	p = *argv++;
	t->consumed++;

	if (!t->opt_end && !strcmp(p, "--")) {
		t->opt_end = TRUE;
		if (*argv == NULL) {
			err = -1;
			goto exit;
		}
		p = *argv++;
		t->consumed++;
	}

	if (t->opt_end) {
		t->positional = TRUE;
		valstr = p;
	}
	else if (!strncmp(p, "--", 2)) {
		eq = strchr(p, '=');
		if (eq == NULL && !t->longflags) {
			osl_ext_printf("%s: missing \" = \" in long param \"%s\"\n", t->name, p);
			err = 1;
			goto exit;
		}
		keylen = eq ? (eq - (p + 2)) : (int)strlen(p) - 2;
		if (keylen > 63) keylen = 63;
		memcpy(t->key, p + 2, keylen);

		if (eq) {
			valstr = eq + 1;
			if (*valstr == '\0') {
				osl_ext_printf("%s: missing value after \" = \" in long param \"%s\"\n",
				        t->name, p);
				err = 1;
				goto exit;
			}
		}
	}
	else if (!strncmp(p, "-", 1)) {
		t->opt = p[1];
		if (strlen(p) > 2) {
			osl_ext_printf("%s: only single char options, error on param \"%s\"\n",
				t->name, p);
			err = 1;
			goto exit;
		}
		if (strchr(t->flags, t->opt)) {
			/* this is a flag option, no value expected */
			valstr = NULL;
		} else {
			if (*argv == NULL) {
				osl_ext_printf(
				"%s: missing value parameter after \"%s\"\n", t->name, p);
				err = 1;
				goto exit;
			}
			valstr = *argv;
			argv++;
			t->consumed++;
		}
	} else {
		t->positional = TRUE;
		valstr = p;
	}

	/* parse valstr as int just in case */
	if (valstr) {
		t->uval = (uint)strtoul(valstr, &endptr, 0);
		t->val = (int)t->uval;
		t->good_int = (*endptr == '\0');
	}

	t->valstr = valstr;

exit:
	if (err == 1)
		t->opt = '?';

	return err;
}

static void * irh;
static WLM_BAND curBand = WLM_BAND_AUTO;
extern int wlu_get(void *wl, int cmd, void *buf, int len);
extern int wlu_set(void *wl, int cmd, void *buf, int len);
extern int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
extern int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
extern int wlu_iovar_setint(void *wl, const char *iovar, int val);


void * wlmLastError(void)
{
	static const char *bcmerrorstrtable[] = BCMERRSTRINGTABLE;
	static char errorString[256];
	int bcmerror;

	if (wlu_iovar_getint(irh, "bcmerror", &bcmerror))
		return "Failed to retrieve error";

	if (bcmerror > 0 || bcmerror < BCME_LAST)
		return "Undefined error";

	sprintf(errorString, "%s (%d)", bcmerrorstrtable[-bcmerror], bcmerror);

	return errorString;
}

int wlmRadioOn(void)
{
	int val;

	/* val = WL_RADIO_SW_DISABLE << 16; */
	val = (1<<0) << 16;

	if (wlu_set(irh, WLC_SET_RADIO, &val, sizeof(int)) < 0) {
		osl_ext_printf("wlmRadioOn: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRadioOff(void)
{
	int val;

	/* val = WL_RADIO_SW_DISABLE << 16 | WL_RADIO_SW_DISABLE; */
	val = (1<<0) << 16 | (1<<0);

	if (wlu_set(irh, WLC_SET_RADIO, &val, sizeof(int)) < 0) {
		 osl_ext_printf("wlmRadioOff: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamingOn(void)
{
	if (wlu_iovar_setint(irh, "roam_off", 0) < 0) {
		osl_ext_printf("wlmRoamingOn: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamingOff(void)
{
	if (wlu_iovar_setint(irh, "roam_off", 1) < 0) {
		osl_ext_printf("wlmRoamingOff: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamTriggerLevelGet(int *val, WLM_BAND band)
{
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = -1;

	if (wlu_get(irh, WLC_GET_ROAM_TRIGGER, &x, sizeof(x)) < 0) {
		osl_ext_printf("wlmRoamTriggerLevelGet: %s\n", wlmLastError());
		return FALSE;
	}

	*val = htod32(x.val);

	return TRUE;
}

int wlmRoamTriggerLevelSet(int val, WLM_BAND band)
{
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = htod32(val);

	if (wlu_set(irh, WLC_SET_ROAM_TRIGGER, &x, sizeof(x)) < 0) {
		osl_ext_printf("wlmRoamTriggerLevelSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmCountryCodeSet(const char * country_name)
{
	wl_country_t cspec;
	int err;

	memset(&cspec, 0, sizeof(cspec));
	cspec.rev = -1;

	/* arg matched a country name */
	memcpy(cspec.country_abbrev, country_name, WLC_CNTRY_BUF_SZ);
	err = 0;

	/* first try the country iovar */
	if (cspec.rev == -1 && cspec.ccode[0] == '\0')
		err = wlu_iovar_set(irh, "country", &cspec, WLC_CNTRY_BUF_SZ);
	else
		err = wlu_iovar_set(irh, "country", &cspec, sizeof(cspec));

	if (err == 0)
		return TRUE;
	return FALSE;
}

int wlmMinPowerConsumption(int enable)
{
	if (wlu_iovar_setint(irh, "mpc", enable)) {
		osl_ext_printf("wlmMinPowerConsumption: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}
int wlmEnableAdapterUp(int enable)
{
	/*  Enable/disable adapter  */
	if (enable)
	{
		if (wlu_set(irh, WLC_UP, NULL, 0)) {
			osl_ext_printf("wlmEnableAdapterUp: %s\n", wlmLastError());
			return FALSE;
		}
	}
	else {
		if (wlu_set(irh, WLC_DOWN, NULL, 0)) {
			osl_ext_printf("wlmEnableAdapterUp: %s\n", wlmLastError());
			return FALSE;
		}
	}

	return TRUE;
}
int wlmBssidGet(char *bssid, int length)
{
	struct ether_addr ea;
	int i; 
	if (length != ETHER_ADDR_LEN) {
		osl_ext_printf("wlmBssiGet: bssid requires %d bytes", ETHER_ADDR_LEN);
		return FALSE;
	}

	if (wlu_get(irh, WLC_GET_BSSID, (void*)&ea, ETHER_ADDR_LEN) == 0) {
		/* associated - format and return bssid */
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			*(bssid+i) = ea.octet[i] & 0xff;
		}
		//strncpy(bssid, wl_ether_etoa(&ea), length);
	}
	else {
		/* not associated - return empty string */
		memset(bssid, 0, length);
	}

	return TRUE;
}

int wlmBssidSet(char *bssid)
{
	if (wlu_set(irh, WLC_SET_BSSID, (void*)bssid, ETHER_ADDR_LEN) < 0) {
		osl_ext_printf("wlmBssidSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSecuritySet(WLM_AUTH_TYPE authType, WLM_AUTH_MODE authMode,
	WLM_ENCRYPTION encryption, const char *key, int keyid)
{
	int length = 0;
	int wpa_auth;
	int sup_wpa;
	int primary_key;
	wl_wsec_key_t wepKey[4];
	wsec_pmk_t psk;
	int wsec;

	if (encryption != WLM_ENCRYPT_NONE && key == 0) {
		osl_ext_printf("wlmSecuritySet: invalid key\n");
		return FALSE;
	}

	if (key) {
		length = strlen(key);
	}

	switch (encryption) {

	case WLM_ENCRYPT_NONE:
		wpa_auth = WPA_AUTH_DISABLED;
		sup_wpa = 0;
		break;

	case WLM_ENCRYPT_WEP: {
		int i;
		int len = length;

		length *= 4;
		wpa_auth = WPA_AUTH_DISABLED;
		sup_wpa = 0;

		if (!(length == 40 || length == 104 || length == 128 || length == 256)) {
			osl_ext_printf("wlmSecuritySet: invalid WEP key length %d"
			"       - expect 40, 104, 128, or 256"
			" (i.e. 10, 26, 32, or 64 for each of 4 keys)\n", length);
			return FALSE;
		}

		/* convert hex key string to 4 binary keys */
		for (i = 0; i < 4; i++) {
			wl_wsec_key_t *k = &wepKey[i];
			const char *data = &key[0];
			unsigned int j;

			memset(k, 0, sizeof(*k));
			k->index = i;
			k->len = len / 2;

			for (j = 0; j < k->len; j++) {
				char hex[] = "XX";
				char *end = NULL;
				strncpy(hex, &data[j * 2], 2);
				k->data[j] = (char)strtoul(hex, &end, 16);
				if (*end != 0) {
					osl_ext_printf("wlmSecuritySet: invalid WEP key"
					"       - expect hex values\n");
					return FALSE;
				}
			}

			switch (k->len) {
			case 5:
				k->algo = CRYPTO_ALGO_WEP1;
				break;
			case 13:
				k->algo = CRYPTO_ALGO_WEP128;
				break;
			case 16:
				k->algo = CRYPTO_ALGO_AES_CCM;
				break;
			case 32:
				k->algo = CRYPTO_ALGO_TKIP;
				break;
			default:
				/* invalid */
				return FALSE;
			}

			k->flags |= WL_PRIMARY_KEY;
		}

		break;
	}


	case WLM_ENCRYPT_TKIP:
	case WLM_ENCRYPT_AES: {

		if (authMode != WLM_WPA_AUTH_PSK && authMode != WLM_WPA2_AUTH_PSK) {
			osl_ext_printf("wlmSecuritySet: authentication mode must be WPA PSK or WPA2 PSK\n");
			return FALSE;
		}

		wpa_auth = authMode;
		sup_wpa = 1;

		if (length < WSEC_MIN_PSK_LEN || length > WSEC_MAX_PSK_LEN) {
			osl_ext_printf("wlmSecuritySet: passphrase must be between %d and %d characters\n",
			WSEC_MIN_PSK_LEN, WSEC_MAX_PSK_LEN);
			return FALSE;
		}

		psk.key_len = length;
		psk.flags = WSEC_PASSPHRASE;
		memcpy(psk.key, key, length);

		break;
	}

	case WLM_ENCRYPT_WSEC:
	case WLM_ENCRYPT_FIPS:
	default:
		osl_ext_printf("wlmSecuritySet: encryption not supported\n");
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "auth", authType)) {
		osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "wpa_auth", wpa_auth)) {
		osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "sup_wpa", sup_wpa)) {
		osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	if (encryption == WLM_ENCRYPT_WEP) {
		int i;
		for (i = 0; i < 4; i++) {
			wl_wsec_key_t *k = &wepKey[i];
			k->index = htod32(k->index);
			k->len = htod32(k->len);
			k->algo = htod32(k->algo);
			k->flags = htod32(k->flags);

			if (wlu_set(irh, WLC_SET_KEY, k, sizeof(*k))) {
				osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
				return FALSE;
			}
		}

		primary_key = htod32(keyid);
		if (wlu_set(irh, WLC_SET_KEY_PRIMARY, &primary_key, sizeof(primary_key)) < 0) {
			osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
			return FALSE;
		}
	}
	else if (encryption == WLM_ENCRYPT_TKIP || encryption == WLM_ENCRYPT_AES) {
		psk.key_len = htod16(psk.key_len);
		psk.flags = htod16(psk.flags);

		if (wlu_set(irh, WLC_SET_WSEC_PMK, &psk, sizeof(psk))) {
			osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
			return FALSE;
		}
	}

	wsec = htod32(encryption);
	if (wlu_set(irh, WLC_SET_WSEC, &wsec, sizeof(wsec)) < 0) {
		osl_ext_printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmAPSet()
{
	char *argv[5] = {"ap", "1", NULL, NULL, NULL};
	cmd_t cmd = { "ap", wl_int, WLC_GET_AP, WLC_SET_AP, "Set AP mode: 0 (STA) or 1 (AP)" };
	
	if(wl_int(irh, &cmd, argv)) {
		osl_ext_printf("wlmAPSet failed\n");
		return FALSE;
	}
	
	return TRUE;
}

int wlmSTASet()
{
	char *argv[5] = {"ap", "0", NULL, NULL, NULL};
	cmd_t cmd = { "ap", wl_int, WLC_GET_AP, WLC_SET_AP, "Set AP mode: 0 (STA) or 1 (AP)" };
	
	if(wl_int(irh, &cmd, argv)) {
		osl_ext_printf("wlmAPSet failed\n");
		return FALSE;
	}
	
	return TRUE;
}

int wlmClosedSet(int val)
{
	char *argv[5] = {"closed", "0", NULL, NULL, NULL};
	cmd_t cmd = { "closed", wl_int, WLC_GET_CLOSED, WLC_SET_CLOSED,
				"hides the network from active scans, 0 or 1.\n"
				"\t0 is open, 1 is hide" };

	sprintf(argv[1], "%d", val);
	
	if(wl_int(irh, &cmd, argv)) {
		osl_ext_printf("wlmAPSet failed\n");
		return FALSE;
	}
	
	return TRUE;
}

int wlmChannelSet(int channel)
{
	/* Check band lock first before set  channel */
	if ((channel <= 14) && (curBand != WLM_BAND_2G)) {
		curBand = WLM_BAND_2G;
	} else if ((channel > 14) && (curBand != WLM_BAND_5G)) {
		curBand = WLM_BAND_5G;
	}

	/* Set 'channel' */
	channel = htod32(channel);
	if (wlu_set(irh, WLC_SET_CHANNEL, &channel, sizeof(channel))) {
		osl_ext_printf("wlmChannelSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSsidGet(char *ssid, int length)
{
	wlc_ssid_t wlc_ssid;

	if (length < SSID_FMT_BUF_LEN) {
		osl_ext_printf("wlmSsidGet: Ssid buffer too short - %d bytes at least\n",
		SSID_FMT_BUF_LEN);
		return FALSE;
	}

	/* query for 'ssid' */
	if (wlu_get(irh, WLC_GET_SSID, &wlc_ssid, sizeof(wlc_ssid_t))) {
		osl_ext_printf("wlmSsidGet: %s\n", wlmLastError());
		return FALSE;
	}

	wl_format_ssid(ssid, wlc_ssid.SSID, dtoh32(wlc_ssid.SSID_len));

	return TRUE;
}

int wlmSsidGetEx(char *ssid, int *length)
{
	//char ssid_buf[SSID_FMT_BUF_LEN] = {0x00};
	wlc_ssid_t wlc_ssid;

	/* query for 'ssid' */
	if (wlu_get(irh, WLC_GET_SSID, &wlc_ssid, sizeof(wlc_ssid_t))) {
		osl_ext_printf("wlmSsidGet: %s\n", wlmLastError());
		return FALSE;
	}

	//wl_format_ssid(ssid, wlc_ssid.SSID, dtoh32(wlc_ssid.SSID_len));
	memcpy((void*)ssid, wlc_ssid.SSID, 32);
	*length = wlc_ssid.SSID_len;

	return TRUE;
}

int wlmSsidSet(char *ssid)
{
	wlc_ssid_t wlc_ssid;

	if(strlen(ssid) == 0 || strlen(ssid) > 32) {
		osl_ext_printf("wlmSsidSet: ssid %s is incorrect\n", ssid);
		return FALSE;
	}

	memset(&wlc_ssid, 0x00, sizeof(wlc_ssid_t));
	memcpy(wlc_ssid.SSID, ssid, strlen(ssid));
	wlc_ssid.SSID_len = strlen(ssid);

	/* set 'ssid' */
	if (wlu_set(irh, WLC_SET_SSID, &wlc_ssid, sizeof(wlc_ssid_t))) {
		osl_ext_printf("wlmSsidSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmDisassociateNetwork(void)
{
	if (wlu_set(irh, WLC_DISASSOC, NULL, 0) < 0) {
		osl_ext_printf("wlmDisassociateNetwork: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmJoinNetwork(const char* ssid, WLM_JOIN_MODE mode)
{
	wlc_ssid_t wlcSsid;
	int infra = htod32(mode);

	if (wlu_set(irh, WLC_SET_INFRA, &infra, sizeof(int)) < 0) {
	    osl_ext_printf("wlmJoinNetwork: %s\n", wlmLastError());
	    return FALSE;
	}

	wlcSsid.SSID_len = htod32(strlen(ssid));
	memcpy(wlcSsid.SSID, ssid, wlcSsid.SSID_len);

	if (wlu_set(irh, WLC_SET_SSID, &wlcSsid, sizeof(wlc_ssid_t)) < 0) {
		osl_ext_printf("wlmJoinNetwork: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

