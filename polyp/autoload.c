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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "autoload.h"
#include "module.h"
#include "xmalloc.h"
#include "memchunk.h"
#include "sound-file.h"
#include "log.h"
#include "scache.h"
#include "subscribe.h"

static void entry_free(struct pa_autoload_entry *e) {
    assert(e);
    pa_subscription_post(e->core, PA_SUBSCRIPTION_EVENT_AUTOLOAD|PA_SUBSCRIPTION_EVENT_REMOVE, PA_INVALID_INDEX);
    pa_xfree(e->name);
    pa_xfree(e->module);
    pa_xfree(e->argument);
    pa_xfree(e);
}

static void entry_remove_and_free(struct pa_autoload_entry *e) {
    assert(e && e->core);

    pa_idxset_remove_by_data(e->core->autoload_idxset, e, NULL);
    pa_hashmap_remove(e->core->autoload_hashmap, e->name);
    entry_free(e);
}

static struct pa_autoload_entry* entry_new(struct pa_core *c, const char *name) {
    struct pa_autoload_entry *e = NULL;
    assert(c && name);
    
    if (c->autoload_hashmap && (e = pa_hashmap_get(c->autoload_hashmap, name)))
        return NULL;
    
    e = pa_xmalloc(sizeof(struct pa_autoload_entry));
    e->core = c;
    e->name = pa_xstrdup(name);
    e->module = e->argument = NULL;
    e->in_action = 0;
    
    if (!c->autoload_hashmap)
        c->autoload_hashmap = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    assert(c->autoload_hashmap);
    
    pa_hashmap_put(c->autoload_hashmap, e->name, e);

    if (!c->autoload_idxset)
        c->autoload_idxset = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
    pa_idxset_put(c->autoload_idxset, e, &e->index);

    pa_subscription_post(c, PA_SUBSCRIPTION_EVENT_AUTOLOAD|PA_SUBSCRIPTION_EVENT_NEW, e->index);
    
    return e;
}

int pa_autoload_add(struct pa_core *c, const char*name, enum pa_namereg_type type, const char*module, const char *argument, uint32_t *index) {
    struct pa_autoload_entry *e = NULL;
    assert(c && name && module && (type == PA_NAMEREG_SINK || type == PA_NAMEREG_SOURCE));
    
    if (!(e = entry_new(c, name)))
        return -1;
        
    e->module = pa_xstrdup(module);
    e->argument = pa_xstrdup(argument);
    e->type = type;

    if (index)
        *index = e->index;
    
    return 0;
}

int pa_autoload_remove_by_name(struct pa_core *c, const char*name, enum pa_namereg_type type) {
    struct pa_autoload_entry *e;
    assert(c && name && type);

    if (!c->autoload_hashmap || !(e = pa_hashmap_get(c->autoload_hashmap, name)) || e->type != type)
        return -1;

    entry_remove_and_free(e);
    return 0;
}

int pa_autoload_remove_by_index(struct pa_core *c, uint32_t index) {
    struct pa_autoload_entry *e;
    assert(c && index != PA_IDXSET_INVALID);

    if (!c->autoload_idxset || !(e = pa_idxset_get_by_index(c->autoload_idxset, index)))
        return -1;

    entry_remove_and_free(e);
    return 0;
}

void pa_autoload_request(struct pa_core *c, const char *name, enum pa_namereg_type type) {
    struct pa_autoload_entry *e;
    struct pa_module *m;
    assert(c && name);

    if (!c->autoload_hashmap || !(e = pa_hashmap_get(c->autoload_hashmap, name)) || (e->type != type))
        return;

    if (e->in_action)
        return;

    e->in_action = 1;

    if (type == PA_NAMEREG_SINK || type == PA_NAMEREG_SOURCE) {
        if ((m = pa_module_load(c, e->module, e->argument)))
            m->auto_unload = 1;
    }
    
    e->in_action = 0;
}

static void free_func(void *p, void *userdata) {
    struct pa_autoload_entry *e = p;
    pa_idxset_remove_by_data(e->core->autoload_idxset, e, NULL);
    entry_free(e);
}

void pa_autoload_free(struct pa_core *c) {
    if (c->autoload_hashmap) {
        pa_hashmap_free(c->autoload_hashmap, free_func, NULL);
        c->autoload_hashmap = NULL;
    }
    
    if (c->autoload_idxset) {
        pa_idxset_free(c->autoload_idxset, NULL, NULL);
        c->autoload_idxset = NULL;
    }
}

const struct pa_autoload_entry* pa_autoload_get_by_name(struct pa_core *c, const char*name, enum pa_namereg_type type) {
    struct pa_autoload_entry *e;
    assert(c && name);
    
    if (!c->autoload_hashmap || !(e = pa_hashmap_get(c->autoload_hashmap, name)) || e->type != type)
        return NULL;

    return e;
}

const struct pa_autoload_entry* pa_autoload_get_by_index(struct pa_core *c, uint32_t index) {
    struct pa_autoload_entry *e;
    assert(c && index != PA_IDXSET_INVALID);
    
    if (!c->autoload_idxset || !(e = pa_idxset_get_by_index(c->autoload_idxset, index)))
        return NULL;

    return e;
}
