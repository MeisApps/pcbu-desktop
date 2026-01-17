import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    description: QI18n.Get('pairing_form_method_type_desc')
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: window.color
        ColumnLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 25
            ButtonGroup {
                id: methodRadioGroup
                onClicked: function(button) {
                    let data = PairingForm.GetData();
                    data.pairingMethodType = button.methodStr;
                    PairingForm.SetData(data);
                }
            }
            ColumnLayout {
                RadioButton {
                    ButtonGroup.group: methodRadioGroup
                    property string methodStr: 'AUTO'
                    text: QI18n.Get('pairing_method_type_automatic_select')
                    checked: PairingForm.GetData().pairingMethodType === methodStr
                }
                Label {
                    Layout.preferredWidth: 500
                    Layout.leftMargin: 40
                    text: QI18n.Get('pairing_method_type_automatic_desc')
                    wrapMode: Label.WordWrap
                }
            }
            ColumnLayout {
                RadioButton {
                    ButtonGroup.group: methodRadioGroup
                    property string methodStr: 'MANUAL'
                    text: QI18n.Get('pairing_method_type_manual_select')
                    checked: PairingForm.GetData().pairingMethodType === methodStr
                }
                Label {
                    Layout.preferredWidth: 500
                    Layout.leftMargin: 40
                    text: QI18n.Get('pairing_method_type_manual_desc')
                    wrapMode: Label.WordWrap
                }
            }
        }
    }
}
