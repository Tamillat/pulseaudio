/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/inotify.h>
#include <libudev.h>

#include <pulsecore/modargs.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/namereg.h>

#include "module-udev-detect-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Detect available audio hardware and load matching drivers");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(TRUE);

struct device {
    char *path;
    char *card_name;
    uint32_t module;
};

struct userdata {
    pa_core *core;
    pa_hashmap *devices;
    pa_bool_t use_tsched;
    struct udev* udev;
    struct udev_monitor *monitor;
    pa_io_event *io;
};

static const char* const valid_modargs[] = {
    "tsched",
    NULL
};

static void device_free(struct device *d) {
    pa_assert(d);

    pa_xfree(d->path);
    pa_xfree(d->card_name);
    pa_xfree(d);
}

static const char *path_get_card_id(const char *path) {
    const char *e;

    if (!path)
        return NULL;

    if (!(e = strrchr(path, '/')))
        return NULL;

    if (!pa_startswith(e, "/card"))
        return NULL;

    return e + 5;
}

static void verify_access(struct userdata *u, struct device *d) {
    char *cd;
    int r;
    pa_card *card;

    pa_assert(u);
    pa_assert(d);

    cd = pa_sprintf_malloc("%s/controlC%s", d->path, path_get_card_id(d->path));
    r = access(cd, W_OK);
    pa_log_info("%s is accessible: %s", cd, pa_yes_no(r >= 0));
    pa_xfree(cd);

    if ((card = pa_namereg_get(u->core, d->card_name, PA_NAMEREG_CARD)))
        pa_card_suspend(card, r < 0, PA_SUSPEND_SESSION);
}

static void card_changed(struct userdata *u, struct udev_device *dev) {
    struct device *d;
    const char *path;
    const char *t;
    char *card_name, *args;
    pa_module *m;
    char *n;

    pa_assert(u);
    pa_assert(dev);

    path = udev_device_get_devpath(dev);

    if ((d = pa_hashmap_get(u->devices, path))) {
        verify_access(u, d);
        return;
    }

    if (!(t = udev_device_get_property_value(dev, "PULSE_NAME")))
        if (!(t = udev_device_get_property_value(dev, "ID_ID")))
            if (!(t = udev_device_get_property_value(dev, "ID_PATH")))
                t = path_get_card_id(path);

    n = pa_namereg_make_valid_name(t);

    card_name = pa_sprintf_malloc("alsa_card.%s", n);
    args = pa_sprintf_malloc("device_id=\"%s\" "
                             "name=\"%s\" "
                             "card_name=\"%s\" "
                             "tsched=%i "
                             "card_properties=\"module-udev-detect.discovered=1\"",
                             path_get_card_id(path),
                             n,
                             card_name,
                             (int) u->use_tsched);

    pa_log_debug("Loading module-alsa-card with arguments '%s'", args);
    m = pa_module_load(u->core, "module-alsa-card", args);
    pa_xfree(args);

    if (m) {
        pa_log_info("Card %s (%s) added.", path, n);

        d = pa_xnew(struct device, 1);
        d->path = pa_xstrdup(path);
        d->card_name = card_name;
        d->module = m->index;

        pa_hashmap_put(u->devices, d->path, d);
    } else
        pa_xfree(card_name);

    pa_xfree(n);
}

static void remove_card(struct userdata *u, struct udev_device *dev) {
    struct device *d;

    pa_assert(u);
    pa_assert(dev);

    if (!(d = pa_hashmap_remove(u->devices, udev_device_get_devpath(dev))))
        return;

    pa_log_info("Card %s removed.", d->path);
    pa_module_unload_request_by_index(u->core, d->module, TRUE);
    device_free(d);
}

