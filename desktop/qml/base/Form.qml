import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PulseUnlock

Rectangle {
    Layout.fillWidth: true
    Layout.fillHeight: true
    id: formRect
    color: window.color

    default property alias content: formRect.children
}
