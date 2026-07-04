// SPDX-License-Identifier: GPL-3.0-or-later
#include "transport/HidRecon.h"

#include "transport/HidTransport.h"

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>

namespace xen {

namespace {

// Locate the sysfs report_descriptor for a hidraw node path like /dev/hidraw4.
std::string sysfsDescriptorPath(const std::string& devPath)
{
    const auto slash = devPath.find_last_of('/');
    const std::string name = (slash == std::string::npos) ? devPath : devPath.substr(slash + 1);
    if (name.empty())
        return {};
    return "/sys/class/hidraw/" + name + "/device/report_descriptor";
}

std::vector<uint8_t> readFileBytes(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

// Minimal HID report-descriptor decode covering the items this device uses.
std::string decodeDescriptor(const std::vector<uint8_t>& d)
{
    std::ostringstream os;
    size_t i = 0;
    auto val = [&](int n) -> long {
        long v = 0;
        for (int k = 0; k < n && i + 1 + k < d.size(); ++k)
            v |= static_cast<long>(d[i + 1 + k]) << (8 * k);
        return v;
    };
    while (i < d.size()) {
        const uint8_t b = d[i];
        const int size = b & 0x03;
        const int type = (b >> 2) & 0x03;
        const int tag = (b >> 4) & 0x0F;
        const long v = val(size);
        char line[128];
        const char* name = "?";
        // type 1 = Global, 2 = Local, 0 = Main
        if (type == 1 && tag == 0x0) name = "Usage Page";
        else if (type == 1 && tag == 0x8) name = "Report ID";
        else if (type == 1 && tag == 0x7) name = "Report Size";
        else if (type == 1 && tag == 0x9) name = "Report Count";
        else if (type == 1 && tag == 0x1) name = "Logical Min";
        else if (type == 1 && tag == 0x2) name = "Logical Max";
        else if (type == 2 && tag == 0x0) name = "Usage";
        else if (type == 0 && tag == 0xA) name = "Collection";
        else if (type == 0 && tag == 0xC) name = "End Collection";
        else if (type == 0 && tag == 0x8) name = "Input";
        else if (type == 0 && tag == 0x9) name = "Output";
        else if (type == 0 && tag == 0xB) name = "Feature";
        std::snprintf(line, sizeof line, "  %-14s 0x%02lX (%ld)\n", name, v, v);
        os << line;
        i += 1 + size;
    }
    return os.str();
}

} // namespace

ReconReport HidRecon::run(int passiveTimeoutMs)
{
    ReconReport r;

    if (auto info = HidEnumerator::findEdge()) {
        r.info = *info;
        r.haveInfo = true;
    } else {
        r.descriptorSource = "device not found";
        return r;
    }

    // Report descriptor from sysfs (read-only file).
    const std::string sysPath = sysfsDescriptorPath(r.info.path);
    r.reportDescriptor = readFileBytes(sysPath);
    if (!r.reportDescriptor.empty()) {
        r.descriptorSource = sysPath;
        r.descriptorDecode = decodeDescriptor(r.reportDescriptor);
    } else {
        r.descriptorSource = "unavailable (" + sysPath + ")";
    }

    // Passive read: open and read once with a short timeout. hid_read never
    // writes to the device; if nothing is volunteered we simply time out.
    if (r.info.accessible) {
        HidTransport t;
        if (t.open(r.info.path)) {
            r.passiveReadAttempted = true;
            auto data = t.read(64, passiveTimeoutMs);
            if (!data.empty()) {
                r.passiveReport = data;
                r.passiveNote = "device volunteered an input report";
            } else {
                r.passiveNote = "no unsolicited report within timeout (expected — "
                                "this device is request/response)";
            }
        } else {
            r.passiveNote = "open failed: " + t.lastError();
        }
    } else {
        r.passiveNote = "skipped (hidraw node not accessible)";
    }

    return r;
}

} // namespace xen
