/*
 * Balloon
 *
 * Copyright IBM, Corp. 2008
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#ifndef _QEMU_BALLOON_H
#define _QEMU_BALLOON_H

#include "monitor.h"

typedef void (QEMUBalloonEvent)(void *opaque, ram_addr_t target,
                                MonitorCompletion cb, void *cb_data);

void qemu_add_balloon_handler(QEMUBalloonEvent *func, void *opaque);

int qemu_balloon(ram_addr_t target, MonitorCompletion cb, void *opaque);

int qemu_balloon_status(MonitorCompletion cb, void *opaque);

void monitor_print_balloon(Monitor *mon, const QObject *data);
int do_info_balloon(Monitor *mon, MonitorCompletion cb, void *opaque);
int do_balloon(Monitor *mon, const QDict *params,
               MonitorCompletion cb, void *opaque);

#endif
