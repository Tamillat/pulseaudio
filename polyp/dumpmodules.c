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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <stdio.h>
#include <ltdl.h>

#include "dumpmodules.h"
#include "modinfo.h"

#define PREFIX "module-"

static void short_info(const char *name, const char *path, struct pa_modinfo *i) {
    assert(name && i);
    printf("%-40s%s\n", name, i->description ? i->description : "n/a");
}

static void long_info(const char *name, const char *path, struct pa_modinfo *i) {
    static int nl = 0;
    assert(name && i);
    
    if (nl)
        printf("\n");

    nl = 1;

    printf("Name: %s\n", name);
    
    if (!i->description && !i->version && !i->author && !i->usage)
        printf("No module information available\n");
    else {
        if (i->version)
            printf("Version: %s\n", i->version);
        if (i->description)
            printf("Description: %s\n", i->description);
        if (i->author)
            printf("Author: %s\n", i->author);
        if (i->usage)
            printf("Usage: %s\n", i->usage);
    }
    
    if (path)
        printf("Path: %s\n", path);
}

static void show_info(const char *name, const char *path, void (*info)(const char *name, const char *path, struct pa_modinfo*i)) {
    struct pa_modinfo *i;
    
    if ((i = pa_modinfo_get_by_name(path ? path : name))) {
        info(name, path, i);
        pa_modinfo_free(i);
    }
}

static int callback(const char *path, lt_ptr data) {
    const char *e;
    struct pa_daemon_conf *c = (data);

    if ((e = (const char*) strrchr(path, '/')))
        e++;
    else
        e = path;

    if (strlen(e) > sizeof(PREFIX)-1 && !strncmp(e, PREFIX, sizeof(PREFIX)-1))
        show_info(e, path, c->verbose ? long_info : short_info);
    
    return 0;
}

void pa_dump_modules(struct pa_daemon_conf *c, int argc, char * const argv[]) {
    if (argc > 0) {
        int i;
        for (i = 0; i < argc; i++)
            show_info(argv[i], NULL, long_info);
    } else
        lt_dlforeachfile(NULL, callback, c);
}
