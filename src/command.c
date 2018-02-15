/*
 *  Code 'n Load daemon commands source code file.
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

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>

#include "command.h"

/*==================[macros and definitions]=================================*/

#define DEFAULT_MONITOR_TIME 60

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

static int cnl_update_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_remove_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_clean_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_status_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_journal_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_serial_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_pubkey_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_clone_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_pull_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_monitor_on_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_monitor_off_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_config_set_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_config_get_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_start_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_stop_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_update_cnld_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_coretemp_cb(const char * cmd, char * rsp, size_t rsp_len);
static int cnl_blinkme_cb(const char * cmd, char * rsp, size_t rsp_len);

/*==================[internal data definition]===============================*/

static pthread_t monitor_tid;
static int monitor_dowork;

/*==================[external data definition]===============================*/

static cnl_cmd_t commands[] = {
		{"update", cnl_update_cb},
		{"remove", cnl_remove_cb},
		{"clean", cnl_clean_cb},
		{"status", cnl_status_cb},
		{"journal", cnl_journal_cb},
		{"serial", cnl_serial_cb},
		{"pubkey", cnl_pubkey_cb},
		{"clone", cnl_clone_cb},
		{"pull", cnl_pull_cb},
		{"monitor on", cnl_monitor_on_cb},
		{"monitor off", cnl_monitor_off_cb},
		{"config get", cnl_config_get_cb},
		{"config set", cnl_config_set_cb},
		{"stop", cnl_stop_cb},
		{"start", cnl_start_cb},
		{"cnldupd", cnl_update_cnld_cb},
		{"coretemp", cnl_coretemp_cb},
		{"blinkme", cnl_blinkme_cb},
		/* always end with NULL */
		{NULL, NULL}
};

/*==================[internal functions definition]==========================*/

static void replace_nonprintable_chars(char * str)
{
	while(*str) {
		if (*str < 32) {
			*str = ' ';
		}
		str++;
	}
}

static size_t trimwhitespace(char *out, size_t len, const char *str)
{
	if ((len == 0 ) || (out == NULL)) {
		return 0;
	}

	const char *end;
	size_t out_size;

	// Trim leading space
	while(isspace((unsigned char)*str)) str++;

	if(*str == 0)  // All spaces?
	{
		*out = 0;
		return 1;
	}

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end--;
	end++;

	// Set output size to minimum of trimmed string length and buffer size minus 1
	out_size = (end - str) < len-1 ? (end - str) : len-1;

	// Copy trimmed string and add null terminator
	memcpy(out, str, out_size);
	out[out_size] = 0;

	return out_size;
}

static int config_set(char * key_value)
{
	int rv = -1;
	char * key = strtok(key_value, "=");
	char * value = strtok(NULL, "=");
	char buffer[PATH_MAX];

	if (key && value) {
		while(strchr(value, '\n')) {
			*strchr(value, '\n') = 0;
		}
		syslog(LOG_INFO, "set %s, value: %s", key, value);
		FILE * f = fopen("/etc/cnl.conf", "r");
		FILE * g = fopen("/etc/cnl.conf.new", "w");
		while (fgets(buffer, PATH_MAX, f)) {
			if (strstr(buffer, key)) {
				snprintf(buffer, PATH_MAX, "%s=%s\n", key, value);
				setenv(key, value, 1);
				rv = 0;
			}
			fputs(buffer, g);
		}
		fclose(f);
		fclose(g);
		system("mv /etc/cnl.conf.new /etc/cnl.conf");
	}
	return rv;
}

static int check_app_update(const char * rsp)
{
	if (strstr(rsp, "Updating")) {
		syslog(LOG_INFO, "updating app in %s, repo %s", getenv("CNL_APP_PATH"), getenv("CNL_APP_URL"));
		cnl_command("update", fileno(stdout));
	}
	return 0;
}

static void * blinkme_thread(void * a)
{
	pthread_detach(pthread_self());
	char irsp[PATH_MAX];
	char * set_led = "echo 0 > /sys/class/leds/led0/brightness";
	char * clr_led = "echo 1 > /sys/class/leds/led0/brightness";
	char * restore_led = "echo mmc0 > /sys/class/leds/led0/trigger";
	int rv = cnl_run("echo none > /sys/class/leds/led0/trigger", irsp, PATH_MAX);
	if (rv != 0) {
		rv = cnl_run("echo none > '/sys/class/leds/bananapi-m2-zero:red:pwr/trigger'", irsp, PATH_MAX);
		if (rv == 0) {
			set_led = "echo 0 > '/sys/class/leds/bananapi-m2-zero:red:pwr/brightness'";
			clr_led = "echo 1 > '/sys/class/leds/bananapi-m2-zero:red:pwr/brightness'";
			restore_led = "echo cpu > '/sys/class/leds/bananapi-m2-zero:red:pwr/trigger'";
		}
	}
	for (int i=0; i < 20; i++) {
		system(clr_led);
		usleep(250000);
		system(set_led);
		usleep(250000);
	}
	system(restore_led);
	return NULL;
}

