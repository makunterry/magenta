// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/syscalls.h>
#include <magenta/types.h>
#include <ddk/iotxn.h>
#include <system/listnode.h>

// for ssize_t:
#include <unistd.h>

typedef struct mx_device mx_device_t;
typedef struct mx_driver mx_driver_t;
typedef struct mx_device_prop mx_device_prop_t;

typedef struct mx_protocol_device mx_protocol_device_t;

typedef struct vnode vnode_t;

//TODO: multi-char constants are implementation-specific
//      move to something more ABI-stable

#define MX_DEVICE_NAME_MAX 32

struct mx_device {
    uintptr_t magic;

    const char* name;

    mx_protocol_device_t* ops;

    uint32_t flags;
    uint32_t refcount;

    mx_handle_t event;
    mx_handle_t remote;
    uintptr_t remote_id;

    // most devices implement a single
    // protocol beyond the base device protocol
    uint32_t protocol_id;
    void* protocol_ops;

    mx_driver_t* driver;
    // driver that has published this device

    mx_device_t* parent;
    // parent in the device tree

    mx_driver_t* owner;
    // driver that is bound to this device, NULL if unbound

    struct list_node node;
    // for the parent's device_list

    struct list_node children;
    // list of this device's children in the device tree

    struct list_node unode;
    // for list of all unmatched devices, if not bound
    // TODO: use this for general lifecycle tracking

    vnode_t* vnode;
    // used by devmgr internals

    mx_device_prop_t* props;
    uint32_t prop_count;
    // properties for driver binding

    char namedata[MX_DEVICE_NAME_MAX + 1];
};

// mx_device_t objects must be created or initialized by the driver manager's
// device_create() and device_init() functions.  Drivers MAY NOT touch any
// fields in the mx_device_t, except for the protocol_id and protocol_ops
// fields which it may fill out after init and before device_add() is called.

// The Device Protocol
typedef struct mx_protocol_device {
    mx_status_t (*get_protocol)(mx_device_t* dev, uint32_t proto_id, void** protocol);
    // Asks if the device supports a specific protocol.
    // If it does, protocol ops returned via **protocol.

    mx_status_t (*open)(mx_device_t* dev, mx_device_t** dev_out, uint32_t flags);
    // The optional dev_out parameter allows a device to create a per-instance
    // child drevice on open and return that (resulting in the opener opening
    // that child device instead).  If dev_out is not modified the device itself
    // is opened.
    //
    // The per-instance child should be created with device_create() or device_init(),
    // but added with device_add_instance() instead of device_add().

    mx_status_t (*close)(mx_device_t* dev);

    mx_status_t (*release)(mx_device_t* dev);
    // Release any resources held by the mx_device_t and free() it.
    // release is called after a device is remove()'d and its
    // refcount hits zero (all closes and unbinds complete)

    ssize_t (*read)(mx_device_t* dev, void* buf, size_t count, mx_off_t off);
    // attempt to read count bytes at offset off
    // off may be ignored for devices without the concept of a position

    ssize_t (*write)(mx_device_t* dev, const void* buf, size_t count, mx_off_t off);
    // attempt to write count bytes at offset off
    // off may be ignored for devices without the concept of a position

    void (*iotxn_queue)(mx_device_t* dev, iotxn_t* txn);
    // queue an iotxn. iotxn's are always completed by its complete() op

    mx_off_t (*get_size)(mx_device_t* dev);
    // optional: return the size (in bytes) of the readable/writable space
    // of the device.  Will default to 0 (non-seekable) if this is unimplemented

    ssize_t (*ioctl)(mx_device_t* dev, uint32_t op,
                     const void* in_buf, size_t in_len,
                     void* out_buf, size_t out_len);
    // optional: do an device-specific io operation
} mx_protocol_device_t;

// Device Convenience Wrappers
static inline mx_status_t device_get_protocol(mx_device_t* dev, uint32_t proto_id, void** protocol) {
    return dev->ops->get_protocol(dev, proto_id, protocol);
}

// State change functions
// Used by driver to indicate if there's data available to read,
// or room to write, or an error condition.
#define DEV_STATE_READABLE MX_SIGNAL_SIGNAL0
#define DEV_STATE_WRITABLE MX_SIGNAL_SIGNAL1
#define DEV_STATE_ERROR MX_SIGNAL_SIGNAL2

static inline void device_state_set(mx_device_t* dev, mx_signals_t stateflag) {
    mx_object_signal(dev->event, 0, stateflag);
}

static inline void device_state_clr(mx_device_t* dev, mx_signals_t stateflag) {
    mx_object_signal(dev->event, stateflag, 0);
}

static inline void device_state_set_clr(mx_device_t* dev, mx_signals_t setflag, mx_signals_t clearflag) {
    mx_object_signal(dev->event, clearflag, setflag);
}
