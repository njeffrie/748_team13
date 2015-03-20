/******************************************************************************
*  Nano-RK, a real-time operating system for sensor networks.
*  Copyright (C) 2007, Real-Time and Multimedia Lab, Carnegie Mellon University
*  All rights reserved.
*
*  This is the Open Source Version of Nano-RK included as part of a Dual
*  Licensing Model. If you are unsure which license to use please refer to:
*  http://www.nanork.org/nano-RK/wiki/Licensing
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, version 2.0 of the License.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include <nrk.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <hal.h>
#include <flash.h>
#include <nrk_error.h>
#include <avr/eeprom.h>
#include <nrk_eeprom.h>
#include <string.h>

int main()
{
	nrk_init();
	flash_init(13);
	flash_task_config();
	flash_enable();
	printf("flash error count after init = %d\n", (int)flash_err_count_get());
	printf("currnent packet size = %d\n", (int)flash_msg_len_get());
	flash_disable();
	flash_enable();
	flash_disable();
	char *buf = "this is a buffer";
	flash_tx_pkt(buf, strlen(buf));
	printf("success...maybe!\n");
	return 1; 
}
