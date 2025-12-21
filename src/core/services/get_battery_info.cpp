/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../../iDescriptor.h"
#include "plist/plist.h"
#include <QDebug>
#include <string>

// FIXME: return bool
void get_battery_info(IdeviceProviderHandle *provider, plist_t &diagnostics)
{
    // 1. Connect to the diagnostics_relay service using the raw C function.
    DiagnosticsRelayClientHandle *client_handle = nullptr;
    IdeviceFfiError *err =
        ::diagnostics_relay_client_connect(provider, &client_handle);

    if (err) {
        qDebug() << "Failed to create diagnostics relay client:"
                 << err->message;
        idevice_error_free(err);
        return;
    }

    // 2. Adopt the raw handle into the C++ RAII wrapper.
    // The client will now be automatically freed when it goes out of scope.
    auto diagnostics_client =
        IdeviceFFI::DiagnosticsRelay::adopt(client_handle);

    // 3. Query IORegistry for battery info.
    auto ioreg_result = diagnostics_client.ioregistry(
        IdeviceFFI::None,                                // current_plane
        IdeviceFFI::None,                                // entry_name
        IdeviceFFI::Some(std::string("IOPMPowerSource")) // entry_class
    );

    if (!ioreg_result.is_ok()) {
        qDebug() << "Failed to query IORegistry:"
                 << ioreg_result.unwrap_err().message.c_str();
        return;
    }

    // 4. Unwrap the result and handle the optional plist.
    auto plist_opt = std::move(ioreg_result).unwrap();
    if (plist_opt.is_some()) {
        // The caller of get_battery_info is responsible for freeing this plist.
        diagnostics = std::move(plist_opt).unwrap();
    }
}