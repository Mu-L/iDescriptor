// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./base"
import "." as App

AnimatedDialog {
    id: dlg
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay

    width: 560
    height: 560

    property int currentIndex: 0
    property int currentMode: 0
    property int hoveredMode: -1
    readonly property int autoPageCount: 3
    readonly property string lockdownPath: QmlUtils.get_lockdown_path()

    function updateNav() {
        prevBtn.enabled = dlg.currentIndex > 0
        nextBtn.enabled = dlg.currentIndex < (dlg.autoPageCount - 1)
    }

    function platformDoneText() {
        return qsTr("You can now unplug the device. iDescriptor will connect to it automatically. (requires iOS 14 or later)")
    }

    function copyLockdownPath() {
        QmlUtils.copy_to_clipboard(dlg.lockdownPath)
    }

    onCurrentIndexChanged: updateNav()
    Component.onCompleted: updateNav()

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Item {
            id: segmentedControl
            Layout.fillWidth: true
            Layout.preferredHeight: 36

            Rectangle {
                anchors.fill: parent
                radius: App.Theme.sidebarCornerRadius
                color: App.Theme.softBg
                border.color: App.Theme.softBgBorder
                border.width: 1
            }

            Rectangle {
                id: selectedPill
                x: 4 + dlg.currentMode * ((segmentedControl.width - 8) / 2)
                y: 4
                width: (segmentedControl.width - 8) / 2
                height: segmentedControl.height - 8
                radius: App.Theme.sidebarCornerRadius
                color: App.Theme.controlFill

                Behavior on x {
                    NumberAnimation {
                        duration: App.Theme.fastAnimation
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on width {
                    NumberAnimation {
                        duration: App.Theme.fastAnimation
                        easing.type: Easing.OutCubic
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 0

                Repeater {
                    model: [qsTr("Auto Setup"), qsTr("Custom")]

                    delegate: Item {
                        required property int index
                        required property string modelData

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Rectangle {
                            anchors.fill: parent
                            radius: App.Theme.sidebarCornerRadius
                            visible: dlg.currentMode !== index && dlg.hoveredMode === index
                            color: App.Theme.hover
                        }

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: dlg.currentMode === index ? App.Theme.text : App.Theme.textMuted
                            font.pixelSize: 13
                            font.weight: dlg.currentMode === index ? Font.DemiBold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: dlg.hoveredMode = index
                            onExited: {
                                if (dlg.hoveredMode === index)
                                    dlg.hoveredMode = -1
                            }
                            onClicked: dlg.currentMode = index
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                id: autoPane
                anchors.fill: parent
                opacity: dlg.currentMode === 0 ? 1 : 0
                enabled: dlg.currentMode === 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: App.Theme.mediumAnimation
                        easing.type: Easing.OutCubic
                    }
                }

                Item {
                    id: autoPageArea
                    anchors.fill: parent
                    anchors.bottomMargin: 20

                    Item {
                        anchors.fill: parent
                        opacity: dlg.currentIndex === 0 ? 1 : 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: App.Theme.mediumAnimation
                                easing.type: Easing.OutCubic
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Item { Layout.fillHeight: true }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Connect your device")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: App.Theme.text
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Plug the device into this computer with a cable so iDescriptor can prepare wireless pairing.")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                lineHeight: 1.4
                                color: App.Theme.textMuted
                                font.pixelSize: 13
                            }

                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: "qrc:/resources/connect.png"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        opacity: dlg.currentIndex === 1 ? 1 : 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: App.Theme.mediumAnimation
                                easing.type: Easing.OutCubic
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Item { Layout.fillHeight: true }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Accept the pairing dialog")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: App.Theme.text
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Keep the device unlocked and tap Trust when iOS asks whether this computer is trusted.")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                lineHeight: 1.4
                                color: App.Theme.textMuted
                                font.pixelSize: 13
                            }

                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: "qrc:/resources/trust.png"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        opacity: dlg.currentIndex === 2 ? 1 : 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: App.Theme.mediumAnimation
                                easing.type: Easing.OutCubic
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Item { Layout.fillHeight: true }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Finish over Wi-Fi")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: App.Theme.text
                            }

                            Text {
                                Layout.fillWidth: true
                                text: dlg.platformDoneText()
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                lineHeight: 1.4
                                color: App.Theme.textMuted
                                font.pixelSize: 13
                            }

                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: "qrc:/resources/ios-version.png"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 12

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 7

                        Repeater {
                            model: dlg.autoPageCount

                            delegate: Rectangle {
                                required property int index

                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                radius: width / 2
                                color: index === dlg.currentIndex ? App.Theme.accent : App.Theme.textMuted
                                opacity: index === dlg.currentIndex ? 1 : 0.28
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item { Layout.fillWidth: true }

                        Button {
                            id: prevBtn
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            icon.source: "qrc:/resources/icons/material-symbols_arrow-left-alt.svg"
                            icon.width: 18
                            icon.height: 18
                            icon.color: enabled ? App.Theme.icon : Qt.rgba(App.Theme.textMuted.r, App.Theme.textMuted.g, App.Theme.textMuted.b, 0.45)
                            onClicked: {
                                if (dlg.currentIndex > 0)
                                    dlg.currentIndex -= 1
                            }

                            background: Item {
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    radius: width / 2
                                    visible: prevBtn.activeFocus
                                    color: "transparent"
                                    border.color: App.Theme.focus
                                    border.width: 2
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: width / 2
                                    color: prevBtn.down ? App.Theme.pressed
                                          : prevBtn.hovered ? App.Theme.hover
                                                            : App.Theme.controlFill
                                    border.color: App.Theme.controlStroke
                                    border.width: 1
                                }
                            }
                        }

                        Button {
                            id: nextBtn
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            icon.source: "qrc:/resources/icons/material-symbols_arrow-right-alt.svg"
                            icon.width: 18
                            icon.height: 18
                            icon.color: enabled ? App.Theme.icon : Qt.rgba(App.Theme.textMuted.r, App.Theme.textMuted.g, App.Theme.textMuted.b, 0.45)
                            onClicked: {
                                if (dlg.currentIndex < dlg.autoPageCount - 1)
                                    dlg.currentIndex += 1
                            }

                            background: Item {
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    radius: width / 2
                                    visible: nextBtn.activeFocus
                                    color: "transparent"
                                    border.color: App.Theme.focus
                                    border.width: 2
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: width / 2
                                    color: nextBtn.down ? App.Theme.pressed
                                          : nextBtn.hovered ? App.Theme.hover
                                                            : App.Theme.controlFill
                                    border.color: App.Theme.controlStroke
                                    border.width: 1
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Item {
                id: customPane
                anchors.fill: parent
                opacity: dlg.currentMode === 1 ? 1 : 0
                enabled: dlg.currentMode === 1

                Behavior on opacity {
                    NumberAnimation {
                        duration: App.Theme.mediumAnimation
                        easing.type: Easing.OutCubic
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16

                    Item { Layout.fillHeight: true }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Use a saved pairing file")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: App.Theme.text
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("You can use the 'Connect with pairing file' button to connect to a device. You have to have a valid pairing file and know the device IP address.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                        color: App.Theme.textMuted
                        font.pixelSize: 13
                    }


                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Pairing files are usually stored here:")
                        color: App.Theme.textMuted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Math.max(36, pathText.implicitHeight + 18)
                        radius: 9
                        color: App.Theme.controlFill
                        border.color: App.Theme.softBgBorder
                        border.width: 1

                        Text {
                            id: pathText
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: dlg.lockdownPath
                            color: App.Theme.text
                            elide: Text.ElideMiddle
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                            font.family: "monospace"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(dlg.lockdownPath)
                        }
                    }

                    Label {
                        visible: Qt.platform.os === "osx"
                        text: qsTr("You can run `sudo ls /var/db/lockdown` to see the pairing files you have on your Mac.")
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
