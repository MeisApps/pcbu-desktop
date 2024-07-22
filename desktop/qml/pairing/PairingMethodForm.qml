import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    description: QI18n.Get('pairing_form_method_desc')
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
                    data.pairingMethod = button.methodStr;
                    PairingForm.SetData(data);
                }
            }
            ColumnLayout {
                RadioButton {
                    ButtonGroup.group: methodRadioGroup
                    property string methodStr: 'TCP'
                    text: QI18n.Get('pairing_method_tcp_select')
                    checked: PairingForm.GetData().pairingMethod === methodStr
                }
                Label {
                    text: QI18n.Get('pairing_method_tcp_desc')
                }
            }
            ColumnLayout {
                enabled: PairingForm.HasBluetooth()
                RadioButton {
                    ButtonGroup.group: methodRadioGroup
                    property string methodStr: 'BLUETOOTH'
                    text: QI18n.Get('pairing_method_bt_select')
                    checked: PairingForm.GetData().pairingMethod === methodStr
                }
                Label {
                    text: QI18n.Get('pairing_method_bt_desc')
                }
            }
            /*ColumnLayout {
                RadioButton {
                    ButtonGroup.group: methodRadioGroup
                    property string methodStr: 'CLOUD_TCP'
                    text: QI18n.Get('pairing_method_cloud_tcp_select')
                    checked: PairingForm.GetData().pairingMethod === methodStr
                }
                Label {
                    text: QI18n.Get('pairing_method_cloud_tcp_desc')
                }
            }*/
        }
    }
}
