/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once

#define LINUX_RAM_BASE    0xb0000000
#define LINUX_RAM_PADDR_BASE LINUX_RAM_BASE
#define LINUX_RAM_SIZE    0x10000000
#define PLAT_RAM_END      0xc0000000
#define LINUX_RAM_OFFSET  0
#define DTB_ADDR          0xbf000000
#define INITRD_MAX_SIZE   0x1900000 //25 MB
#define INITRD_ADDR       (DTB_ADDR - INITRD_MAX_SIZE) //0xBD700000

#define IRQ_SPI_OFFSET 32
#define GIC_IRQ_PHANDLE 0x01

static const int linux_pt_irqs[] = {
    /*27, // VTCNT */
};

static const int free_plat_interrupts[] =  { 50 + IRQ_SPI_OFFSET };
static const char *plat_keep_devices[] = {
    "/timer",
    "/xin24m",
    "/amba",
    "/pmu_a53",
    "/pmu_a72",
    "/psci",
};
static const char *plat_keep_device_and_disable[] = {
	"/pmu-clock-controller@ff750000",
	"/clock-controller@ff760000",
};
static const char *plat_keep_device_and_subtree[] = {
    "/interrupt-controller@fee00000",
};
static const char *plat_keep_device_and_subtree_and_disable[] = {};
