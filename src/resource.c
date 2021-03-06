
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018-2019 by Omar Muhamed
 */

/* ACPI Resource Template Implementation */
/* Allows discovering of each device's used resources, and thus is needed
 * for basic system enumeration as well as PCI IRQ routing. */

#include <lai/core.h>
#include "libc.h"

#define ACPI_SMALL_IRQ            0x04
#define ACPI_SMALL_DMA            0x05
#define ACPI_SMALL_IO            0x08
#define ACPI_SMALL_FIXED_IO        0x09
#define ACPI_SMALL_FIXED_DMA        0x0A
#define ACPI_SMALL_VENDOR        0x0E
#define ACPI_SMALL_END            0x0F

#define ACPI_LARGE_MEM24        0x81
#define ACPI_LARGE_REGISTER        0x82
#define ACPI_LARGE_MEM32        0x85
#define ACPI_LARGE_FIXED_MEM32        0x86
#define ACPI_LARGE_IRQ            0x89

// lai_read_resource(): Reads a device's resource settings
// Param:    lai_nsnode_t *device - device handle
// Param:    acpi_resource_t *dest - destination array
// Return:    size_t - count of entries successfully read

size_t lai_read_resource(lai_nsnode_t *device, acpi_resource_t *dest)
{
    char crs[ACPI_MAX_NAME];
    lai_strcpy(crs, device->path);
    lai_strcpy(crs + lai_strlen(crs), "._CRS");    // _CRS: current resource settings

    lai_object_t buffer = {0};
    int status = lai_eval(&buffer, crs);
    if(status != 0)
        return 0;

    // read the resource buffer
    size_t count = 0;
    uint8_t *data = (uint8_t*)buffer.buffer;
    size_t data_size;

    acpi_small_irq_t *small_irq;
    uint16_t small_irq_mask;

    acpi_large_irq_t *large_irq;

    size_t i;

    while(data[0] != 0x79)
    {
        if((data[0] & 0x80) == 0)
        {
            // small resource descriptor
            data_size = (size_t)data[0] & 7;

            switch(data[0] >> 3)
            {
            case ACPI_SMALL_END:
                return count;

            case ACPI_SMALL_IRQ:
                small_irq = (acpi_small_irq_t*)&data[0];
                small_irq_mask = small_irq->irq_mask;

                i = 0;

                while(small_irq_mask != 0)
                {
                    if(small_irq_mask & (1 << i))
                    {
                        dest[count].type = ACPI_RESOURCE_IRQ;
                        dest[count].base = (uint64_t)i;

                        /* ACPI spec says when irq flags are not present, we should
                           assume active high, edge-triggered, exclusive */
                        if(data_size >= 3)
                            dest[count].irq_flags = small_irq->config;
                        else
                            dest[count].irq_flags = ACPI_IRQ_ACTIVE_HIGH | ACPI_IRQ_EDGE | ACPI_IRQ_EXCLUSIVE;

                        small_irq_mask &= ~(1 << i);
                        count++;
                    }

                    i++;
                }

                data += data_size + 1;
                break;

            default:
                lai_debug("acpi warning: undefined small resource, byte 0 is %02X, ignoring...\n", data[0]);
                return 0;
            }
        } else
        {
            // large resource descriptor
            data_size = (size_t)data[1] & 0xFF;
            data_size |= (size_t)((data[2] & 0xFF) << 8);

            switch(data[0])
            {
            case ACPI_LARGE_IRQ:
                large_irq = (acpi_large_irq_t*)&data[0];

                dest[count].type = ACPI_RESOURCE_IRQ;
                dest[count].base = (uint64_t)large_irq->irq;
                dest[count].irq_flags = large_irq->config;

                count++;
                data += data_size + 3;
                break;

            default:
                lai_debug("acpi warning: undefined large resource, byte 0 is %02X, ignoring...\n", data[0]);
                return 0;
            }
        }
    }

    return count;
}







