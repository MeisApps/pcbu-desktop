import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    description: QI18n.Get('pairing_form_qr_desc')
    nextTitle: QI18n.Get('main_menu')
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: window.color
        RowLayout {
            anchors.fill: parent
            Image {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignCenter
                fillMode: Image.PreserveAspectFit
                source: PairingForm.GetQRImage()
                sourceSize: Qt.size(qrImage.sourceSize.width * 5, qrImage.sourceSize.height * 5)
                Image {
                    id: qrImage
                    source: parent.source
                    width: 0
                    height: 0
                }
            }
        }
    }
}