static void process_device(struct userdata *u, struct udev_device *dev) {
    const char *action, *ff;

    pa_assert(u);
    pa_assert(dev);

    if (udev_device_get_property_value(dev, "PULSE_IGNORE")) {
        pa_log_info("Ignoring %s, because marked so.", udev_device_get_devpath(dev));
        return;
    }

    if ((ff = udev_device_get_property_value(dev, "SOUND_FORM_FACTOR")) &&
        pa_streq(ff, "modem")) {
        pa_log_info("Ignoring %s, because it is a modem.", udev_device_get_devpath(dev));
        return;
    }

    action = udev_device_get_action(dev);

    if (action && pa_streq(action, "remove"))
        remove_card(u, dev);
    else if ((!action || pa_streq(action, "change")) &&
             udev_device_get_property_value(dev, "SOUND_INITIALIZED"))
        card_changed(u, dev);

    /* For an explanation why we don't look for 'add' events here
     * have a look into /lib/udev/rules.d/78-sound-card.rules! */
}

static void process_path(struct userdata *u, const char *path) {
    struct udev_device *dev;

    if (!path_get_card_id(path))
        return;

    if (!(dev = udev_device_new_from_syspath(u->udev, path))) {
        pa_log("Failed to get udev device object from udev.");
        return;
    }

    process_device(u, dev);
    udev_device_unref(dev);
}

static void monitor_cb(pa_mainloop_api*a, pa_io_event* e, int fd, pa_io_event_flags_t events, void *userdata) {
    struct userdata *u = userdata;
    struct udev_device *dev;

    pa_assert(a);

    if (!(dev = udev_monitor_receive_device(u->monitor))) {
        pa_log("Failed to get udev device object from monitor.");
        return;
    }

    if (!path_get_card_id(udev_device_get_devpath(dev)))
        return;

    process_device(u, dev);
    udev_device_unref(dev);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma;
    struct udev_enumerate *enumerate = NULL;
    struct udev_list_entry *item = NULL, *first = NULL;
    int fd;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->devices = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    u->use_tsched = TRUE;

    if (pa_modargs_get_value_boolean(ma, "tsched", &u->use_tsched) < 0) {
        pa_log("Failed to parse tsched argument.");
        goto fail;
    }

    if (!(u->udev = udev_new())) {
        pa_log("Failed to initialize udev library.");
        goto fail;
    }

    if (!(u->monitor = udev_monitor_new_from_netlink(u->udev, "udev"))) {
        pa_log("Failed to initialize monitor.");
        goto fail;
    }

    errno = 0;
    if (udev_monitor_enable_receiving(u->monitor) < 0) {
        pa_log("Failed to enable monitor: %s", pa_cstrerror(errno));
        if (errno == EPERM)
            pa_log_info("Most likely your kernel is simply too old and "
                        "allows only priviliged processes to listen to device events. "
                        "Please upgrade your kernel to at least 2.6.30.");
        goto fail;
    }

    if ((fd = udev_monitor_get_fd(u->monitor)) < 0) {
        pa_log("Failed to get udev monitor fd.");
        goto fail;
    }

    pa_assert_se(u->io = u->core->mainloop->io_new(u->core->mainloop, fd, PA_IO_EVENT_INPUT, monitor_cb, u));

    if (!(enumerate = udev_enumerate_new(u->udev))) {
        pa_log("Failed to initialize udev enumerator.");
        goto fail;
    }

    if (udev_enumerate_add_match_subsystem(enumerate, "sound") < 0) {
        pa_log("Failed to match to subsystem.");
        goto fail;
    }

    if (udev_enumerate_scan_devices(enumerate) < 0) {
        pa_log("Failed to scan for devices.");
        goto fail;
    }

    first = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(item, first)
        process_path(u, udev_list_entry_get_name(item));

    udev_enumerate_unref(enumerate);

    pa_log_info("Loaded %u modules.", pa_hashmap_size(u->devices));

    pa_modargs_free(ma);

    return 0;

fail:

    if (enumerate)
        udev_enumerate_unref(enumerate);

    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->monitor)
        udev_monitor_unref(u->monitor);

    if (u->udev)
        udev_unref(u->udev);

    if (u->devices) {
        struct device *d;

        while ((d = pa_hashmap_steal_first(u->devices)))
            device_free(d);

        pa_hashmap_free(u->devices, NULL, NULL);
    }

    pa_xfree(u);
}