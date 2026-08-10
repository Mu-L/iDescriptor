// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import ".."

ToolWindow {
    id: root

    required property var info
    property var batteryInfo: ({
    })
    property bool loading: false
    readonly property string rawProductType: root.info && root.info["ProductType"] ? root.info["ProductType"] : ""
    readonly property real batteryLevel: Math.max(0, Math.min(100, Number(root.value("current_battery_level", 0))))
    readonly property real normalizedBatteryLevel: root.batteryLevel / 100
    readonly property bool isCharging: Boolean(root.value("is_charging", false))
    readonly property bool fullyCharged: Boolean(root.value("fully_charged", false))
    readonly property int healthPercent: Math.max(0, Math.min(100, parseInt(String(root.value("battery_health", "0"))) || 0))

    function value(key, fallback) {
        if (!root.batteryInfo)
            return fallback;

        const result = root.batteryInfo[key];
        if (result === undefined || result === null || result === "")
            return fallback;

        return result;
    }

    function capacityText(value) {
        const capacity = Number(value);
        return capacity > 0 ? qsTr("%1 mAh").arg(capacity) : qsTr("Unavailable");
    }

    function voltageText(value) {
        const millivolts = Number(value);
        return millivolts > 0 ? qsTr("%1 V").arg((millivolts / 1000).toFixed(1)) : qsTr("Unavailable");
    }

    function connectionText() {
        const connection = String(root.value("usb_connection_type", ""));
        if (!connection.length || connection.toLowerCase() === "unknown")
            return qsTr("Unavailable");

        if (connection.toLowerCase() === "usb type-c")
            return qsTr("USB-C");

        return connection;
    }

    function statusText() {
        if (root.fullyCharged)
            return qsTr("Fully Charged");

        return root.isCharging ? qsTr("Charging") : qsTr("Not Charging");
    }

    function requestBatteryInfo() {
        if (!root.rawProductType.length) {
            root.loading = false;
            stateView.errorText = qsTr("The device model is unavailable, so battery information cannot be refreshed.");
            stateView.viewState = StateView.State.Error;
            return ;
        }
        root.loading = true;
        stateView.viewState = StateView.State.Loading;
        root.device.service_manager.get_battery_info(root.rawProductType, info.ios_version_major);
    }

    title: qsTr("Battery - iDescriptor")
    width: 720
    height: 620
    minimumWidth: 620
    minimumHeight: 520

    onVisibleChanged: {
        if (visible)
            root.requestBatteryInfo()
    }

    Connections {
        function onBatteryInfoUpdated(updatedInfo) {
            if (!updatedInfo || !updatedInfo.DIAG_INFO) {
                root.loading = false;
                stateView.errorText = qsTr("The device returned incomplete battery information.");
                stateView.viewState = StateView.State.Error;
                return ;
            }
            root.batteryInfo = updatedInfo.DIAG_INFO;
            root.loading = false;
            stateView.viewState = StateView.State.Content;
        }

        function onBatteryInfoUpdateFailed(error) {
            console.error("Battery information refresh failed:", error);
            root.loading = false;
            stateView.errorText = qsTr("Battery information could not be refreshed. Make sure the device is connected and unlocked.");
            stateView.viewState = StateView.State.Error;
        }

        target: root.device.service_manager
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                spacing: 2

                Label {
                    text: qsTr("Battery")
                    color: Theme.text
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                }

                Label {
                    text: qsTr("Live power and capacity information")
                    color: Theme.textMuted
                    font.pixelSize: 12
                }

            }

            Item {
                Layout.fillWidth: true
            }

            IconToolButton {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                icon.source: "qrc:/resources/icons/ic_outline-refresh.svg"
                enabled: !root.loading
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("Refresh battery information")
                onClicked: root.requestBatteryInfo()
            }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.separator
        }

        StateView {
            id: stateView

            Layout.fillWidth: true
            Layout.fillHeight: true
            autoSwitchContent: false
            viewState: StateView.State.Loading
            errorText: qsTr("Battery information could not be loaded.")
            onRetryRequested: root.requestBatteryInfo()

            contentItem: ColumnLayout {
                anchors.fill: parent
                spacing: 14

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 250

                        Item {
                            id: verticalBattery

                            width: 116
                            height: 224
                            anchors.centerIn: parent

                            Rectangle {
                                id: batteryTerminal

                                width: 34
                                height: 9
                                radius: 4
                                anchors.top: parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: Theme.icon
                            }

                            Rectangle {
                                id: batteryBody

                                width: 104
                                height: 206
                                radius: 22
                                anchors.top: batteryTerminal.bottom
                                anchors.topMargin: -1
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: Theme.softBg
                                border.color: Theme.icon
                                border.width: 2

                                Rectangle {
                                    id: batteryClip

                                    anchors.fill: parent
                                    anchors.margins: 7
                                    radius: 15
                                    color: "transparent"
                                    clip: true

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: parent.height * root.normalizedBatteryLevel
                                        //FIXME: can we do better?
                                        radius: 15
                                        color: Theme.systemGreen

                                        Behavior on height {
                                            NumberAnimation {
                                                duration: Theme.fastAnimation
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }

                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("%1%").arg(Math.round(root.batteryLevel))
                                    color: root.batteryLevel >= 55 ? Theme.textSelected : Theme.text
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                }

                            }

                        }

                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 112
                            radius: 14
                            color: Theme.elevatedSurface
                            border.color: Theme.separator
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 5
                                ToolTip.visible: healthHelpHover.hovered
                                ToolTip.delay: 300
                                ToolTip.text: qsTr("Battery health is maximum charge capacity divided by design capacity, multiplied by 100 and capped at 100%.")

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: qsTr("Battery Health")
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                }

                                Label {
                                    text: root.value("battery_health", qsTr("Unavailable"))
                                    color: root.healthPercent >= 80 ? Theme.systemGreen : root.healthPercent >= 60 ? Theme.systemOrange : Theme.systemRed
                                    font.pixelSize: 30
                                    font.weight: Font.DemiBold
                                }

                                HoverHandler {
                                    id: healthHelpHover
                                }

                            }

                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 86
                            radius: 14
                            color: Theme.rowSurface
                            border.color: Theme.separator
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 12

                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: root.isCharging ? Theme.systemGreen : Theme.textMuted
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.statusText()
                                        color: Theme.text
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                    }

                                    Label {
                                        text: root.connectionText()
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                    }

                                }

                            }

                        }

                    }

                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        model: [{
                            "label": qsTr("Cycle Count"),
                            "value": String(root.value("cycle_count", 0))
                        }, {
                            "label": qsTr("Maximum Capacity"),
                            "value": root.capacityText(root.value("max_capacity", 0))
                        }, {
                            "label": qsTr("Design Capacity"),
                            "value": root.capacityText(root.value("design_capacity", 0))
                        }, {
                            "label": qsTr("Adapter Power"),
                            "value": Number(root.value("adapter_watts", 0)) > 0 ? qsTr("%1 W").arg(root.value("adapter_watts", 0)) : qsTr("Unavailable")
                        }, {
                            "label": qsTr("Adapter Voltage"),
                            "value": root.voltageText(root.value("adapter_voltage", 0))
                        }, {
                            "label": qsTr("Connection"),
                            "value": root.connectionText()
                        }]

                        delegate: Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            radius: 12
                            color: Theme.rowSurface
                            border.color: Theme.separator
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 11
                                spacing: 3

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                CopyableText {
                                    Layout.fillWidth: true
                                    text: modelData.value
                                    color: Theme.text
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                            }

                        }

                    }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 62
                    radius: 12
                    color: Theme.rowSurface
                    border.color: Theme.separator
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Label {
                            text: qsTr("Battery Serial Number")
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        PrivateText {
                            Layout.maximumWidth: 300
                            text: root.value("battery_serial_number", qsTr("Unavailable"))
                            color: Theme.text
                            elide: Text.ElideRight
                        }

                    }

                }

            }

        }

    }

}
