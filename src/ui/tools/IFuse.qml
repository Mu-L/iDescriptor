// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../base"
import "../" as App

ToolWindow {
    id: root
    width: 560
    height: 330
    minimumWidth: 420
    minimumHeight: 280
    title: qsTr("iFuse Mount - iDescriptor")

    property string mountPath: ""
    property string mountRootPath: ""
    // TODO: Flathub rejected the use of flatpak-spawn --host for mounting devices
    // however we should keep conditional code in place
    readonly property bool flatpakBuild: iFuse.is_flatpak_build()
    property bool openedCurrentMount: false
    property var mountState: ({
        busy: false,
        mounted: false,
        mountPath: "",
        message: "",
        isError: false
    })

    function updateMountState() {
        root.mountState = iFuse.state_for_device(root.udid)
    }

    function deviceName() {
        if (root.device.info && root.device.info.marketing_name)
            return root.device.info.marketing_name
        if (root.device.info && root.device.info.product_type)
            return root.device.info.product_type
        return root.device.text || qsTr("Unknown Device")
    }

    function productType() {
        if (root.device.info && root.device.info.product_type)
            return root.device.info.product_type
        return root.deviceName()
    }

    function parentPath(path) {
        const normalized = String(path || "").replace(/\\/g, "/")
        const separator = normalized.lastIndexOf("/")
        return separator > 0 ? normalized.substring(0, separator) : normalized
    }

    Component.onCompleted: {
        // FIXME: skipped WinFsp DiagnoseDialog check from QWidget port.
        // The original code showed DiagnoseDialog when IsWinFspInstalled() != SERVICE_AVAILABLE on Windows.
        root.mountRootPath = iFuse.mount_root_path()
        root.mountPath = iFuse.default_mount_path(root.productType())
        root.updateMountState()
        stateView.viewState = StateView.State.Content
    }

    Connections {
        target: App.DeviceContext

        function onDeviceRemoved(removedUdid) {
            if (root.udid === removedUdid && root.mountState.mounted)
                iFuse.unmount_device_path(removedUdid, root.mountState.mountPath)
        }
    }

    FolderDialog {
        id: linuxFolderDialog
        title: qsTr("Select Mount Directory")
        options: root.flatpakBuild ? FolderDialog.DontUseNativeDialog : 0
        currentFolder: {
            const path = root.flatpakBuild ? root.mountRootPath : root.mountPath
            return path ? App.Helpers.toFileUrl(path) : ""
        }
        onAccepted: {
            const selectedPath = QmlUtils.url_to_path(selectedFolder)
            if (iFuse.is_mount_path_supported(selectedPath)) {
                root.mountPath = selectedPath
                return
            }

            App.Helpers.showError(
                root,
                qsTr("This folder cannot be used by the Flatpak build. Choose a subfolder inside %1.")
                    .arg(root.mountRootPath)
            )
        }
    }

    FileDialog {
        id: windowsMountDialog
        title: qsTr("Select Mount Directory")
        fileMode: FileDialog.SaveFile
        currentFolder: root.mountPath ? App.Helpers.toFileUrl(root.parentPath(root.mountPath)) : ""
        onAccepted: root.mountPath = QmlUtils.url_to_path(selectedFile)
    }

    StateView {
        id: stateView
        anchors.fill: parent
        viewState: StateView.State.Loading

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            Label {
                Layout.fillWidth: true
                text: qsTr("Mount %1's media as a drive on your PC.").arg(root.deviceName())
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                opacity: 0.72
                bottomPadding: 10
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.mountState.message && root.mountState.message.length > 0
                radius: 4
                color: root.mountState.isError ? "#ffe6e6" : "#e6ffe6"
                border.color: root.mountState.isError ? "#ffcccc" : "#ccffcc"
                implicitHeight: statusLabel.implicitHeight + 16

                Label {
                    id: statusLabel
                    anchors.fill: parent
                    anchors.margins: 8
                    text: root.mountState.message || ""
                    color: root.mountState.isError ? "#dd0000" : "#006600"
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 35
                    radius: 4
                    border.color: "#cccccc"
                    color: "transparent"

                    Label {
                        anchors.fill: parent
                        anchors.margins: 8
                        text: root.mountPath || qsTr("Mount directory will be shown here")
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: root.mountState.mounted && !root.mountState.busy
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: Qt.openUrlExternally(App.Helpers.toFileUrl(root.mountPath))
                    }
                }

                Button {
                    text: qsTr("Browse...")
                    enabled: !root.mountState.busy && !root.mountState.mounted
                    onClicked: {
                        if (Qt.platform.os === "windows")
                            windowsMountDialog.open()
                        else
                            linuxFolderDialog.open()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                implicitHeight: 40
                text: {
                    if (root.mountState.busy)
                        return root.mountState.mounted ? qsTr("Unmounting...") : qsTr("Mounting...")
                    return root.mountState.mounted ? qsTr("Unmount Device") : qsTr("Mount Device")
                }
                enabled: !root.mountState.busy && root.mountPath.length > 0
                onClicked: {
                    if (root.mountState.mounted)
                        iFuse.unmount_device_path(root.udid, root.mountState.mountPath)
                    else
                        iFuse.mount(root.udid, root.mountPath)
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    Connections {
        target: iFuse

        function onDeviceStateChanged(changedUdid) {
            if (changedUdid !== root.udid)
                return

            root.updateMountState()
            if (!root.mountState.mounted)
                root.openedCurrentMount = false

            if (root.mountState.mounted && !root.mountState.busy
                    && root.mountState.mountPath && !root.openedCurrentMount) {
                root.openedCurrentMount = true
                Qt.openUrlExternally(App.Helpers.toFileUrl(root.mountState.mountPath))
            }
        }
    }
}
