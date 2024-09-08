import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    description: QI18n.Get('pairing_form_password_desc')
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: window.color
        ColumnLayout {
            anchors.fill: parent
            GroupBox {
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.preferredWidth: parent.width / 1.8
                ColumnLayout {
                    anchors.fill: parent
                    ColumnLayout {
                        Layout.margins: 15
                        UserListModel {
                            id: userListModel
                        }

                        Label {
                            id: msAccountNoticeLabel
                            text: QI18n.Get('notice_ms_account')
                        }

                        RowLayout {
                            Label {
                                Layout.minimumWidth: 100
                                text: '%1:'.arg(QI18n.Get('user'))
                                verticalAlignment: Text.AlignVCenter
                            }
                            ColumnLayout {
                                ComboBox {
                                    Layout.fillWidth: true
                                    id: userComboBox
                                    model: userListModel
                                    textRole: 'userName'
                                    currentIndex: {
                                        for(let i = 0; i < model.size(); i++) {
                                            if(model.get(i).isCurrentUser)
                                                return i;
                                        }
                                        return 0;
                                    }
                                    onCurrentIndexChanged: {
                                        let currUser = userListModel.get(currentIndex);
                                        let data = PairingForm.GetData();
                                        data.userName = currUser.userName;
                                        PairingForm.SetData(data);
                                        msAccountNoticeLabel.visible = currUser.isMicrosoftAccount;
                                    }
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    id: userTextField
                                    text: ''
                                    visible: false
                                    onTextChanged: {
                                        let data = PairingForm.GetData();
                                        data.userName = userTextField.text;
                                        PairingForm.SetData(data);
                                    }
                                }
                                CheckBox {
                                    id: userEnterManualCheckBox
                                    text: QI18n.Get('manual_enter')
                                    checked: false
                                    onToggled: {
                                        let isChecked = userEnterManualCheckBox.checked;
                                        userComboBox.visible = !isChecked;
                                        userTextField.visible = isChecked;

                                        let data = PairingForm.GetData();
                                        data.isManualUserName = isChecked;
                                        if(isChecked) {
                                            data.userName = userTextField.text;
                                        } else {
                                            let currUser = userListModel.get(userComboBox.currentIndex);
                                            data.userName = currUser.userName;
                                            msAccountNoticeLabel.visible = currUser.isMicrosoftAccount;
                                        }
                                        PairingForm.SetData(data);
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Label {
                                Layout.minimumWidth: 100
                                text: '%1:'.arg(QI18n.Get('password'))
                                verticalAlignment: Text.AlignVCenter
                            }
                            TextField {
                                Layout.fillWidth: true
                                id: pairingPwTextField
                                echoMode: TextField.Password
                                onTextChanged: {
                                    let data = PairingForm.GetData();
                                    data.password = pairingPwTextField.text;
                                    PairingForm.SetData(data);
                                }
                                focus: true
                                Keys.onReturnPressed: PairingForm.OnNextClicked(viewLoader, window)
                                Keys.onEnterPressed: PairingForm.OnNextClicked(viewLoader, window)
                            }
                        }
                    }
                }
            }
        }
    }
    Component.onCompleted: {
        let currUser = userListModel.get(userComboBox.currentIndex);
        let data = PairingForm.GetData();
        data.userName = currUser.userName;
        PairingForm.SetData(data);
        msAccountNoticeLabel.visible = currUser.isMicrosoftAccount;
    }
}
