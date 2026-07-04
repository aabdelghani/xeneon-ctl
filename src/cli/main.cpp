// xeneonctl — CLI for the Corsair Xeneon Edge on Linux.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "transport/HidEnumerator.h"
#include "transport/HidRecon.h"

#include <cstdio>
#include <cstring>

static int cmdList()
{
    auto info = xen::HidEnumerator::findEdge();
    if (!info) {
        std::puts("No Xeneon Edge (1b1c:1d0d) found.");
        return 1;
    }
    std::printf("XENEON EDGE\n");
    std::printf("  product:    %s\n", info->product.c_str());
    std::printf("  serial:     %s\n", info->serial.c_str());
    std::printf("  hidraw:     %s\n", info->path.c_str());
    std::printf("  usage page: 0x%04X\n", info->usagePage);
    std::printf("  access:     %s\n",
                info->accessible ? "OK" : "DENIED (install udev rule, see README)");
    return info->accessible ? 0 : 2;
}

int main(int argc, char** argv)
{
    if (argc < 2 || std::strcmp(argv[1], "list") == 0)
        return cmdList();

    if (std::strcmp(argv[1], "probe") == 0) {
        // Read-only reconnaissance. This command NEVER writes to the device.
        xen::ReconReport r = xen::HidRecon::run();
        if (!r.haveInfo) {
            std::puts("No Xeneon Edge found.");
            return 1;
        }
        std::printf("XENEON EDGE  (read-only probe — no writes sent)\n");
        std::printf("  product:    %s\n", r.info.product.c_str());
        std::printf("  serial:     %s\n", r.info.serial.c_str());
        std::printf("  hidraw:     %s\n", r.info.path.c_str());
        std::printf("  usage page: 0x%04X\n", r.info.usagePage);
        std::printf("\nReport descriptor (%zu bytes, %s):\n",
                    r.reportDescriptor.size(), r.descriptorSource.c_str());
        for (size_t i = 0; i < r.reportDescriptor.size(); ++i)
            std::printf("%02X%s", r.reportDescriptor[i],
                        (i + 1) % 16 == 0 ? "\n" : " ");
        if (!r.reportDescriptor.empty() && r.reportDescriptor.size() % 16 != 0)
            std::putchar('\n');
        std::printf("\nDecode:\n%s", r.descriptorDecode.c_str());
        std::printf("\nPassive read: %s\n", r.passiveNote.c_str());
        if (!r.passiveReport.empty()) {
            std::printf("  bytes:");
            for (uint8_t b : r.passiveReport)
                std::printf(" %02X", b);
            std::putchar('\n');
        }
        return 0;
    }

    std::printf("usage: %s [list|probe]\n", argv[0]);
    return 64;
}
