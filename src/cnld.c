/*
 *  Code 'n Load daemon main source code file.
 *  Copyright (C) 2018  Pablo Ridolfi - Code 'n Load
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*==================[inclusions]=============================================*/

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <syslog.h>
#include <linux/limits.h>
#include <pthread.h>

#include "command.h"

/*==================[macros and definitions]=================================*/

#define AUTOSSH_CHECK_TIME 60

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

static pthread_t autossh_tid;

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

static void * check_autossh_thread(void * a)
{
	char rsp[PATH_MAX];
	char ssid[128];
	char new_ssid[128];
	pthread_detach(pthread_self());
	cnl_run("iwgetid", ssid, 128);
	syslog(LOG_INFO, "[check_autossh] current ssid: %s", ssid);
	while (1) {
		cnl_run("systemctl status cnl_ssh", rsp, PATH_MAX);
		if (strstr(rsp, "Warning: remote port forwarding failed for listen port")) {
			syslog(LOG_ALERT, "[check_autossh] check failed, restarting cnl_ssh");
			system("systemctl restart cnl_ssh");
		}
		cnl_run("iwgetid", new_ssid, 128);
		if (strcmp(ssid, new_ssid) != 0) {
			syslog(LOG_ALERT, "[check_autossh] wifi ssid changed, restarting cnl_ssh");
			syslog(LOG_INFO, "[check_autossh] new ssid: %s", new_ssid);
			system("systemctl restart cnl_ssh");
			strcpy(ssid, new_ssid);
		}
		sleep(AUTOSSH_CHECK_TIME);
	}
	return NULL;
}

/*==================[external functions definition]==========================*/

int main(void)
{
	char buf[100];
	int fd,cl,rc;

	int fds = sd_listen_fds(0);
	syslog(LOG_INFO,"sd_listen_fds: %d\n", fds);

	fd = SD_LISTEN_FDS_START;

	if (listen(fd, 5) == -1) {
		syslog(LOG_ERR,"listen error");
		exit(-1);
	}

	if (strtol(getenv("CNL_APP_MONITOR_TIME"), NULL, 10) != 0) {
		syslog(LOG_INFO, "enabling monitor");
		cnl_command("monitor on", fileno(stdout));
	}

	pthread_create(&autossh_tid, NULL, check_autossh_thread, NULL);

	while (1) {
		if ( (cl = accept(fd, NULL, NULL)) == -1) {
			syslog(LOG_ERR,"accept error");
			continue;
		}
		rc=read(cl,buf,sizeof(buf));
		if (rc > 0) {
			syslog(LOG_INFO,"read %u bytes: %.*s\n", rc, rc, buf);
			buf[rc] = 0;
			cnl_command(buf, cl);
		}
		if (rc == -1) {
			syslog(LOG_ERR,"read error");
		}
		close(cl);
	}
	return 0;
}

/*==================[end of file]============================================*/
