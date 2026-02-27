import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

import PCBioUnlock
import 'qrc:/ui/base'

Form {
    RowLayout {
        id: mainRowLayout
        anchors.fill: parent
        GroupBox {
            title: QI18n.Get('service_module')
            Layout.preferredWidth: parent.width / 3.5
            Layout.preferredHeight: parent.height / 1.6
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: '%1: %2'.arg(QI18n.Get('status')).arg(MainWindow.IsInstalled() ? QI18n.Get('installed') : QI18n.Get('not_installed'))
                }
                Label {
                    text: '%1: %2'.arg(QI18n.Get('installed_version')).arg(MainWindow.GetInstalledVersion())
                }
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: MainWindow.IsInstalled() ? QI18n.Get('uninstall') : QI18n.Get('install')
                    onClicked: MainWindow.OnInstallClicked(window)
                }
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: QI18n.Get('reinstall')
                    enabled: MainWindow.IsInstalled()
                    onClicked: MainWindow.OnReinstallClicked(window)
                }
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: QI18n.Get('settings')
                    enabled: MainWindow.IsInstalled()
                    onClicked: SettingsForm.Show(viewLoader)
                }
            }
        }

        GroupBox {
            title: QI18n.Get('devices')
            Layout.preferredWidth: parent.width / 1.8
            Layout.preferredHeight: parent.height / 1.45
            Layout.alignment: Qt.AlignRight
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: '%1: %2'.arg(QI18n.Get('status')).arg(MainWindow.IsPaired() ? QI18n.Get('paired') : QI18n.Get('not_paired'))
                }
                DevicesTableModel {
                    id: devicesTableModel
                }
                HorizontalHeaderView {
                    Layout.fillWidth: true
                    id: devicesTableHeader
                    syncView: devicesTableView
                    model: [QI18n.Get('device_name'), QI18n.Get('user'), QI18n.Get('method')]
                    clip: true
                    delegate: Rectangle {
                        color: window.color
                        border.width: 1
                        implicitHeight: 50
                        Label {
                            color: Material.foreground
                            text: modelData
                            anchors.centerIn: parent
                        }
                    }
                }
                TableView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    id: devicesTableView
                    boundsBehavior: Flickable.StopAtBounds
                    columnWidthProvider: function(column) { return devicesTableView.width / 3; }
                    model: devicesTableModel

                    property int selectedRow: -1
                    delegate: ItemDelegate {
                        highlighted: row === devicesTableView.selectedRow
                        onClicked: {
                            devicesTableView.selectedRow = row
                            deviceRemoveBtn.enabled = row !== -1;
                            deviceTestUnlockBtn.enabled = row !== -1;
                        }
                        text: model.tableData
                    }
                }
                RowLayout {
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        text: QI18n.Get('pair_device')
                        enabled: MainWindow.IsInstalled()
                        onClicked: PairingForm.Show(viewLoader, window)
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 60
                        id: deviceRemoveBtn
                        text: QI18n.Get('remove_device')
                        enabled: false
                        onClicked: {
                            showConfirmMessage(QI18n.Get('confirm_remove_device'), function () {
                                let selDevice = devicesTableModel.get(devicesTableView.selectedRow);
                                MainWindow.OnRemoveDeviceClicked(viewLoader, selDevice[0]);
                            });
                        }
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 60
                        id: deviceTestUnlockBtn
                        text: QI18n.Get('unlock_test')
                        enabled: false
                        onClicked: {
                            let selDevice = devicesTableModel.get(devicesTableView.selectedRow);
                            let unlockWin = Qt.createComponent("qrc:/ui/UnlockTestWindow.qml").createObject(window, {deviceId: selDevice[0]});
                            unlockWin.show();
                        }
                    }
                }
            }
        }
    }
    ColumnLayout {
        anchors.top: mainRowLayout.top
        anchors.bottom: parent.bottom
        width: parent.width
        height: 60
        ColumnLayout {
            Layout.minimumWidth: 100
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            RowLayout {
                Button {
                    Layout.minimumWidth: 100
                    Layout.minimumHeight: 40
                    text: QI18n.Get('about')
                    onClicked: {
                        let aboutWin = Qt.createComponent("qrc:/ui/AboutWindow.qml").createObject(window);
                        aboutWin.show();
                    }
                }
                Button {
                    Layout.minimumWidth: 100
                    Layout.minimumHeight: 40
                    text: QI18n.Get('logs')
                    onClicked: {
                        let logsWin = Qt.createComponent("qrc:/ui/LogsWindow.qml").createObject(window);
                        logsWin.show();
                    }
                }
            }
            Label {
                Layout.minimumWidth: 200
                horizontalAlignment: Text.AlignHCenter
                text: 'Version %1'.arg(UpdaterWindow.GetAppVersion())
            }
        }
    }
}
