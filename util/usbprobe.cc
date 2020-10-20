/* Implements the Usb_probe object we use to probe for certain devices.
 */

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

#include "usbprobe.hpp"
#include "logging.hpp"


/// CTOR : Get a libusb context
Usb_probe::Usb_probe()
{
    int rc = libusb_init(&m_ctx);
    if (rc) {
        LOG_ERROR(Lgr) << "Usb_probe failed to init libusb: error=" << rc;
        // the object is unusable
    }
    // LOG_DEBUG(Lgr) << "Usb_probe Device[0]="
    //                << std::setbase(16) << std::setw(4) << std::setfill('0')
    //                << m_specs[0].vendor << ":"
    //                << std::setbase(16) << std::setw(4) << std::setfill('0')
    //                << m_specs[0].product;
}

/// DTOR : Release the libusb context
Usb_probe::~Usb_probe()
{
    libusb_exit( m_ctx );
}

/// Empty the collection of acceptable devices.
///
void Usb_probe::clear_devices()
{
    m_specs.clear();
}

/// Add a vendor/device the collection of acceptable devices.
///
void Usb_probe::add_device(uint16_t vendor, uint16_t product)
{
    Dev_spec ds { vendor, product };
    m_specs.push_back( ds );
}


/// Log the idx indexed descriptor string of the USB device dev.
///
void Usb_probe::describe_dev( libusb_device *dev, uint8_t idx )
{
    libusb_device_handle *handle=nullptr;
    if (int err = libusb_open( dev, &handle )) {
        LOG_ERROR(Lgr) << "Usb_probe failed to open device, error " << err;
        return;
    } else {
        /*unsigned*/ char data[128];
        memset(data,0,sizeof(data));
        int nb = libusb_get_string_descriptor_ascii( handle,idx,
                                                     (unsigned char*)data,
                                                     sizeof(data)-1 );
        if (nb > 0) {
            std::string devstr { data };
            LOG_INFO(Lgr) << "Usb_probe matching device '" << devstr << "'";
        } else {
            LOG_WARNING(Lgr) << "Usb_probe could not fetch description string";
        }
    }
}

/// This is used to look for any of the vendor/product pairs. Return
/// the number of matches discovered on the USB buses.
///
size_t Usb_probe::count_devices(bool verbose)
{
    libusb_device **list=NULL;
    size_t discovered=0;
    ssize_t nd = 0;

    if (! m_ctx) { return 0; }
    nd = libusb_get_device_list(m_ctx, &list);
    
    for (ssize_t i=0; i<nd; i++) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor desc;
        if (int rc = libusb_get_device_descriptor( dev, &desc )) {
            LOG_ERROR(Lgr) << "Usb_probe error " << rc << 
                "while fetching USB device descriptor";
        } else {
            // compare against target specs
            for (auto spec : m_specs) {
                if ((desc.idVendor == spec.vendor) 
                      and (desc.idProduct == spec.product)) {
                    if (verbose) describe_dev( dev, desc.iProduct );
                    discovered++;
                }
            }
        }
    }
    libusb_free_device_list( list, 1 );
    return discovered;
}


