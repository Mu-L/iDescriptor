// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./base"

Item {
    id : root
    required property var info
    required property var device
    required property real galleryUsage
    required property bool galleryUsageResolved
    readonly property int contentMargin: 10
    readonly property int contentMaxWidth: 1040
    readonly property real diskUsageWidthRatio: 0.8
    property var batteryInfoWindow: null

    function v(key, fallback) {
        if (!info) return fallback
        const val = info[key]
        if (val === undefined || val === null || val === "") return fallback
        return val
    }

    function activationStateColor(state) {
        if (state === "Activated" || state === "WildcardActivated")
            return Theme.systemGreen
        if (state === "FactoryActivated")
            return Theme.systemOrange
        return Theme.systemRed
    }

    function activationStateText(state) {
        switch (state) {
            case "Activated":
            case "WildcardActivated":
                return "Activated"
            case "FactoryActivated":
                return "Factory Activated"
            case "Unactivated":
                return "Unactivated"
            default:
                return state
        }
    }

    function refreshBatteryInfo() {
        const rawProductType = v("ProductType", "")
        if (rawProductType.length > 0)
            root.device.service_manager.get_battery_info(rawProductType, info.ios_version_major)
    }

    function updateBatteryInfo(updatedInfo) {
        if (!updatedInfo || !updatedInfo.DIAG_INFO)
            return

        const nextInfo = Object.assign({}, root.info)
        nextInfo.DIAG_INFO = updatedInfo.DIAG_INFO
        root.info = nextInfo
    }

    function openBatteryInfo() {
        if (batteryInfoWindow) {
            batteryInfoWindow.show()
            batteryInfoWindow.raise()
            batteryInfoWindow.requestActivate()
            return
        }

        const comp = Qt.createComponent("./BatteryInfo.qml")
        if (comp.status !== Component.Ready) {
            console.error("Failed to load BatteryInfo:", comp.errorString())
            return
        }

        const win = comp.createObject(root, {
            udid: root.v("UniqueDeviceID", ""),
            device: root.device,
            info: Qt.binding(function() { return root.info })
        })

        if (!win) {
            console.error("Failed to create BatteryInfo:", comp.errorString())
            return
        }

        batteryInfoWindow = win
        win.closing.connect(function(closeEvent) {
            if (!closeEvent.accepted)
                return

            if (root.batteryInfoWindow === win)
                root.batteryInfoWindow = null
            win.destroy(0)
        })
        win.show()
        win.raise()
        win.requestActivate()
    }

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: root.refreshBatteryInfo()
    }

    Connections {
        target: root.device.service_manager

        function onBatteryInfoUpdated(updatedInfo) {
            root.updateBatteryInfo(updatedInfo)
        }
    }

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(root.contentMargin, (parent.height - implicitHeight) / 2)
        width: Math.min(Math.max(0, parent.width - root.contentMargin * 2), root.contentMaxWidth)
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            ColumnLayout {
                DeviceImage {
                    iosVersion: info ? info.ios_version_major : 0
                    displayName: v("product_type", qsTr("Unknown Device"))
                }
                RowLayout {
                    id: deviceActions
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    IconToolButton {
                        id: shutdownButton
                        icon.source: "qrc:/resources/icons/ic_outline-power-settings-new.svg"
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("Shut down device")
                        onClicked: Toolbox.toolClicked(11, true, false)
                    }
                    IconToolButton {
                        id: restartButton
                        icon.source: "qrc:/resources/icons/ic_twotone-restart-alt.svg"
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("Restart device")
                        onClicked: Toolbox.toolClicked(10, true, false)
                    }
                    IconToolButton {
                        id: recoveryButton
                        icon.source: "qrc:/resources/icons/hugeicons_wrench-01.svg"
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("Enter recovery mode")
                        onClicked: Toolbox.toolClicked(12, true, false)
                    }
                }
            }

            ColumnLayout {
                id: detailsColumn
                spacing: 20
                Layout.fillWidth: true

                SectionBox {
                    Layout.fillWidth: true
                    padding: 6
                    leftPadding: 10
                    rightPadding: 10

                    RowLayout {
                        spacing: 15

                        CopyableText {
                            text: v("product_type", qsTr("Unknown Device"))
                            font.bold: false
                            elide: Text.ElideRight
                        }

                        CopyableText {
                            horizontalPadding: 4
                            verticalPadding: 4
                            backgroundColor: Theme.accent
                            backgroundRadius: 13
                            text: {
                                const totalDiskCapacity = v("TotalDiskCapacity", null)
                                if (totalDiskCapacity === null) return ""
                                const gb = totalDiskCapacity / (1000 * 1000 * 1000)
                                if (gb >= 1000) {
                                    const tb = gb / 1024
                                    return tb.toFixed(1) + " TB"
                                } else {
                                    return gb.toFixed(0) + " GB"
                                }
                            }

                            color: Theme.textSelected
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            spacing: 3.5

                            CopyableText {
                                text: info.DIAG_INFO.current_battery_level + "%"
                                color: palette.text
                                visible: info.DIAG_INFO.is_charging
                            }

                            BatteryIndicator {
                                value: info.DIAG_INFO.current_battery_level
                                isCharging: info.DIAG_INFO.is_charging
                            }
                        }

                        CopyableText {
                            visible: info.DIAG_INFO.adapter_watts > 0
                            text: {
                                const watts = info.DIAG_INFO.adapter_watts
                                let text = ""
                                switch (info.DIAG_INFO.usb_connection_type) {
                                    case "usb type-c":
                                        text = watts + "W/USB-C"
                                        break
                                    default:
                                        text = watts + "W/USB"
                                }
                                return text
                            }
                            color: palette.text
                        }

                        Label {
                            text: qsTr("Wireless")
                            visible: info.DIAG_INFO.adapter_watts < 1 && info.is_wireless
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    implicitHeight: grid.implicitHeight + 20
                    // implicitHeight: grid.implicitHeight

                    SectionBox {
                        anchors.fill: parent
                        z: -1
                    }

                    GridLayout {
                        id: grid
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 8
                        anchors.fill: parent
                        anchors.margins: 10

                        // Left: iOS Version; Right: Hardware Model
                        Label { text: qsTr("iOS Version:"); font.bold: false }
                        CopyableText { text: v("ProductVersion", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Hardware Model:"); font.bold: false }
                        CopyableText { text: v("HardwareModel", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: Device Name; Right: Region
                        Label { text: qsTr("Device Name:"); font.bold: false }
                        CopyableText { text: v("DeviceName", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Region:"); font.bold: false }
                        CopyableText { text: v("region", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: Activation State; Right: Hardware Platform
                        Label { text: qsTr("Activation State:"); font.bold: false }
                        CopyableText {
                            text: root.activationStateText(v("ActivationState", qsTr("Unknown")))
                            color: root.activationStateColor(text)
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label { text: qsTr("Hardware Platform:"); font.bold: false }
                        CopyableText { text: v("HardwarePlatform", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: Device Class; Right: Firmware Version
                        Label { text: qsTr("Device Class:"); font.bold: false }
                        CopyableText { text: v("DeviceClass", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Firmware Version:"); font.bold: false }
                        CopyableText { text: v("FirmwareVersion", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: Jailbroken; Right: Battery Health
                        Label { text: qsTr("Jailbroken:"); font.bold: false }
                        CopyableText { text: v("Jailbroken", false) ? qsTr("Yes") : qsTr("No"); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Battery Health:"); font.bold: false }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            CopyableText {
                                text: root.info.DIAG_INFO.battery_health
                                elide: Text.ElideRight
                            }

                            Button {
                                text: qsTr("More")
                                font.pixelSize: 11
                                leftPadding: 8
                                rightPadding: 8
                                topPadding: 3
                                bottomPadding: 3
                                implicitHeight: 26
                                implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding + 5
                                onClicked: root.openBatteryInfo()
                            }
                        }

                        // Left: Model Number; Right: Production Device
                        Label { text: qsTr("Model Number:"); font.bold: false }
                        CopyableText { text: v("ModelNumber", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Production Device:"); font.bold: false }
                        CopyableText { text: v("ProductionDevice", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: CPU Architecture; Right: Serial Number
                        Label { text: qsTr("CPU Architecture:"); font.bold: false }
                        CopyableText { text: v("CPUArchitecture", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("Serial Number:"); font.bold: false }
                        PrivateText { text: v("SerialNumber", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }

                        // Left: Build Version; Right: IMEI
                        Label { text: qsTr("Build Version:"); font.bold: false }
                        CopyableText { text: v("BuildVersion", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: qsTr("IMEI:"); font.bold: false }
                        PrivateText { text: v("InternationalMobileEquipmentIdentity", qsTr("Unknown")); elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }


                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: -detailsColumn.spacing + 2
                    Layout.leftMargin: 10
                    spacing: 0

                    Label {
                        text: qsTr("UDID:")
                        font.pixelSize: 10
                        color: Theme.textMuted
                    }

                    PrivateText {
                        text: v("UniqueDeviceID", qsTr("Unknown"))
                        color: Theme.textMuted
                    }
                }

                    DiskUsage {
                        Layout.fillWidth: true
                        device: root.device
                        galleryUsage: root.galleryUsage
                        galleryUsageResolved: root.galleryUsageResolved
                    }
            }
        }

    }
}
