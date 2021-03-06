
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018-2019 by Omar Muhamed
 */

/* PCI IRQ Routing */
/* Every PCI device that is capable of generating an IRQ has an "interrupt pin"
   field in its configuration space. Contrary to what most people believe, this
   field is valid for both the PIC and the I/O APIC. The PCI local bus spec clearly
   says the "interrupt line" field everyone trusts are simply for BIOS or OS-
   -specific use. Therefore, nobody should assume it contains the real IRQ. Instead,
   the four PCI pins should be used: LNKA, LNKB, LNKC and LNKD. */

#include <lai/core.h>
#include "libc.h"
#include "eval.h"

#define PCI_PNP_ID        "PNP0A03"

// lai_pci_route(): Resolves PCI IRQ routing for a specific device
// Param:    acpi_resource_t *dest - destination buffer
// Param:    uint8_t bus - PCI bus
// Param:    uint8_t slot - PCI slot
// Param:    uint8_t function - PCI function
// Return:    int - 0 on success

int lai_pci_route(acpi_resource_t *dest, uint8_t bus, uint8_t slot, uint8_t function)
{
    //lai_debug("attempt to resolve PCI IRQ for device %X:%X:%X\n", bus, slot, function);

    // determine the interrupt pin
    uint8_t pin = (uint8_t)(laihost_pci_read(bus, slot, function, 0x3C) >> 8);
    if(pin == 0 || pin > 4)
        return 1;

    pin--;        // because PCI numbers the pins from 1, but ACPI numbers them from 0

    // find the PCI bus in the namespace
    lai_object_t bus_number = {0};
    lai_object_t pnp_id = {0};

    lai_eisaid(&pnp_id, PCI_PNP_ID);

    size_t index = 0;
    lai_nsnode_t *handle = lai_get_deviceid(index, &pnp_id);
    char path[ACPI_MAX_NAME];
    int status;

    while(handle != NULL)
    {
        lai_strcpy(path, handle->path);
        lai_strcpy(path + lai_strlen(path), "._BBN");    // _BBN: Base bus number

        status = lai_eval(&bus_number, path);
        if(status != 0)
        {
            // when _BBN is not present, we assume bus 0
            bus_number.type = LAI_INTEGER;
            bus_number.integer = 0;
        }

        if((uint8_t)bus_number.integer == bus)
            break;

        index++;
        handle = lai_get_deviceid(index, &pnp_id);
    }

    if(handle == NULL)
        return 1;

    // read the PCI routing table
    lai_strcpy(path, handle->path);
    lai_strcpy(path + lai_strlen(path), "._PRT");    // _PRT: PCI Routing Table

    lai_object_t prt = {0};
    lai_object_t prt_package = {0};
    lai_object_t prt_entry = {0};

    /* _PRT is a package of packages. Each package within the PRT is in the following format:
       0: Integer:    Address of device. Low WORD = function, high WORD = slot
       1: Integer:    Interrupt pin. 0 = LNKA, 1 = LNKB, 2 = LNKC, 3 = LNKD
       2: Name or Integer:    If name, this is the namespace device which allocates the interrupt.
        If it's an integer, then this field is ignored.
       3: Integer:    If offset 2 is a Name, this is the index within the resource descriptor
        of the specified device which contains the PCI interrupt. If offset 2 is an
        integer, this field is the ACPI GSI of this PCI IRQ. */

    status = lai_eval(&prt, path);

    if(status != 0)
        return 1;

    size_t i = 0;

    while(1)
    {
        // read the _PRT package
        status = lai_eval_package(&prt, i, &prt_package);
        if(status != 0)
            return 1;

        if(prt_package.type != LAI_PACKAGE)
            return 1;

        // read the device address
        status = lai_eval_package(&prt_package, 0, &prt_entry);
        if(status != 0)
            return 1;

        if(prt_entry.type != LAI_INTEGER)
            return 1;

        // is this the device we want?
        if((prt_entry.integer >> 16) == slot)
        {
            if((prt_entry.integer & 0xFFFF) == 0xFFFF || (prt_entry.integer & 0xFFFF) == function)
            {
                // is this the interrupt pin we want?
                status = lai_eval_package(&prt_package, 1, &prt_entry);
                if(status != 0)
                    return 1;

                if(prt_entry.type != LAI_INTEGER)
                    return 1;

                if(prt_entry.integer == pin)
                    goto resolve_pin;
            }
        }

        // nope, go on
        i++;
    }

resolve_pin:
    // here we've found what we need
    // is it a link device or a GSI?
    status = lai_eval_package(&prt_package, 2, &prt_entry);
    if(status != 0)
        return 1;

    acpi_resource_t *res;
    size_t res_count;

    if(prt_entry.type == LAI_INTEGER)
    {
        // GSI
        status = lai_eval_package(&prt_package, 3, &prt_entry);
        if(status != 0)
            return 1;

        dest->type = ACPI_RESOURCE_IRQ;
        dest->base = prt_entry.integer;
        dest->irq_flags = ACPI_IRQ_LEVEL | ACPI_IRQ_ACTIVE_HIGH | ACPI_IRQ_SHARED;

        lai_debug("PCI device %02X:%02X:%02X is using IRQ %d\n", bus, slot, function, (int)dest->base);
        return 0;
    } else if(prt_entry.type == LAI_HANDLE)
    {
        // PCI Interrupt Link Device
        lai_debug("PCI interrupt link is %s\n", prt_entry.handle->path);

        // read the resource template of the device
        res = lai_calloc(sizeof(acpi_resource_t), ACPI_MAX_RESOURCES);
        res_count = lai_read_resource(prt_entry.handle, res);

        if(!res_count)
            return 1;

        i = 0;
        while(i < res_count)
        {
            if(res[i].type == ACPI_RESOURCE_IRQ)
            {
                dest->type = ACPI_RESOURCE_IRQ;
                dest->base = res[i].base;
                dest->irq_flags = res[i].irq_flags;

                laihost_free(res);

                lai_debug("PCI device %02X:%02X:%02X is using IRQ %d\n", bus, slot, function, (int)dest->base);
                return 0;
            }

            i++;
        }

        return 0;
    } else
        return 1;
}