static void * monitor_thread(void * a)
{
	pthread_detach(pthread_self());
	char last_url[256];
	strcpy(last_url, getenv("CNL_APP_URL"));
	int sleep_time = 0;
	int time_counter = -1;
	while (monitor_dowork) {
		if (strcmp(last_url, getenv("CNL_APP_URL"))) {
			syslog(LOG_NOTICE, "CNL_APP_URL changed. Removing and reloading application.");
			cnl_command("remove", fileno(stdout));
			cnl_command("clone", fileno(stdout));
			cnl_command("update", fileno(stdout));
			strcpy(last_url, getenv("CNL_APP_URL"));
		} else {
			if (sleep_time != strtol(getenv("CNL_APP_MONITOR_TIME"), NULL, 10)) {
				sleep_time = strtol(getenv("CNL_APP_MONITOR_TIME"), NULL, 10);
				time_counter = (sleep_time == 0 ? -1 : 0);
			}
			if (time_counter == 0) {
				syslog(LOG_INFO, "[monitor] performing pull command");
				cnl_command("pull", fileno(stdout));
				time_counter = sleep_time;
			} else if (time_counter > 0) {
				time_counter--;
				sleep(1);
			}
		}
	}
	syslog(LOG_INFO, "[monitor] exiting thread");
	return NULL;
}

/* update: compile and update app */
static int cnl_update_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char icmd[PATH_MAX];
	char irsp[PATH_MAX];
	snprintf(icmd, PATH_MAX, "make -C %s update 2>&1", getenv("CNL_APP_PATH"));
	int rv = cnl_run(icmd, irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s\n%s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

/* clean: clean app */
static int cnl_clean_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char irsp[PATH_MAX];
	char icmd[PATH_MAX];
	snprintf(icmd, PATH_MAX, "make -C %s clean 2>&1", getenv("CNL_APP_PATH"));
	int rv = cnl_run(icmd, irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s\n%s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

/* remove: delete cnl_app folder with sources in device */
static int cnl_remove_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char icmd[PATH_MAX];
	char irsp[PATH_MAX];
	snprintf(icmd, PATH_MAX, "rm -rf %s", getenv("CNL_APP_PATH"));
	int rv = cnl_run(icmd, irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s\n%s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

/* status: get systemd status of all cnl services */
static int cnl_status_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	int rv = -1;
	if (strstr(cmd, "status app")) {
		rv = cnl_run("systemctl status cnl_app.service", rsp, rsp_len);
	} else if (strstr(cmd, "status cnld")) {
		rv = cnl_run("systemctl status cnld.service", rsp, rsp_len);
	} else if (strstr(cmd, "status ssh")) {
		rv = cnl_run("systemctl status cnl_ssh.service", rsp, rsp_len);
	} else {
		char irsp[PATH_MAX];
		irsp[0] = 0;
		rsp[0] = 0;
		cnl_run("systemctl status cnl_app.service", irsp, PATH_MAX);
		strncat(rsp, irsp, rsp_len/2-2);
		strcat(rsp, "\n");
		cnl_run("systemctl status cnld.service", irsp, PATH_MAX);
		strncat(rsp, irsp, rsp_len/2-1);
		strcat(rsp, "\n");
		rv = 0;
	}
	return rv;
}

/* journal: get systemd journal of all cnl services */
static int cnl_journal_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	int rv = -1;
	if (strstr(cmd, "journal app")) {
		rv = cnl_run("journalctl -u cnl_app.service -n 50", rsp, rsp_len);
	} else if (strstr(cmd, "journal cnld")) {
		rv = cnl_run("journalctl -u cnld.service -n 50", rsp, rsp_len);
	} else if (strstr(cmd, "journal ssh")) {
		rv = cnl_run("journalctl -u cnl_ssh.service -n 50", rsp, rsp_len);
	} else {
		strcpy(rsp, "use: journal app|cnld|ssh\n");
		rv = 0;
	}
	return rv;
}

/* serial: get hardware serial number */
static int cnl_serial_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	return cnl_run("cat /proc/cpuinfo | grep Serial | cut -d' ' -f2", rsp, rsp_len);
}

/* pubkey: get public key */
static int cnl_pubkey_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char icmd[PATH_MAX];
	sprintf(icmd, "cat %s.pub", getenv("CNL_SSH_KEY"));
	return cnl_run(icmd, rsp, rsp_len);
}

/* clone: clone app repository */
static int cnl_clone_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	int rv = -1;
	char icmd[PATH_MAX];
	char irsp[PATH_MAX];
	irsp[0] = 0;
	icmd[0] = 0;
	snprintf(icmd, PATH_MAX, "GIT_SSH_COMMAND=\"ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" git clone %s %s 2>&1",
			getenv("CNL_SSH_KEY"), getenv("CNL_APP_URL"), getenv("CNL_APP_PATH"));
	rv = cnl_run(icmd, irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s\n%s\ncommand returned %d\n", icmd, irsp, rv);
	check_app_update(irsp);
	return rv;
}

