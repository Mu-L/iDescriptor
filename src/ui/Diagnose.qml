// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./base"

Item {
    id: root

    property var diagnoseState: DiagnoseImpl.state
    property bool autoCheck: true
    property string focusedDependencyId: ""
    property string message: ""

    function openDiagnostics() {
        diagnosticsDialog.open()
    }

    function openDiagnosticsFor(dependencyId, message) {
        root.focusedDependencyId = dependencyId
        root.message = message || ""
        diagnosticsDialog.open()
        root.scheduleCheck()
    }

    function focusSelectedDependency() {
        if (root.focusedDependencyId.length === 0 || !root.diagnoseState.items)
            return

        for (let index = 0; index < root.diagnoseState.items.length; ++index) {
            if (root.diagnoseState.items[index].id === root.focusedDependencyId) {
                dependencyList.currentIndex = index
                dependencyList.positionViewAtIndex(index, ListView.Center)
                return
            }
        }
    }

    function colorForKind(kind) {
        if (kind === "ok")
            return "#16a34a"
        if (kind === "warning")
            return "#d97706"
        if (kind === "error")
            return "#dc2626"
        return "#6b7280"
    }

    function statusText(modelData) {
        if (modelData.availability === 0)
            return qsTr("Installed")
        if (modelData.availability === 1)
            return qsTr("Installed, not running")
        if (modelData.availability === 2)
            return qsTr("Missing")
        return qsTr("Unable to check")
    }

    function actionText(modelData) {
        if (modelData.actionMode === "instructions")
            return qsTr("View Instructions")
        if (modelData.id === "udev_rules")
            return qsTr("View Instructions")
        if (modelData.availability === 1)
            return qsTr("Start")
        return qsTr("Install")
    }

    function updateStateView() {
        if (diagnoseState.error && diagnoseState.error.length > 0)
            diagnosticsState.viewState = StateView.State.Error
        else if (diagnoseState.checking)
            diagnosticsState.viewState = StateView.State.Loading
        else
            diagnosticsState.viewState = StateView.State.Content
    }

    function scheduleCheck() {
        diagnosticsState.viewState = StateView.State.Loading
        delayedCheckTimer.restart()
    }

    onDiagnoseStateChanged: {
        updateStateView()

        if (diagnoseState.notice && diagnoseState.notice.length > 0)
            noticeDialog.open()

        if (!diagnoseState.checking)
            Qt.callLater(root.focusSelectedDependency)
    }

    Component.onCompleted: {
        if (root.autoCheck)
            scheduleCheck()
    }

    Timer {
        id: delayedCheckTimer
        interval: 350
        repeat: false
        onTriggered: DiagnoseImpl.check()
    }

    Dialog {
        id: noticeDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 340
        title: qsTr("Dependency Check")
        standardButtons: Dialog.Ok
        onAccepted: Qt.callLater(DiagnoseImpl.clear_notice)
        onRejected: Qt.callLater(DiagnoseImpl.clear_notice)

        TextEdit {
            readOnly: true
            selectByMouse: true
            persistentSelection: true
            color: palette.text
            width: 300
            text: root.diagnoseState.notice || ""
            wrapMode: Text.WordWrap
        }
    }

    AnimatedDialog {
        id: diagnosticsDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("Diagnostics")
        width: 620
        height: 500
        standardButtons: Dialog.Close
        onOpened: {
            root.updateStateView()
            Qt.callLater(root.focusSelectedDependency)
        }

        contentItem: StateView {
            id: diagnosticsState
            implicitWidth: 580
            implicitHeight: 390
            autoSwitchContent: false
            retryable: true
            errorText: root.diagnoseState.error || qsTr("Unable to check system dependencies.")
            onRetryRequested: root.scheduleCheck()

            contentItem: SectionBox {
                anchors.fill: parent
                title: qsTr("Dependency Check")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            Layout.fillWidth: true
                            text: root.diagnoseState.summary || qsTr("Checking system dependencies...")
                            color: root.colorForKind(root.diagnoseState.summaryKind)
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Button {
                            text: qsTr("Refresh")
                            enabled: !root.diagnoseState.checking
                            onClicked: root.scheduleCheck()
                        }
                    }

                    ListView {
                        id: dependencyList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        interactive: contentHeight > height
                        model: root.diagnoseState.items || []
                        spacing: 8

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 78
                            radius: 7
                            color: "transparent"
                            border.color: modelData.id === root.focusedDependencyId
                                          ? diagnosticsDialog.palette.highlight
                                          : "transparent"
                            border.width: modelData.id === root.focusedDependencyId ? 2 : 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10


                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: "white"
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            visible: modelData.optional
                                            text: qsTr("Optional")
                                            color: "#94a3b8"
                                            font.pixelSize: 11
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        color: "#94a3b8"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.statusText(modelData)
                                        color: root.colorForKind(modelData.statusKind)
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }
                                }

                                BusyIndicator {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    visible: root.diagnoseState.installingId === modelData.id
                                    running: visible
                                }

                                Button {
                                    visible: modelData.actionVisible && root.diagnoseState.installingId !== modelData.id
                                    enabled: !root.diagnoseState.checking && root.diagnoseState.installingId.length === 0
                                    text: root.actionText(modelData)
                                    onClicked: {
                                        if (modelData.actionMode === "instructions")
                                            Qt.openUrlExternally(modelData.documentationUrl)
                                        else
                                            DiagnoseImpl.install(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.message.length > 0
                        text: root.message
                        color: "#dc2626"
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
