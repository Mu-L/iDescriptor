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

#ifndef SERVICEMANAGER_H
#define SERVICEMANAGER_H

#include "iDescriptor.h"
#include <QDebug>
#include <functional>
#include <libimobiledevice/afc.h>
#include <mutex>
#include <optional>

/**
 * @brief Centralized manager for device service operations with thread safety
 *
 * This class provides thread-safe wrappers for all device operations to prevent
 * crashes when devices are unplugged during active operations. It uses a
 * per-device recursive mutex to ensure that device cleanup waits for all
 * operations to complete.
 */
class ServiceManager
{
public:
    template <typename T>
    static T executeOperation(iDescriptorDevice *device,
                              std::function<T(afc_client_t)> operation,
                              std::optional<afc_client_t> altAfc = std::nullopt)
    {
        if (!device) {
            return T{}; // Return default-constructed value for the type
        }

        std::lock_guard<std::recursive_mutex> lock(device->mutex);

        // Double-check device is still valid after acquiring lock
        if (!device->afcClient) {
            return T{};
        }

        if (altAfc && !*altAfc) {
            // altAfc was explicitly provided but is null, which is an
            // invalid state.
            return T{};
        }

        // Determine which client to use
        afc_client_t client = altAfc ? *altAfc : device->afcClient;
        return operation(client);
    }

    template <typename T>
    static T executeOperation(iDescriptorDevice *device,
                              std::function<T()> operation,
                              std::optional<afc_client_t> altAfc = std::nullopt)
    {
        if (!device) {
            return T{}; // Return default-constructed value for the type
        }

        std::lock_guard<std::recursive_mutex> lock(device->mutex);

        // Double-check device is still valid after acquiring lock
        if (!device->afcClient) {
            return T{};
        }

        if (altAfc && !*altAfc) {
            // altAfc was explicitly provided but is null, which is an
            // invalid state.
            return T{};
        }
        return operation();
    }

    template <typename T>
    static T executeOperation(iDescriptorDevice *device,
                              std::function<T()> operation, T failureValue,
                              std::optional<afc_client_t> altAfc = std::nullopt)
    {
        if (!device) {
            return failureValue;
        }

        std::lock_guard<std::recursive_mutex> lock(device->mutex);

        // Double-check device is still valid after acquiring lock
        if (!device->afcClient) {
            return failureValue;
        }

        if (altAfc && !*altAfc) {
            // altAfc was explicitly provided but is null, which is an
            // invalid state.
            return failureValue;
        }

        return operation();
    }

    static void
    executeOperation(iDescriptorDevice *device, std::function<void()> operation,
                     std::optional<afc_client_t> altAfc = std::nullopt)
    {
        if (!device) {
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(device->mutex);

        // Double-check device is still valid after acquiring lock
        if (!device->afcClient) {
            return;
        }

        if (altAfc && !*altAfc) {
            // altAfc was explicitly provided but is null, which is an
            // invalid state.
            return;
        }

        operation();
    }

    static afc_error_t
    executeAfcOperation(iDescriptorDevice *device,
                        std::function<afc_error_t(afc_client_t)> operation,
                        std::optional<afc_client_t> altAfc = std::nullopt)
    {
        try {
            if (!device) {
                return AFC_E_UNKNOWN_ERROR;
            }

            std::lock_guard<std::recursive_mutex> lock(device->mutex);

            // Double-check device is still valid after acquiring lock
            if (!device->afcClient) {
                return AFC_E_UNKNOWN_ERROR;
            }

            if (altAfc && !*altAfc) {
                // altAfc was explicitly provided but is null, which is an
                // invalid state.
                return AFC_E_INVALID_ARG;
            }

            // Determine which client to use
            afc_client_t client = altAfc ? *altAfc : device->afcClient;
            return operation(client);
        } catch (const std::exception &e) {
            qDebug() << "Exception in executeAfcOperation:" << e.what();
            return AFC_E_UNKNOWN_ERROR;
        }
    }

    // Specific AFC operation wrappers
    static afc_error_t
    safeAfcReadDirectory(iDescriptorDevice *device, const char *path,
                         char ***dirs,
                         std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcGetFileInfo(iDescriptorDevice *device, const char *path,
                       char ***info,
                       std::optional<afc_client_t> altAfc = std::nullopt);

    static afc_error_t
    safeAfcGetFileInfoPlist(iDescriptorDevice *device, const char *path,
                            plist_t *info,
                            std::optional<afc_client_t> altAfc = std::nullopt);

    static afc_error_t
    safeAfcFileOpen(iDescriptorDevice *device, const char *path,
                    afc_file_mode_t mode, uint64_t *handle,
                    std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcFileRead(iDescriptorDevice *device, uint64_t handle, char *data,
                    uint32_t length, uint32_t *bytes_read,
                    std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcFileWrite(iDescriptorDevice *device, uint64_t handle,
                     const char *data, uint32_t length, uint32_t *bytes_written,
                     std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcFileClose(iDescriptorDevice *device, uint64_t handle,
                     std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcFileSeek(iDescriptorDevice *device, uint64_t handle, int64_t offset,
                    int whence,
                    std::optional<afc_client_t> altAfc = std::nullopt);
    static afc_error_t
    safeAfcFileTell(iDescriptorDevice *device, uint64_t handle,
                    uint64_t *position,
                    std::optional<afc_client_t> altAfc = std::nullopt);

    // Utility functions
    static QByteArray safeReadAfcFileToByteArray(
        iDescriptorDevice *device, const char *path,
        std::optional<afc_client_t> altAfc = std::nullopt);
    static AFCFileTree
    safeGetFileTree(iDescriptorDevice *device, const std::string &path = "/",
                    bool checkDir = true,
                    std::optional<afc_client_t> altAfc = std::nullopt);
};

#endif // SERVICEMANAGER_H