/* pull: get latest changes from app repository */
static int cnl_pull_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char icmd[PATH_MAX];
	char irsp[PATH_MAX];
	irsp[0] = 0;
	icmd[0] = 0;
	snprintf(icmd, PATH_MAX, "cd %s; GIT_SSH_COMMAND=\"ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" git pull 2>&1",
			getenv("CNL_APP_PATH"), getenv("CNL_SSH_KEY"));
	int rv = cnl_run(icmd, irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s\n%s\ncommand returned %d\n", icmd, irsp, rv);
	check_app_update(irsp);
	return rv;
}

/* monitor on: start monitoring changes from app repository */
static int cnl_monitor_on_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	if (monitor_dowork == 0) {
		monitor_dowork = 1;
		pthread_create(&monitor_tid, NULL, monitor_thread, NULL);
		snprintf(rsp, rsp_len, "monitoring changes on %s every %s seconds\n", getenv("CNL_APP_URL"), getenv("CNL_APP_MONITOR_TIME"));
	} else {
		snprintf(rsp, rsp_len, "monitor already enabled\n");
	}
	return 0;
}

/* monitor off: stop monitoring changes in app repository */
static int cnl_monitor_off_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	monitor_dowork = 0;
	pthread_cancel(monitor_tid);
	snprintf(rsp, rsp_len, "monitoring disabled for %s\n", getenv("CNL_APP_URL"));
	return 0;
}

/* config set: set environment variables for configuration */
static int cnl_config_set_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	int rv = config_set(strstr(cmd, "config set ")+11);
	strcpy(rsp, rv == 0 ? "OK\n" : "ERROR\n");
	return rv;
}

/* config get: get environment variables */
static int cnl_config_get_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char field[64];
	trimwhitespace(field, 64, strstr(cmd, "config get ")+11);
	char * value = getenv(field);
	snprintf(rsp, rsp_len, "%s=%s\n", field, value);
	return 0;
}

static int cnl_start_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char irsp[PATH_MAX];
	int rv = cnl_run("systemctl start cnl_app.service", irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s %s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

static int cnl_stop_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char irsp[PATH_MAX];
	int rv = cnl_run("systemctl stop cnl_app.service", irsp, PATH_MAX);
	snprintf(rsp, rsp_len, "%s %s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

static int cnl_coretemp_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char irsp[PATH_MAX];
	int rv = cnl_run("/opt/vc/bin/vcgencmd measure_temp", irsp, PATH_MAX);
	if (rv != 0) {
		rv = cnl_run("cat /etc/armbianmonitor/datasources/soctemp", irsp, PATH_MAX);
		snprintf(irsp, PATH_MAX, "temp=%.3f'C\n", atof(irsp)/1000);
	}
	snprintf(rsp, rsp_len, "%s %s(%d)\n", irsp, rv == 0 ? "OK" : "ERROR", rv);
	return rv;
}

static int cnl_blinkme_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	pthread_t bm_tid;
	pthread_create(&bm_tid, NULL, blinkme_thread, NULL);
	strcpy(rsp, "OK\n");
	return 0;
}

static int cnl_update_cnld_cb(const char * cmd, char * rsp, size_t rsp_len)
{
	char icmd[PATH_MAX];
	snprintf(icmd, PATH_MAX,
			"cd %s; GIT_SSH_COMMAND=\"ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" git pull 2>&1; make update;",
			getenv("CNL_PATH"), getenv("CNL_SSH_KEY"));
#if 0
	return cnl_run(icmd, rsp, rsp_len);
#else
	strcpy(rsp, "error - command not available yet\n");
	return -1;
#endif
}

/*==================[external functions definition]==========================*/

int cnl_run(const char * cmd, char * rsp, size_t rsp_len)
{
	char cmd_ext[PATH_MAX];
	FILE * fp = popen(cmd, "r");
	int rv = -1;
	if (fp == NULL) {
		syslog(LOG_ERR, "popen error - cmd was '%s'", cmd);
		strncat(rsp, "ERROR", rsp_len);
	} else {
		rsp[0] = 0;
		while (fgets(cmd_ext, PATH_MAX-1, fp) != NULL) {
			strncat(rsp, cmd_ext, rsp_len-1-strlen(rsp));
		}
		rv = WEXITSTATUS(pclose(fp));
		if (rv != 0) {
			syslog(LOG_WARNING, "[cnl_run] command %s returned %d", cmd, rv);
		}
	}
	return rv;
}

int cnl_command(const char * cmd, int client)
{
	int rv = -1;
	int i = 0;
	char rsp[PATH_MAX];
	while(commands[i].cmd != NULL) {
		if (strstr(cmd, commands[i].cmd)) {
			if (commands[i].cb != NULL) {
				rv = commands[i].cb(cmd, rsp, PATH_MAX);
				write(client, rsp, strlen(rsp));
				replace_nonprintable_chars(rsp);
				syslog(LOG_INFO, "[cnl_command] cmd:%s rsp:%s rv:%d", cmd, rsp, rv);
			}
		}
		i++;
	}
	return rv;
}

/*==================[end of file]============================================*/
