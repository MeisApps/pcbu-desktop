import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import PCBioUnlock

ApplicationWindow {
    property string deviceId: ""

    id: unlockTestWindow
    width: 600
    height: 300
    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height
    title: QI18n.Get('unlock_test')
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Label {
                id: lblUnlockMsg
                anchors.centerIn: parent
                font.pixelSize: 28
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                text: QI18n.Get('initializing')
            }
        }
        Button {
            id: btnUnlockAction
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            Layout.preferredHeight: 56
            text: QI18n.Get('cancel')
            onClicked: {
                if(UnlockTestWindow.IsRunning()) {
                    UnlockTestWindow.StopUnlock(unlockTestWindow)
                } else {
                    UnlockTestWindow.StartUnlock(unlockTestWindow, deviceId)
                }
            }
        }
    }

    Component.onCompleted: {
        UnlockTestWindow.StartUnlock(unlockTestWindow, deviceId)
    }
    onClosing: {
        UnlockTestWindow.StopUnlock(unlockTestWindow)
    }

    function setUnlockMessage(text) {
        lblUnlockMsg.text = text
    }
    function setUnlockButtonText(text) {
        btnUnlockAction.text = text
    }
}
