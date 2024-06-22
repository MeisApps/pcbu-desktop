import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

Form {
    Rectangle {
        anchors.fill: parent
        color: window.color
        ColumnLayout {
            id: loadProgressContainer
            anchors.top: parent.top
            anchors.topMargin: parent.height / 3
            width: parent.width
            Label {
                id: loadProgressLabel
                text: QI18n.Get('please_wait')
            }
            ProgressBar {
                Layout.fillWidth: true
                id: loadProgressBar
                indeterminate: true
                to: 100
            }
        }

        ColumnLayout {
            anchors.top: loadProgressContainer.bottom
            anchors.topMargin: 15
            width: parent.width
            height: parent.height - loadProgressContainer.y - loadProgressContainer.height
            TextArea {
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: loadOutputTextArea
                readOnly: true
            }
            Button {
                id: loadOkButton
                Layout.minimumWidth: 100
                Layout.minimumHeight: 50
                Layout.alignment: Qt.AlignRight
                text: "Ok"
                enabled: false
                onClicked: MainWindow.Show(viewLoader)
            }
        }
    }

    property alias progressLbl: loadProgressLabel
    property alias progressBar: loadProgressBar
    property alias okBtn: loadOkButton
    property alias outputTxtArea: loadOutputTextArea
}
