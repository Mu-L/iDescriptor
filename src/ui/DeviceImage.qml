// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Item {
    id: root

    implicitWidth: 175
    implicitHeight: 400

    required property var displayName
    property string mockupName: getMockupNameFromDisplayName(displayName)

    required property int iosVersion

    readonly property int cornerRadiusPx: 35

    readonly property bool isUnknown: mockupName === "unknown"
    readonly property bool useRoundedCorners: (mockupName === "x" || mockupName === "15" || mockupName === "16")

    function mockupSourceForName(name) {
        if (name === "iPad" || name === "unknown")
            return "qrc:/resources/ipad-mockups/ipad.png"
        return "qrc:/resources/iphone-mockups/iphone-" + name + ".png"
    }


    function getMockupNameFromDisplayName(displayName) {
        if (displayName.includes("iPhone 16") ||
            displayName.includes("iPhone 17")) {
            return "16";
        } else if (displayName.includes("iPhone 15") ||
                displayName.includes("iPhone 14")) {
            return "15";
        } else if (displayName.includes("iPhone X") ||
                displayName.includes("iPhone 11") ||
                displayName.includes("iPhone 12") ||
                displayName.includes("iPhone 13")) {
            return "x";
        } else if (displayName.includes("iPhone 6") ||
                displayName.includes("iPhone 7") ||
                displayName.includes("iPhone 8") ||
                displayName.includes("iPhone SE 2nd") ||
                displayName.includes("iPhone SE 3rd")) {
            return "6";
        } else if (displayName.includes("iPhone 5") ||
                displayName.includes("iPhone SE")) {
            return "5";
        } else if (displayName.includes("iPhone 4")) {
            return "4";
        } else if (displayName.includes("iPhone 3")) {
            return "3";
        } else if (displayName.includes("iPad")) {
            return "iPad";
        } else {
            return "unknown";
        }
    }

    function wallpaperSourceForIos(version) {
        if (version >= 27)
            return "qrc:/resources/ios-wallpapers/iphone-ios27.jpg"
        return `qrc:/resources/ios-wallpapers/iphone-ios${version}.png`
    }

    function screenRectForMockup(name, srcW, srcH) {
        if (name === "3")  return Qt.rect(145, 72, 209, 310)
        if (name === "4")  return Qt.rect(414, 181, 380, 548)
        if (name === "5")  return Qt.rect(27, 106, 304, 537)
        if (name === "6")  return Qt.rect(68, 348, 1279, 2270)
        if (name === "x")  return Qt.rect(245, 200, 2389, 5303)
        if (name === "15") return Qt.rect(15, 10, 337, 730)
        if (name === "16") return Qt.rect(17, 16, 333, 730)
        if (name === "iPad")    return Qt.rect(30, 30, 480, 690)
        if (name === "unknown") return Qt.rect(33, 36, 471, 680)

        return Qt.rect(srcW * 0.12, srcH * 0.08, srcW * 0.76, srcH * 0.84)
    }

    Image {
        id: mockup
        z: 10
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        source: root.mockupSourceForName(root.mockupName)
    }

    readonly property real paintedLeft: mockup.x + (mockup.width - mockup.paintedWidth) / 2
    readonly property real paintedTop:  mockup.y + (mockup.height - mockup.paintedHeight) / 2

    readonly property real srcW: (mockup.sourceSize.width  > 0 ? mockup.sourceSize.width  : mockup.implicitWidth)
    readonly property real srcH: (mockup.sourceSize.height > 0 ? mockup.sourceSize.height : mockup.implicitHeight)

    readonly property real scaleX: (srcW > 0 ? mockup.paintedWidth  / srcW : 1.0)
    readonly property real scaleY: (srcH > 0 ? mockup.paintedHeight / srcH : 1.0)

    readonly property rect screenRectSrc: screenRectForMockup(root.mockupName, srcW, srcH)
    readonly property rect screenRectPainted: Qt.rect(
        paintedLeft + screenRectSrc.x * scaleX,
        paintedTop  + screenRectSrc.y * scaleY,
        screenRectSrc.width  * scaleX,
        screenRectSrc.height * scaleY
    )

    Item {
        id: screenLayer
        z: 5
        x: root.screenRectPainted.x
        y: root.screenRectPainted.y
        width: root.screenRectPainted.width
        height: root.screenRectPainted.height

        Image {
            id: wallpaper
            anchors.fill: parent
            source: root.wallpaperSourceForIos(root.iosVersion)
            fillMode: Image.Stretch
            smooth: true
            visible: !root.useRoundedCorners
        }

        Rectangle {
            id: roundedMask
            anchors.fill: parent
            radius: root.cornerRadiusPx * Math.min(root.scaleX, root.scaleY)
            color: "white"
            visible: false
        }

        OpacityMask {
            id: roundedMaskedWallpaper
            anchors.fill: parent
            source: wallpaper
            maskSource: roundedMask
            visible: root.useRoundedCorners
        }

        // question mark for unknown devices
        Text {
            anchors.centerIn: parent
            visible: root.isUnknown
            text: "?"
            color: "white"
            font.bold: true
            font.pixelSize: Math.max(12, parent.width / 3)
            style: Text.Outline
            styleColor: "#96000000"
        }

        // time
        Text {
            id: timeText
            anchors.centerIn: parent
            text: Qt.formatTime(new Date(), "hh:mm")
            color: "white"
            font.weight: Font.Light
            font.pixelSize: Math.max(10, parent.width / 5)
        }

        Timer {
            interval: 60000
            running: true
            repeat: true
            onTriggered: timeText.text = Qt.formatTime(new Date(), "hh:mm")
        }
    }
}
