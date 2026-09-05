// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../base"
import ".." as App

ToolWindow {
    id: root
    width: 720
    height: 610
    minimumWidth: 600
    minimumHeight: 500
    title: qsTr("Wired Gallery Import - iDescriptor")

    property bool auto_close: true
    property real uploadedBytes: 0
    property real totalBytes: 0
    property string currentFile: ""

    readonly property var compatibleExtensions: [
        "jpg", "jpeg", "png", "heic", "heif", "gif", "tif", "tiff", "bmp", "dng",
        "mov", "mp4", "m4v", "3gp"
    ]

    function fileName(path) {
        const normalized = path.replace(/\\/g, "/")
        return normalized.substring(normalized.lastIndexOf("/") + 1)
    }

    function compatible(path) {
        const dot = path.lastIndexOf(".")
        return dot >= 0 && compatibleExtensions.indexOf(path.substring(dot + 1).toLowerCase()) >= 0
    }

    function contains(path) {
        for (let i = 0; i < filesModel.count; ++i) {
            if (filesModel.get(i).path === path)
                return true
        }
        return false
    }

    function addFiles(urls, replace) {
        if (replace)
            filesModel.clear()
        for (let i = 0; i < urls.length; ++i) {
            const path = QmlUtils.url_to_path(urls[i])
            if (path.length && compatible(path) && !contains(path))
                filesModel.append({ path: path, name: fileName(path) })
        }
    }

    function selectedPaths() {
        const result = []
        for (let i = 0; i < filesModel.count; ++i)
            result.push(filesModel.get(i).path)
        return result
    }

    component Card: Rectangle {
        radius: 16
        color: App.Theme.groupedBackground
        border.color: App.Theme.softBgBorder
        border.width: 1
    }

    ListModel { id: filesModel }

    FileDialog {
        id: picker
        title: qsTr("Select Photos and Videos")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Photos and Videos (*.jpg *.jpeg *.png *.heic *.heif *.gif *.tif *.tiff *.bmp *.dng *.mov *.mp4 *.m4v *.3gp)"),
            qsTr("All Files (*)")
        ]
        onAccepted: root.addFiles(selectedFiles, true)
    }

    Connections {
        target: wiredGalleryImportBackend
        function onUploadProgress(fileName, bytesUploaded, bytesTotal) {
            root.currentFile = fileName
            root.uploadedBytes = bytesUploaded
            root.totalBytes = bytesTotal
        }
        function onErrorOccurred(message) {
            console.error("[WiredGalleryImport]", message)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: App.Theme.windowBackground

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    width: 48
                    height: 48
                    radius: 13
                    color: Qt.rgba(App.Theme.accent.r, App.Theme.accent.g, App.Theme.accent.b, 0.14)
                    Label {
                        anchors.centerIn: parent
                        text: "↧"
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        color: App.Theme.accent
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: qsTr("Import to Photos over USB")
                        font.pixelSize: 21
                        font.weight: Font.DemiBold
                        color: App.Theme.text
                    }
                    Label {
                        text: qsTr("Files are verified, added to Photos, and grouped in the iDescriptor album.")
                        color: App.Theme.textMuted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 104

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("iDescriptor Companion")
                            font.weight: Font.DemiBold
                            color: App.Theme.text
                        }
                        Label {
                            text: wiredGalleryImportBackend.state.detail || qsTr("Ready")
                            color: wiredGalleryImportBackend.state.phase === "error" || wiredGalleryImportBackend.state.phase === "failed"
                                   ? App.Theme.systemRed : App.Theme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    BusyIndicator {
                        running: wiredGalleryImportBackend.state.running === true
                        visible: running
                    }

                    Button {
                        visible: wiredGalleryImportBackend.state.phase === "connectionRequired"
                        text: qsTr("Reconnect")
                        onClicked: wiredGalleryImportBackend.reconnect()
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Selected media")
                            font.weight: Font.DemiBold
                            color: App.Theme.text
                        }
                        Label {
                            text: qsTr("%1 item(s)").arg(filesModel.count)
                            color: App.Theme.textMuted
                        }
                        Button {
                            text: qsTr("Choose…")
                            enabled: wiredGalleryImportBackend.state.running !== true
                            onClicked: picker.open()
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: filesModel
                        spacing: 4

                        delegate: Rectangle {
                            required property int index
                            required property string name
                            width: ListView.view.width
                            height: 42
                            radius: 9
                            color: index % 2 ? "transparent" : App.Theme.controlFill
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                Label {
                                    Layout.fillWidth: true
                                    text: name
                                    color: App.Theme.text
                                    elide: Text.ElideMiddle
                                }
                                ToolButton {
                                    text: "×"
                                    enabled: wiredGalleryImportBackend.state.running !== true
                                    onClicked: filesModel.remove(index)
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: filesModel.count === 0
                            text: qsTr("Choose or drop photos and videos here")
                            color: App.Theme.textMuted
                        }

                        DropArea {
                            anchors.fill: parent
                            enabled: wiredGalleryImportBackend.state.running !== true
                            onDropped: function(drop) {
                                if (drop.hasUrls) {
                                    root.addFiles(drop.urls, false)
                                    drop.acceptProposedAction()
                                }
                            }
                        }
                    }
                }
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: wiredGalleryImportBackend.state.phase === "uploading" || wiredGalleryImportBackend.state.phase === "cancelling"
                from: 0
                to: Math.max(1, root.totalBytes)
                value: root.uploadedBytes
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.currentFile.length && wiredGalleryImportBackend.state.phase === "uploading"
                          ? qsTr("Uploading %1").arg(root.currentFile)
                          : wiredGalleryImportBackend.state.totalItems > 0
                            ? qsTr("%1 imported, %2 failed").arg(wiredGalleryImportBackend.state.completedItems).arg(wiredGalleryImportBackend.state.failedItems)
                            : qsTr("Keep Companion open while the import is running.")
                    color: App.Theme.textMuted
                    elide: Text.ElideMiddle
                }

                Button {
                    visible: wiredGalleryImportBackend.state.canCancel === true
                    text: qsTr("Cancel")
                    onClicked: wiredGalleryImportBackend.cancel()
                }

                Button {
                    text: qsTr("Import")
                    highlighted: true
                    enabled: filesModel.count > 0 && wiredGalleryImportBackend.state.running !== true
                    onClicked: {
                        root.uploadedBytes = 0
                        root.totalBytes = 0
                        root.currentFile = ""
                        wiredGalleryImportBackend.start(root.udid, root.device.connectionId, root.selectedPaths())
                    }
                }
            }
        }
    }
}
