#pragma once

/*   Part of the rsked package.
 *
 *   Copyright 2020 Steven A. Harp
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include <vector>

/// link with libusb-1.0  (-lusb-1.0)
#include <libusb-1.0/libusb.h>

/// This is used to probe for various SDR devices on USB.
///
class Usb_probe {
private:
    struct Dev_spec {
        uint16_t vendor {0};
        uint16_t product {0};
    };
    std::vector<Dev_spec> m_specs { {0x0bda, 0x2838}, {0x1df7, 0x3010} };
    void describe_dev(libusb_device *, uint8_t );
    libusb_context *m_ctx {nullptr};
public:
    Usb_probe();
    Usb_probe( Usb_probe const& ) = delete;
    ~Usb_probe();
    void operator=( Usb_probe const& ) = delete;
    //
    void add_device( uint16_t, uint16_t);
    void clear_devices();
    size_t count_devices(bool verbose=false);
    size_t nspecs() const { return m_specs.size(); }
};
