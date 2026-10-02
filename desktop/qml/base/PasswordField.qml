import QtQuick
import QtQuick.Controls

TextField {
    id: passwordField
    property bool passwordVisible: false

    echoMode: passwordVisible ? TextField.Normal : TextField.Password
    rightPadding: visibilityButton.width + 8

    ToolButton {
        id: visibilityButton
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 40
        height: 40
        focusPolicy: Qt.NoFocus
        icon.source: passwordField.passwordVisible ? 'qrc:/res/icons/visibility_off.svg' : 'qrc:/res/icons/visibility.svg'
        icon.width: 20
        icon.height: 20
        onClicked: passwordField.passwordVisible = !passwordField.passwordVisible
    }
}
