/*
 * common.c - Common utilities
 * Copyright (C) 2003,2006, 2007 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <pthread.h>

#include "common.h"

void set_speaking_thread_parameters(void)
{
	int ret;
	sigset_t all_signals;

	ret = sigfillset(&all_signals);
	if (ret == 0) {
		ret = pthread_sigmask(SIG_BLOCK, &all_signals, NULL);
		if (ret != 0)
			MSG(1, "Can't set signal set, expect problems when terminating!\n");
	} else {
		MSG(1, "Can't fill signal set, expect problems when terminating!\n");
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}
