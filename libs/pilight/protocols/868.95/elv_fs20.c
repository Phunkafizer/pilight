#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "elv_fs20.h"

//#define LEARN_REPEATS			40
#define NORMAL_REPEATS 3
//#define PULSE_MULTIPLIER	4
//#define MIN_PULSE_LENGTH	250
//#define MAX_PULSE_LENGTH	320
//#define AVG_PULSE_LENGTH	315

#define PAUSE_LENGTH 10000
#define SHORT_PULSE_LENGTH	400
#define LONG_PULSE_LENGTH	600
#define MIN_RAW_LENGTH 118 //w/o extension byte
#define MAX_RAW_LENGTH 136 //w/ extension byte
#define MAX_GAP_LENGTH 200000
#define MIN_GAP_LENGTH 8000


static int raw_pos = 0;
static uint8_t checksum = 0;

static void addBit(uint8_t b) {
	int l = (b != 0) ? LONG_PULSE_LENGTH : SHORT_PULSE_LENGTH;
	
	elv_fs20->raw[raw_pos++] = l;
	elv_fs20->raw[raw_pos++] = l;
}

static void addSync() {
	int i=0;
	for(i=0; i<12; i++) {
		addBit(0);
	}
	addBit(1);
}

static void addByte(uint8_t b) {
	int par = 0;
	uint8_t mask = 0x80;
	while (mask) {
		uint8_t bit = (b & mask);
		addBit(bit);
		if (bit) {
			par ^= 1;
		}
		mask >>= 1;
	}
	addBit(par); //add parity bit
	checksum += b;
}

static int createCode(struct JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	int dimlevel = -1;
	int learn = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0) {
		//convert pseudoquadruple id 
		uint32_t q = (uint32_t)round(itmp);
		uint8_t p=0;
		id = 0;
		while (q) {
			id |= ((q - 1) % 4) << p;
			q /= 10;
			p += 2;
		}
	}
	
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "dimlevel", &itmp) == 0)
		dimlevel = (int)round(itmp);

	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if((id == -1) || (dimlevel == -1 && state == -1)) {
		logprintf(LOG_ERR, "elv_fs20: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 0xFFFF || id < 0) {
		logprintf(LOG_ERR, "elv_fs20: invalid id range");
		return EXIT_FAILURE;
//	} else if((unit > 15 || unit < 0) && all == 0) {
//		logprintf(LOG_ERR, "arctech_dimmer: invalid unit range");
//		return EXIT_FAILURE;
//	} else if(dimlevel != -1 && (dimlevel > max || dimlevel < min) ) {
//		logprintf(LOG_ERR, "arctech_dimmer: invalid dimlevel range");
//		return EXIT_FAILURE;
//	} else if(dimlevel >= 0 && state == 0) {
//		logprintf(LOG_ERR, "arctech_dimmer: dimlevel and off state cannot be combined");
//		return EXIT_FAILURE;
	} else {
		//if(unit == -1 && all == 1) {
		//	unit = 0;
		//}
		//if(dimlevel >= 0) {
		//	state = -1;
		//}
		raw_pos = 0;
		checksum = 6;
		addSync();
		addByte(id >> 8);
		addByte(id & 0xFF);
		addByte(unit);
		
		uint8_t cmd;
		if (state > -1) {
			if (state) {
				cmd = 0x10; //switch on
			}
			else {
				cmd = 0x00; //switch off
			}
		}
		else {
			cmd = dimlevel;
		}
		
		addByte(checksum);
		
		addBit(0); //EOT
		elv_fs20->raw[raw_pos - 1] += PAUSE_LENGTH;
		
		//createMessage(id, unit, state, all, dimlevel, learn);
		
		elv_fs20->rawlen = raw_pos;
	}
	return EXIT_SUCCESS;
}



#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void arctechSwitchInit(void) {
	protocol_register(&elv_fs20);
	protocol_set_id(elv_fs20, "elv_fs20");
	elv_fs20->devtype = SWITCH;
	elv_fs20->hwtype = RF868;
	elv_fs20->txrpt = NORMAL_REPEATS;
	elv_fs20->minrawlen = MIN_RAW_LENGTH;
	elv_fs20->maxrawlen = MAX_RAW_LENGTH;
	elv_fs20->maxgaplen = MAX_GAP_LENGTH;
	elv_fs20->mingaplen = MIN_GAP_LENGTH;

	options_add(&elv_fs20->options, 'h', "housecode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-4]{1,8}$");
	options_add(&elv_fs20->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elv_fs20->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elv_fs20->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	
	//options_add(&elv_fs20->options, 'a', "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	//options_add(&elv_fs20->options, 'l', "learn", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	elv_fs20->createCode=&createCode;
	//elv_fs20->parseCode=&parseCode;
	
	//elv_fs20->printHelp=&printHelp;
	//elv_fs20->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ELV FS20";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "";
}

void init(void) {
	elvfs20Init();
}
#endif
