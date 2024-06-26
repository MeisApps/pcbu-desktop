import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import PCBioUnlock
import 'qrc:/ui/base'

Form {
    RowLayout {
        id: settingsLayout
        anchors.fill: parent
        GroupBox {
            Layout.preferredWidth: parent.width / 1.8
            Layout.preferredHeight: parent.height / 1.5
            title: QI18n.Get('common_settings')
            GridLayout {
                anchors.fill: parent
                anchors.margins: 10
                columns: 4
                rows: 2
                columnSpacing: 5

                Label {
                    Layout.row: 0
                    Layout.column: 0
                    text: '%1:'.arg(QI18n.Get('language'))
                }
                Label {
                    Layout.row: 1
                    Layout.column: 0
                    text: '%1:'.arg(QI18n.Get('ip_address'))
                }
                Label {
                    Layout.row: 2
                    Layout.column: 0
                    text: '%1:'.arg(QI18n.Get('server_port'))
                }

                ComboBox { // Lang
                    Layout.fillWidth: true
                    Layout.row: 0
                    Layout.column: 1
                    model: ListModel {
                        id: langComboBoxModel
                        ListElement { text: "Auto"; val: "auto" }
                        ListElement { text: "English"; val: "en" }
                        ListElement { text: "Deutsch"; val: "de" }
                    }
                    textRole: 'text'
                    currentIndex: {
                        let currLang = SettingsForm.GetSettings().language;
                        for(let i = 0; i < langComboBoxModel.count; i++) {
                            if(currLang === langComboBoxModel.get(i).val) {
                                return i;
                            }
                        }
                        return 0;
                    }
                    onCurrentIndexChanged: {
                        let settings = SettingsForm.GetSettings();
                        settings.language = langComboBoxModel.get(currentIndex).val;
                        SettingsForm.SetSettings(settings);
                    }
                }
                ColumnLayout { // IP
                    Layout.fillWidth: true
                    Layout.row: 1
                    Layout.column: 1
                    NetworkListModel {
                        id: networkListModel
                    }
                    CheckBox {
                        id: ipSelectAutoCheckBox
                        text: QI18n.Get('auto_detect')
                        checked: SettingsForm.GetSettings().serverIP === 'auto'
                        onToggled: {
                            let isChecked = ipSelectAutoCheckBox.checked;
                            ipSelectComboBox.visible = isChecked;
                            ipSelectTextField.visible = !isChecked;
                            ipSelectTextField.text = isChecked ? 'auto' : '';

                            let settings = SettingsForm.GetSettings();
                            settings.serverIP = isChecked ? 'auto' : '';
                            SettingsForm.SetSettings(settings);
                        }
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: ipSelectComboBox
                        model: networkListModel
                        textRole: 'ifWithIp'
                        currentIndex: {
                            let settings = SettingsForm.GetSettings();
                            for(let i = 0; i < networkListModel.size(); i++) {
                                if(networkListModel.get(i).macAddress === settings.serverMAC) {
                                    return i;
                                }
                            }
                            return 0;
                        }
                        visible: SettingsForm.GetSettings().serverIP === 'auto'
                    }
                    TextField {
                        Layout.fillWidth: true
                        id: ipSelectTextField
                        text: SettingsForm.GetSettings().serverIP
                        visible: SettingsForm.GetSettings().serverIP !== 'auto'
                    }
                }
                TextField { // Pairing port
                    Layout.fillWidth: true
                    Layout.row: 2
                    Layout.column: 1
                    id: serverPortTextField
                    text: SettingsForm.GetSettings().serverPort
                }
            }
        }
        GroupBox {
            Layout.preferredWidth: parent.width / 3
            Layout.preferredHeight: parent.height / 2.5
            Layout.alignment: Qt.AlignRight
            title: '%1 %2'.arg(SettingsForm.GetOperatingSystem()).arg(QI18n.Get('service_settings'))
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 5
                id: serviceSettingsLayout
            }
        }
    }
    ColumnLayout {
        anchors.top: settingsLayout.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        StepButtons {
            nextTitle: QI18n.Get('save')
            onBackClicked: MainWindow.Show(viewLoader)
            onNextClicked: {
                let ipV4Regex = /^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$/gm;
                let serverIp = ipSelectTextField.text;
                let serverPortNum = parseInt(serverPortTextField.text, 10);
                if(isNaN(serverPortNum) || serverPortNum < 0 || serverPortNum > 65535) {
                    showErrorMessage(QI18n.Get('error_invalid_port'));
                    return;
                } else if(serverIp !== 'auto' && !ipV4Regex.test(serverIp)) {
                    showErrorMessage(QI18n.Get('error_invalid_ip'));
                    return;
                } else {
                    let settings = SettingsForm.GetSettings();
                    settings.serverIP = serverIp;
                    settings.serverPort = serverPortNum;
                    settings.serverMAC = serverIp === 'auto' ? networkListModel.get(ipSelectComboBox.currentIndex).macAddress : '';
                    SettingsForm.SetSettings(settings);
                }
                SettingsForm.OnSaveSettingsClicked(viewLoader)
            }
        }
    }
    Component.onCompleted: {
        let settings = SettingsForm.GetServiceSettings();
        for(let i = 0; i < settings.length; i++) {
            Qt.createQmlObject("
                import QtQuick
                import QtQuick.Controls
                import PCBioUnlock

                CheckBox {
                    id: serviceSettingCheckBox%1
                    text: \"%2\"
                    checked: %3
                    onToggled: {
                        let settings = SettingsForm.GetServiceSettings();
                        for(let i = 0; i < settings.length; i++) {
                            if(settings[i].name === serviceSettingCheckBox%1.text) {
                                settings[i].enabled = serviceSettingCheckBox%1.checked;
                                break;
                            }
                        }
                        SettingsForm.SetServiceSettings(settings);
                    }
                }
            ".arg(i).arg(settings[i].name).arg(settings[i].enabled ? 'true' : 'false'), serviceSettingsLayout);
        }
    }
}
