#ifndef fooautoloadhfoo
#define fooautoloadhfoo

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

#include "namereg.h"

struct pa_autoload_entry {
    struct pa_core *core;
    uint32_t index;
    char *name;
    enum pa_namereg_type type;
    int in_action;
    char *module, *argument;   
};

int pa_autoload_add(struct pa_core *c, const char*name, enum pa_namereg_type type, const char*module, const char *argument, uint32_t *index);
void pa_autoload_free(struct pa_core *c);
int pa_autoload_remove_by_name(struct pa_core *c, const char*name, enum pa_namereg_type type);
int pa_autoload_remove_by_index(struct pa_core *c, uint32_t index);
void pa_autoload_request(struct pa_core *c, const char *name, enum pa_namereg_type type);

const struct pa_autoload_entry* pa_autoload_get_by_name(struct pa_core *c, const char*name, enum pa_namereg_type type);
const struct pa_autoload_entry* pa_autoload_get_by_index(struct pa_core *c, uint32_t index);

#endif
