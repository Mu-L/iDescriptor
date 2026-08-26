// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    required property string iconSource
    property int radius: 12 
    clip : true
    Layout.preferredWidth: 48
    Layout.preferredHeight: 48
    color: "transparent"

    // FIXME: hardcoded
    readonly property color shimmerBase: "#e5e7eb"
    readonly property color shimmerHighlight: "#f4f4f5"
    readonly property bool loading: iconImg.status === Image.Loading || iconImg.status === Image.Null

    // Actual icon 
    Image {
        id: iconImg
        anchors.fill: parent
        source: root.iconSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: loading
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
    }


    // base for placeholder 
    Rectangle {
        id: placeholder
        anchors.fill: parent
        color: root.shimmerBase
        visible: loading
        radius: root.radius

    }

    // shimmer
    Rectangle {
        id: shimmerBand
        visible: loading
        height: parent.height
        width: Math.round(parent.width * 0.65)
        y: 0
        x: -width

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.0) }
            GradientStop { position: 0.5; color: root.shimmerHighlight }
            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
        }
        opacity: 0.9

        // angle
        rotation: -12
        transformOrigin: Item.Center

        NumberAnimation on x {
            running: loading
            loops: Animation.Infinite
            duration: 1200
            from: -shimmerBand.width
            to: iconImg.width + shimmerBand.width
        }
    }


    Label {
        text : "iD"
        anchors.fill : parent
        horizontalAlignment : Text.AlignHCenter
        verticalAlignment : Text.AlignVCenter

    }

    
    OpacityMask {
        anchors.fill: parent
        source: iconImg
        maskSource: mask
    }


}
