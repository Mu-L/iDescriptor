// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Effects
import QtQuick.Templates as T

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 6
    spacing: 6

    indicator: Rectangle {
        id: indicator
        x: control.text
           ? (control.mirrored ? control.leftPadding : control.width - width - control.rightPadding)
           : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 38
        implicitHeight: 22
        radius: implicitHeight / 2

        readonly property real downTintFactor: 1.05
        readonly property bool light: control.palette.windowText.hslLightness
                                      < control.palette.window.hslLightness

        color: indicator.light
               ? Qt.darker(control.checked ? control.palette.accent : "#d9d6d2", control.down ? downTintFactor : 1)
               : Qt.lighter(control.checked ? control.palette.accent : "#454545", control.down ? downTintFactor : 1)

        states: State {
            name: "checked"
            when: control.checked

            PropertyChanges {
                indicator.color: indicator.light
                                 ? control.checked ? control.palette.accent : "#d9d6d2"
                                 : control.checked ? control.palette.accent : "#454545"
            }
        }

        transitions: Transition {
            ColorAnimation {
                target: indicator
                property: "color"
                duration: 226
                easing.type: Easing.InOutQuad
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: "transparent"
            border.color: indicator.light
                          ? Qt.darker("#06000000", control.down ? indicator.downTintFactor : 1)
                          : Qt.lighter("#1affffff", control.down ? indicator.downTintFactor : 1)

            Rectangle {
                x: 1
                y: 1
                width: parent.width - 2
                height: parent.height - 2
                radius: parent.radius
                color: "transparent"
                border.color: indicator.light
                              ? Qt.darker("#02000000", control.down ? indicator.downTintFactor : 1)
                              : Qt.lighter("#04ffffff", control.down ? indicator.downTintFactor : 1)
            }
        }

        Rectangle {
            id: handle
            x: Math.max(1, Math.min(indicator.width - width - 1, control.visualPosition * indicator.width - width / 2))
            y: (indicator.height - height) / 2
            width: 20
            height: 20
            radius: 10
            color: indicator.light
                   ? Qt.darker(control.palette.base, control.down ? 1.05 : 1)
                   : Qt.lighter("#cdcbc9", control.down ? 1.05 : 1)

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                blurMax: 10
                shadowBlur: 0.2
                shadowScale: 0.92
                shadowOpacity: 1
            }

            Behavior on x {
                enabled: !control.down

                SmoothedAnimation {
                    velocity: 200
                }
            }
        }
    }

    contentItem: CheckLabel {
        leftPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: control.palette.windowText
    }
}
