/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

/* CAmkES-side handler for events emitted from a CAmkES component to a
 * guest user process.
 *
 * In order for a CAmkES component to emit events to a guest user process,
 * it must have an "emits" event interfaces connected to the vmm component.
 * When the vmm receives an event, it places some information identifying
 * the event in memory visible to the guest, then injects an interrupt into
 * the guest. The guest is expected to acknowledge the interrupt by making
 * a hypercall after it's finished reading from the shared state.
 */

#include <sel4/sel4.h>
#include "camkes_consumes_event.h"
#include "camkes_mutex.h"
#include "cross_vm_shared/cross_vm_shared_vmm_to_guest_event.h"
#include <sel4vm/guest_vm.h>
#include <sel4vm/arch/guest_x86_context.h>
#include <sel4vm/arch/vmcall.h>
#include <vspace/vspace.h>

static event_context_t *event_context = NULL;
static vspace_t *vmm_vspace;
static camkes_mutex_t *cross_vm_event_mutex;
static seL4_CPtr irq_notification;

static void event_camkes_callback(void *arg) {
    int error UNUSED;
    camkes_consumes_event_t *event = arg;

    if (event_context) {

        /* The event context is shared between the guest and the vmm.
         * It is used to communicate the identifier of the event
         * that is currently being emitted to the guest.
         * We take a mutex before updating the event context, and
         * the mutex is released by a hypercall made by the guest
         * after it's finished reading the event context.
         *
         * A mutex is required, as between updating the event context
         * in response to one event, and the guest processing the
         * injected interrupt, a second event may arrive.
         */
        error = camkes_mutex_lock(cross_vm_event_mutex);
        assert(!error);

        event_context->id = event->id;
        seL4_Signal(irq_notification);
    }

    error = camkes_event_reg_callback_self(event, event_camkes_callback);
    assert(!error);
}

static void event_interrupt_ack(void) {
    /* The guest will make a hypercall when it is finished reading
     * from the shared context.
     */
    int error UNUSED = camkes_mutex_unlock(cross_vm_event_mutex);
    assert(!error);
}

static int event_shmem_init(uintptr_t paddr, vm_mem_t *guest_mem) {

    // share event context between guest and vmm
    event_context = vspace_share_mem(&guest_mem->vm_vspace, vmm_vspace, (void*)paddr, 1 /* num pages */,
                                     PAGE_BITS_4K, seL4_AllRights, 1 /* cacheable */);

    if (event_context == NULL) {
        ZF_LOGE("Failed to share event context with guest");
        return -1;
    }

    // sanity check for shared memory
    event_context->magic = EVENT_CONTEXT_MAGIC;

    return 0;
}

static int event_vmcall_handler(vm_vcpu_t *vcpu) {

    int cmd;
    int error = vm_get_thread_context_reg(vcpu, VCPU_CONTEXT_EBX, &cmd);
    if (error) {
        ZF_LOGE("Failed to get thread context register for command");
        return -1;
    }

    switch (cmd) {
    case EVENT_CMD_INIT: {
        uintptr_t paddr;
        error = vm_get_thread_context_reg(vcpu, VCPU_CONTEXT_ECX, &paddr);
        if (error) {
            ZF_LOGE("Failed to get thread context register for event addr");
            return -1;
        }
        error = event_shmem_init(paddr, &vcpu->vm->mem);
        if (error) {
            return error;
        }
        break;
    }
    case EVENT_CMD_ACK: {
        event_interrupt_ack();
        break;
    }
    default: {
        ZF_LOGE("Unknown command: %d", cmd);
        return -1;
    }
    }

    return 0;
}

int cross_vm_consumes_events_init_common(vm_t *vm, vspace_t *vspace, camkes_mutex_t *mutex,
                                camkes_consumes_event_t *events, int n, seL4_CPtr notification) {

    vmm_vspace = vspace;
    cross_vm_event_mutex = mutex;
    irq_notification = notification;

    for (int i = 0; i < n; i++) {
        int error = camkes_event_reg_callback_self(&events[i], event_camkes_callback);
        if (error != 0) {
            return error;
        }
    }
    return reg_new_handler(vm, event_vmcall_handler, EVENT_VMCALL_VMM_TO_GUEST_HANDLER_TOKEN);
}

int cross_vm_consumes_event_irq_num(void) {
    return EVENT_IRQ_NUM;
}
