#ifndef foodaemonconfhfoo
#define foodaemonconfhfoo

/* $Id$ */

/***
  This file is part of polypaudio.

  polypaudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  polypaudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with polypaudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include "log.h"

enum pa_daemon_conf_cmd {
	PA_CMD_DAEMON,
	PA_CMD_HELP,
        PA_CMD_VERSION,
	PA_CMD_DUMP_CONF,
	PA_CMD_DUMP_MODULES
};

struct pa_daemon_conf {
    enum pa_daemon_conf_cmd cmd;
    int daemonize,
        fail,
        verbose,
        high_priority,
        disallow_module_loading,
        exit_idle_time,
        module_idle_time,
        scache_idle_time,
        auto_log_target;
    char *script_commands, *dl_search_path, *default_script_file;
    enum pa_log_target log_target;
    int resample_method;
    char *config_file;
};

struct pa_daemon_conf* pa_daemon_conf_new(void);
void pa_daemon_conf_free(struct pa_daemon_conf*c);

int pa_daemon_conf_load(struct pa_daemon_conf *c, const char *filename);
char *pa_daemon_conf_dump(struct pa_daemon_conf *c);
int pa_daemon_conf_env(struct pa_daemon_conf *c);

int pa_daemon_conf_set_log_target(struct pa_daemon_conf *c, const char *string);
int pa_daemon_conf_set_resample_method(struct pa_daemon_conf *c, const char *string);

#endif
