/*
	Copyright (C) 2018 Phunkafizer & CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

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
#include "voltcraft.h"

#define MIN_PULSE_LENGTH	670
#define MAX_PULSE_LENGTH	690
#define AVG_PULSE_LENGTH	680
#define RAW_LENGTH			42

static int validate(void) {
	if(voltcraft->rawlen == RAW_LENGTH) {
		//if(voltcraft->raw[voltcraft->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		//  voltcraft->raw[voltcraft->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
		//	return 0;
		//}
	}

	return -1;
}

static void createMessage(int id, int unit, int state) {
	voltcraft->message = json_mkobject();
	json_append_member(voltcraft->message, "id", json_mknumber(id, 0));
	json_append_member(voltcraft->message, "unit", json_mknumber(unit, 0));
	if(state == 0) {
		json_append_member(voltcraft->message, "state", json_mkstring("on"));
	} else {
		json_append_member(voltcraft->message, "state", json_mkstring("off"));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/4], x = 0, i = 0;

	if(voltcraft->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "pollin: parsecode - invalid parameter passed %d", voltcraft->rawlen);
		return;
	}

	for(x=0;x<voltcraft->rawlen-2;x+=4) {
		/*if(voltcraft->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}*/
	}

	int systemcode = binToDec(binary, 0, 4);
	int unitcode = binToDec(binary, 5, 9);
	int state = binary[11];
	createMessage(systemcode, unitcode, state);
}

static void createRawCode(int id, int unit, int state) {
	int binbuf[20];
	int *praw;
	int i;
	
	memset(binbuf, 0, sizeof(binbuf));
	decToBinRev(id, &binbuf[0]);
	decToBinRev(unit - 1, &binbuf[12]);
	//rawbuf[14] = 0 //only for dimm/all-on
	binbuf[15] = state;
	//rawbuf[16] = 0; //only for bright/dimm
	//rawbuf[17] = 0; //always zero
	binbuf[18] = binbuf[12] ^ binbuf[14] ^ binbuf[16]; //checksum 1
	binbuf[19] = binbuf[13] ^ binbuf[15] ^ binbuf[17]; //checksum 2
	
	praw = voltcraft->raw;
	*praw++ = AVG_PULSE_LENGTH; //start bit
	for (i=0; i<sizeof(binbuf)/sizeof(binbuf[0]); i++) {
		if (binbuf[i] != 0) {
			*praw++ = 2 * AVG_PULSE_LENGTH;
			*praw++ = AVG_PULSE_LENGTH;			
		} else {
			*praw++ = AVG_PULSE_LENGTH;		
			*praw++ = 2 * AVG_PULSE_LENGTH;
		}
		
		printf("i %d: in %d -> %d-%d\n", i, binbuf[i], voltcraft->raw[i * 2], voltcraft->raw[i * 2 + 1]);
	}
	*praw++ = 119L * AVG_PULSE_LENGTH;
	
	
	
	printf("pulses: %d\n", praw - voltcraft->raw);
	
	voltcraft->rawlen = RAW_LENGTH;
}

static int createCode(struct JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int i;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "voltcraft: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "voltcraft: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 4 || unit < 1) {
		logprintf(LOG_ERR, "voltcraft: invalid unit range");
		return EXIT_FAILURE;
	} else {
		createMessage(id, unit, state);
		
		createRawCode(id, unit, state);
		
		for (i=0; i<voltcraft->rawlen; i++)
			printf("%d ", voltcraft->raw[i]);
	}
	
	printf("ID: %d\n", id);
	
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void voltcraftInit(void) {

	protocol_register(&voltcraft);
	protocol_set_id(voltcraft, "voltcraft");
	protocol_device_add(voltcraft, "voltcraft", "Voltcraft RC30 Switches");
	voltcraft->devtype = SWITCH;
	voltcraft->hwtype = RF433;
	voltcraft->minrawlen = RAW_LENGTH;
	voltcraft->maxrawlen = RAW_LENGTH;
	voltcraft->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	voltcraft->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&voltcraft->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&voltcraft->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&voltcraft->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-4]{1})$");
	options_add(&voltcraft->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,3}|[0-3][0-9]{3}|40([0-8][0-9]|9[0-5]))$");

	voltcraft->parseCode=&parseCode;
	voltcraft->createCode=&createCode;
	voltcraft->printHelp=&printHelp;
	voltcraft->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "voltcraft";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	voltcraftInit();
}
#endif
