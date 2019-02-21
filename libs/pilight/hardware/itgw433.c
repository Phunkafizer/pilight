/*
	Copyright (C) 2019 CurlyMo & S. Seegel

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
#include <unistd.h>
#include <sys/time.h>
#include <wiringx.h>

#include "../core/log.h"

#ifdef PILIGHT_REWRITE
#include "hardware.h"
#else
#include "../config/hardware.h"
#include "../config/settings.h"
#endif
#include "itgw433.h"

#define SEARCH_REPLY "HCGW:VC:pilight;MC:RaspyRFM;FW:1.00;IP:192.168.178.1;;"

static uv_loop_t *uv_loop;
static uv_udp_t server;

static void send_cb(uv_udp_send_t *req, int status) {
	FREE(req);
}

static void *reason_send_code_free(void *param) {
	struct reason_send_code_t *data = param;
	FREE(data);
	return NULL;
}

static void on_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* rcvbuf, const struct sockaddr* addr, unsigned flags) {
    if (nread > 0) {
		if (strncmp(rcvbuf->base, "SEARCH HCGW", 11) == 0)  {
			uv_udp_send_t *send_req = MALLOC(sizeof(uv_udp_send_t));
			uv_buf_t sndbuf = uv_buf_init(SEARCH_REPLY, sizeof(SEARCH_REPLY));
			uv_udp_send(send_req, handle, (const struct uv_buf_t *) &sndbuf, 1, addr, send_cb);
		}
		else if (strncmp(rcvbuf->base, "TXP:", 4) == 0) {
			char *ptr = strtok(rcvbuf->base, ",;");
			while(ptr != NULL) {
				if ((ptr = strtok(NULL, ",;")) == NULL)
					break;
				
				if ((ptr = strtok(NULL, ",;")) == NULL)
					break;
				int repeats = atoi(ptr);

				if ((ptr = strtok(NULL, ",;")) == NULL)
					break;
				int pause = atoi(ptr);

				if ((ptr = strtok(NULL, ",;")) == NULL)
					break;
				int timebase = atoi(ptr);

				if ((ptr = strtok(NULL, ",;")) == NULL)
					break;
				int numpulses = atoi(ptr) * 2;

				struct reason_send_code_t *data = MALLOC(sizeof(struct reason_send_code_t));
				int i=0;
				for(i=0; i<numpulses; i++) {
					if ((ptr = strtok(NULL, ",;")) == NULL)
						break;
					data->pulses[i] = atoi(ptr) * timebase;
				}
				if (i == numpulses) {
					data->pulses[i-1] += pause;
                    data->origin = 1;
                    data->rawlen = numpulses;
                    data->txrpt = repeats;
                    strncpy(data->protocol, "dummy", 255);
                    data->hwtype = RF433;
                    memset(data->uuid, 0, UUID_LENGTH+1);
                    eventpool_trigger(REASON_SEND_CODE, reason_send_code_free, data);
				}
				else
				{
					FREE(data);
				}
				
				break;
			}

			
		}
    }
	char *buf = (char *) rcvbuf->base;
    FREE(buf);
}
 
static void on_alloc(uv_handle_t* client, size_t suggested_size, uv_buf_t* buf) {
    buf->base = MALLOC(suggested_size);
    buf->len = suggested_size;
}

static unsigned short int itgw433HwInit(void) {
    int status;
    struct sockaddr_in addr;
    uv_loop = uv_default_loop();
    
    status = uv_udp_init(uv_loop,&server);
    if (status != 0) {
        logprintf(LOG_ERR, "unable to init udp");
		return EXIT_FAILURE;
    }
    uv_ip4_addr("0.0.0.0", 49880, &addr);
    status = uv_udp_bind(&server, (const struct sockaddr*)&addr,0);
    if (status != 0) {
        logprintf(LOG_ERR, "unable to bind udp");
		return EXIT_FAILURE;
    }
    status = uv_udp_recv_start(&server, on_alloc, on_recv);
    if (status != 0) {
        logprintf(LOG_ERR, "unable to receive udp");
		return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void itgw433Init(void) {
    hardware_register(&itgw433);
	hardware_set_id(itgw433, "itgw433");

	options_add(&itgw433->options, "m", "mode", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "^server$");
	
	itgw433->minrawlen = 1000;
	itgw433->maxrawlen = 0;
	itgw433->mingaplen = 5100;
	itgw433->maxgaplen = 10000;
	itgw433->init = &itgw433HwInit;
	itgw433->comtype = COMNONE;
	itgw433->hwtype = HWINTERNAL;

	char *mode;
	if(config_setting_get_string("mode", 0, &mode) < 0) {
		logprintf(LOG_ERR, "no mode configured");
		return;
	}

	printf("mode is: %s\n", mode);

    if(strcmp(mode, "client") == 0) {
        itgw433->comtype = COMPLSTRAIN;
	    itgw433->hwtype = RF433;
    }

	//itgw433->settings=&raspyrfmSettings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "itgw433";
	module->version = "1.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	itgw433Init();
}
#endif
