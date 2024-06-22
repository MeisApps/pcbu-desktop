import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    property alias selectBTList: selectBTDeviceList
    property alias selectBTListModel: selectBTDeviceListModel
    description: QI18n.Get('pairing_form_bt_select_desc')
    nextBtn.enabled: false
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Material.background
        ListView {
            anchors.fill: parent
            id: selectBTDeviceList
            model: ListModel {
                id: selectBTDeviceListModel
            }
            spacing: 5
            currentIndex: -1
            delegate: ItemDelegate {
                width: selectBTDeviceList.width
                height: 40
                background: Rectangle {
                    anchors.fill: parent
                    color: selectBTDeviceList.currentIndex === index ? Material.accent : Material.color(Material.Grey, Material.Shade700)
                    radius: 20
                }
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    Column {
                        Label {
                            text: name
                            color: Material.foreground
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: selectBTDeviceList.currentIndex = index
                }
            }
            focus: true
            onCurrentItemChanged: {
                let item = model.get(selectBTDeviceList.currentIndex);
                let data = PairingForm.GetData();
                data.bluetoothAddress = item.address;
                PairingForm.SetData(data);
                nextBtn.enabled = true;
            }
        }
    }
}
