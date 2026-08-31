// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import iDescriptor
import "." as App

Item {
    id: root

    readonly property var selectedDevice:
        App.DeviceContext.currentDestination === "device"
        ? App.DeviceContext.getDevice(App.DeviceContext.currentDestinationId)
        : null
    readonly property int selectedDeviceSection:
        root.selectedDevice ? root.selectedDevice.currentSection : 0

    function continueStartup() {
        NetworkDeviceProvider.startBrowsing()

        Qt.callLater(whatsNewDialog.showIfNeeded)
    }

    function startStartupFlow() {
        if (Qt.platform.os === "osx"
                && NetworkDeviceProvider.localNetworkPrivacyRequired
                && !settingsManager.local_network_onboarding_shown()) {
            localNetworkPermissionDialog.open()
            return
        }

        root.continueStartup()
    }

    WhatsNew {
        id: whatsNewDialog
    }

    LocalNetworkPermissionDialog {
        id: localNetworkPermissionDialog

        onContinueRequested: {
            settingsManager.set_local_network_onboarding_shown(true)
            localNetworkPermissionDialog.close()
            root.continueStartup()
        }
    }

    function destinationTitle() {
        switch (App.DeviceContext.currentDestination) {
        case "welcome":
            return qsTr("Welcome")
        case "apps":
            return qsTr("Apps")
        case "toolbox":
            return qsTr("Toolbox")
        case "jailbroken":
            return qsTr("Jailbroken")
        case "community":
            return qsTr("Community")
        case "donate":
            return qsTr("Donate")
        case "pendingDevice":
            return qsTr("Connecting…")
        case "recoveryDevice":
            return qsTr("Recovery Device")
        case "device":
            return root.selectedDevice && root.selectedDevice.info
                    ? root.selectedDevice.info.product_type : qsTr("Device")
        default:
            return ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppSidebar {
                id: sidebar

                visible: App.DeviceContext.sidebarVisible
                Layout.fillHeight: true
                Layout.minimumWidth: 180
                Layout.preferredWidth: Math.round(root.width * 0.2)
                Layout.maximumWidth: 260
                onToggleRequested: App.DeviceContext.sidebarVisible = false
            }

            Rectangle {
                visible: App.DeviceContext.sidebarVisible
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: App.Theme.sidebarDivider
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    radius: {
                        Qt.platform.os === "linux" && settingsManager.z_linux_window() ?  Theme.radius : 0
                    }
                    color: {
                        switch (Qt.platform.os) {
                            case "osx": return App.Theme.windowBackgroundMacOS
                            case "windows": return App.Theme.windowBackgroundWindows
                            default: return App.Theme.windowBackground
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48

                        SidebarToggleButton {
                            id: collapsedSidebarButton
                            visible: !App.DeviceContext.sidebarVisible
                            // The macOS traffic lights occupy x=20...74. Keep the
                            // collapsed-sidebar control beside them, not below them.
                            anchors.left: parent.left
                            anchors.leftMargin: Qt.platform.os === "osx" ? 84 : 10
                            anchors.top: parent.top
                            // anchors.topMargin: Qt.platform.os === "windows" ? 25 : 0
                            anchors.topMargin: Qt.platform.os !== "osx" ? 25 : 0
                            // anchors.verticalCenter: parent.verticalCenter
                            onClicked: App.DeviceContext.sidebarVisible = true
                        }

                        Text {
                            visible: App.DeviceContext.currentDestination !== "device"
                            anchors.left: collapsedSidebarButton.visible
                                          ? collapsedSidebarButton.right : parent.left
                            anchors.leftMargin: collapsedSidebarButton.visible ? 10 : 16
                            anchors.top: parent.top
                            anchors.topMargin: 25
                            // anchors.verticalCenter: parent.verticalCenter
                            text: root.destinationTitle()
                            color: App.Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            width: Math.min(180, implicitWidth)
                        }

                        // DeviceSectionTabs {
                        //     visible: App.DeviceContext.currentDestination === "device"
                        //     anchors.horizontalCenter: parent.horizontalCenter
                        //     // anchors.verticalCenter: parent.verticalCenter
                        //     anchors.top: parent.top
                        //     anchors.topMargin: Qt.platform.os === "windows" ? 25 : 0

                        //     currentSection: root.selectedDeviceSection
                        //     onSectionRequested: function(sectionIndex) {
                        //         App.DeviceContext.selectDeviceSection(sectionIndex)
                        //     }
                        // }
                    }

                    WorkspaceStack {
                        currentDestination: App.DeviceContext.currentDestination
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                }
            }
        }
    }

    Component.onCompleted: Qt.callLater(root.startStartupFlow)
}
