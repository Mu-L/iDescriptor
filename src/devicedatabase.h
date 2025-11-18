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

#ifndef DEVICEDATABASE_H
#define DEVICEDATABASE_H

#include <string>

struct DeviceDatabaseInfo {
    const char *modelIdentifier;
    const char *boardId;
    int boardNumber;
    int chipId;
    const char *marketingName;
    const char *displayName;
};

class DeviceDatabase
{
public:
    DeviceDatabase() = delete;

    static const DeviceDatabaseInfo *
    findByIdentifier(const std::string &identifier);
    static const DeviceDatabaseInfo *findByHwModel(const std::string &hwModel);
    static std::string parseRegionInfo(const std::string &code);

private:
    static const DeviceDatabaseInfo m_devices[];
};

#endif // DEVICEDATABASE_H
