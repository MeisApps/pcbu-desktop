import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PulseUnlock
import "qrc:/ui/base"

StepForm {
    description: QI18n.Get('pairing_form_device_type_desc')
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: window.color
        ColumnLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 25
            ButtonGroup {
                id: deviceTypeRadioGroup
                onClicked: function (button) {
                    let data = PairingForm.GetData();
                    data.deviceType = button.deviceTypeStr;
                    PairingForm.SetData(data);
                }
            }
            ColumnLayout {
                RadioButton {
                    ButtonGroup.group: deviceTypeRadioGroup
                    property string deviceTypeStr: 'ANDROID'
                    text: QI18n.Get('pairing_device_type_android_select')
                    checked: PairingForm.GetData().deviceType === deviceTypeStr
                }
                Label {
                    Layout.preferredWidth: 500
                    Layout.leftMargin: 40
                    text: QI18n.Get('pairing_device_type_android_desc')
                    wrapMode: Label.WordWrap
                }
            }
            ColumnLayout {
                RadioButton {
                    ButtonGroup.group: deviceTypeRadioGroup
                    property string deviceTypeStr: 'IOS'
                    text: QI18n.Get('pairing_device_type_ios_select')
                    checked: PairingForm.GetData().deviceType === deviceTypeStr
                }
                Label {
                    Layout.preferredWidth: 500
                    Layout.leftMargin: 40
                    text: QI18n.Get('pairing_device_type_ios_desc')
                    wrapMode: Label.WordWrap
                }
            }
        }
    }
}
