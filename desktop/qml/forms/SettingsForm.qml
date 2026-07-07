import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import PCBioUnlock
import "qrc:/ui/base"

Form {
    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Common settings
            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 14

                // General + Network
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    // General
                    GroupBox {
                        id: generalGroup
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: generalCol.implicitHeight + topPadding + bottomPadding
                        title: QI18n.Get('general_settings')
                        ColumnLayout {
                            id: generalCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            spacing: 6

                            // Language
                            Label {
                                text: '%1:'.arg(QI18n.Get('language'))
                            }
                            ComboBox {
                                id: langComboBox
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: ListModel {
                                    id: langComboBoxModel
                                    ListElement {
                                        text: "Auto"
                                        val: "auto"
                                    }
                                    ListElement {
                                        text: "English"
                                        val: "en"
                                    }
                                    ListElement {
                                        text: "Deutsch"
                                        val: "de"
                                    }
                                    ListElement {
                                        text: "Chinese (Simplified)"
                                        val: "zh_CN"
                                    }
                                    ListElement {
                                        text: "Portuguese"
                                        val: "pt_PT"
                                    }
                                    ListElement {
                                        text: "Portuguese (Brazil)"
                                        val: "pt_BR"
                                    }
                                }
                                textRole: 'text'
                                currentIndex: {
                                    let currLang = SettingsForm.GetSettings().language;
                                    for (let i = 0; i < langComboBoxModel.count; i++) {
                                        if (currLang === langComboBoxModel.get(i).val) {
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

                            // Debug logging
                            CheckBox {
                                id: debugLogCheckBox
                                Layout.fillWidth: true
                                text: QI18n.Get('enable_debug_logs')
                                checked: SettingsForm.IsDebugLoggingEnabled()
                                ToolTip.text: QI18n.Get('enable_debug_logs_desc')
                                ToolTip.visible: hovered
                                onToggled: {
                                    SettingsForm.SetDebugLoggingEnabled(debugLogCheckBox.checked);
                                }
                            }
                        }
                    }

                    // Network
                    GroupBox {
                        id: networkGroup
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: networkCol.implicitHeight + topPadding + bottomPadding
                        title: QI18n.Get('network_settings')

                        // Interface selection
                        ColumnLayout {
                            id: networkCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            spacing: 6
                            NetworkListModel {
                                id: networkListModel
                            }
                            Label {
                                text: '%1:'.arg(QI18n.Get('ip_address'))
                                ToolTip.text: QI18n.Get('ip_address_desc')
                                ToolTip.visible: netHoverHandler.hovered
                                HoverHandler {
                                    id: netHoverHandler
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
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
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            ComboBox {
                                id: ipSelectComboBox
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: networkListModel
                                textRole: 'ifWithIp'
                                currentIndex: {
                                    let settings = SettingsForm.GetSettings();
                                    for (let i = 0; i < networkListModel.size(); i++) {
                                        if (networkListModel.get(i).macAddress === settings.serverMAC) {
                                            return i;
                                        }
                                    }
                                    return 0;
                                }
                                visible: SettingsForm.GetSettings().serverIP === 'auto'
                            }
                            TextField {
                                id: ipSelectTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().serverIP
                                visible: SettingsForm.GetSettings().serverIP !== 'auto'
                            }
                        }
                    }
                }

                // Ports
                GroupBox {
                    id: portsGroup
                    Layout.fillWidth: true
                    Layout.preferredHeight: portsGrid.implicitHeight + topPadding + bottomPadding
                    title: QI18n.Get('ports_settings')
                    GridLayout {
                        id: portsGrid
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8

                        // Pairing discovery port
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: QI18n.Get('pairing_discovery_port')
                                ToolTip.text: QI18n.Get('pairing_discovery_port_desc')
                                ToolTip.visible: discoveryHoverHandler.hovered
                                HoverHandler {
                                    id: discoveryHoverHandler
                                }
                            }
                            TextField {
                                id: pairingDiscoveryPortTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().pairingDiscoveryPort
                            }
                        }

                        // Pairing port
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: QI18n.Get('pairing_server_port')
                                ToolTip.text: QI18n.Get('pairing_server_port_desc')
                                ToolTip.visible: pairingPortHoverHandler.hovered
                                HoverHandler {
                                    id: pairingPortHoverHandler
                                }
                            }
                            TextField {
                                id: pairingServerPortTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().pairingServerPort
                            }
                        }

                        // Unlock port
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    text: QI18n.Get('unlock_server_port')
                                    ToolTip.text: QI18n.Get('unlock_server_port_desc')
                                    ToolTip.visible: unlockPortHoverHandler.hovered
                                    HoverHandler {
                                        id: unlockPortHoverHandler
                                    }
                                }
                                Label {
                                    text: QI18n.Get('method_tag_udp')
                                    opacity: 0.6
                                    font.italic: true
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            TextField {
                                id: unlockServerPortTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().unlockServerPort
                            }
                        }
                    }
                }

                // Connection
                GroupBox {
                    id: connectionGroup
                    Layout.fillWidth: true
                    Layout.preferredHeight: connectionGrid.implicitHeight + topPadding + bottomPadding
                    title: QI18n.Get('connection_settings')
                    GridLayout {
                        id: connectionGrid
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8

                        // Connect timeout
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    text: QI18n.Get('client_connect_timeout')
                                    ToolTip.text: QI18n.Get('client_connect_timeout_desc')
                                    ToolTip.visible: connectTimeoutHoverHandler.hovered
                                    HoverHandler {
                                        id: connectTimeoutHoverHandler
                                    }
                                }
                                Label {
                                    text: QI18n.Get('method_tag_tcp_bt')
                                    opacity: 0.6
                                    font.italic: true
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            TextField {
                                id: clientConnectTimeoutTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().clientConnectTimeout
                            }
                        }

                        // Connect retries
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    text: QI18n.Get('client_connect_retries')
                                    ToolTip.text: QI18n.Get('client_connect_retries_desc')
                                    ToolTip.visible: connectRetriesHoverHandler.hovered
                                    HoverHandler {
                                        id: connectRetriesHoverHandler
                                    }
                                }
                                Label {
                                    text: QI18n.Get('method_tag_tcp_bt')
                                    opacity: 0.6
                                    font.italic: true
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            TextField {
                                id: clientConnectRetriesTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().clientConnectRetries
                            }
                        }

                        // Read/Write timeout
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    text: QI18n.Get('client_socket_timeout')
                                    ToolTip.text: QI18n.Get('client_socket_timeout_desc')
                                    ToolTip.visible: socketTimeoutHoverHandler.hovered
                                    HoverHandler {
                                        id: socketTimeoutHoverHandler
                                    }
                                }
                                Label {
                                    text: QI18n.Get('method_tag_tcp_bt')
                                    opacity: 0.6
                                    font.italic: true
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            TextField {
                                id: clientSocketTimeoutTextField
                                Layout.fillWidth: true
                                text: SettingsForm.GetSettings().clientSocketTimeout
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // OS settings
            GroupBox {
                id: serviceGroup
                Layout.preferredHeight: serviceSettingsLayout.implicitHeight + topPadding + bottomPadding
                Layout.alignment: Qt.AlignVCenter
                title: '%1 %2'.arg(SettingsForm.GetOperatingSystem()).arg(QI18n.Get('service_settings'))
                ColumnLayout {
                    id: serviceSettingsLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 8
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }

        StepButtons {
            Layout.fillWidth: true
            nextTitle: QI18n.Get('save')
            onBackClicked: MainWindow.Show(viewLoader)
            onNextClicked: {
                let pairingDiscoveryPortNum = parseInt(pairingDiscoveryPortTextField.text, 10);
                let pairingServerPortNum = parseInt(pairingServerPortTextField.text, 10);
                let unlockServerPortNum = parseInt(unlockServerPortTextField.text, 10);
                if (isNaN(pairingDiscoveryPortNum) || pairingDiscoveryPortNum < 0 || pairingDiscoveryPortNum > 65535) {
                    showErrorMessage(QI18n.Get('error_invalid_port'));
                    return;
                }
                if (isNaN(pairingServerPortNum) || pairingServerPortNum < 0 || pairingServerPortNum > 65535) {
                    showErrorMessage(QI18n.Get('error_invalid_port'));
                    return;
                }
                if (isNaN(unlockServerPortNum) || unlockServerPortNum < 0 || unlockServerPortNum > 65535) {
                    showErrorMessage(QI18n.Get('error_invalid_port'));
                    return;
                }

                let ipV4Regex = /^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$/gm;
                let serverIp = ipSelectTextField.text;
                if (serverIp !== 'auto' && !ipV4Regex.test(serverIp)) {
                    showErrorMessage(QI18n.Get('error_invalid_ip'));
                    return;
                }

                let clientSocketTimeoutNum = parseInt(clientSocketTimeoutTextField.text, 10);
                let clientConnectTimeoutNum = parseInt(clientConnectTimeoutTextField.text, 10);
                let clientConnectRetriesNum = parseInt(clientConnectRetriesTextField.text, 10);
                if (isNaN(clientSocketTimeoutNum) || clientSocketTimeoutNum < 1) {
                    clientSocketTimeoutNum = 1;
                }
                if (isNaN(clientConnectTimeoutNum) || clientConnectTimeoutNum < 1) {
                    clientConnectTimeoutNum = 1;
                }
                if (isNaN(clientConnectRetriesNum) || clientConnectRetriesNum < 0) {
                    clientConnectRetriesNum = 0;
                }

                let doSave = function () {
                    let settings = SettingsForm.GetSettings();
                    settings.serverIP = serverIp;
                    settings.serverMAC = serverIp === 'auto' ? networkListModel.get(ipSelectComboBox.currentIndex).macAddress : '';
                    settings.pairingDiscoveryPort = pairingDiscoveryPortNum;
                    settings.pairingServerPort = pairingServerPortNum;
                    settings.unlockServerPort = unlockServerPortNum;
                    settings.clientSocketTimeout = clientSocketTimeoutNum;
                    settings.clientConnectTimeout = clientConnectTimeoutNum;
                    settings.clientConnectRetries = clientConnectRetriesNum;
                    SettingsForm.SetSettings(settings);
                    SettingsForm.OnSaveSettingsClicked(viewLoader, window);
                };

                if (unlockServerPortNum !== SettingsForm.GetSettings().unlockServerPort && SettingsForm.HasUDPDevices()) {
                    showConfirmMessage(QI18n.Get('confirm_unlock_port_change'), function () {
                        SettingsForm.RemoveUDPDevices();
                        doSave();
                    });
                } else {
                    doSave();
                }
            }
        }
    }
    Component.onCompleted: {
        let settings = SettingsForm.GetServiceSettings();
        for (let i = 0; i < settings.length; i++) {
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
