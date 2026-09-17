import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import PulseUnlock

ApplicationWindow {
    id: sshServersWindow
    width: 720
    height: 500
    minimumWidth: 640
    minimumHeight: 420
    title: QI18n.Get('ssh_servers')
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    readonly property string selectedDeviceId: deviceComboBox.currentIndex < 0 ? '' : deviceListModel.get(deviceComboBox.currentIndex).deviceId

    function reloadServers() {
        serversTableView.selectedRow = -1;
        serversTableModel.reload(selectedDeviceId);
    }

    SshDeviceListModel {
        id: deviceListModel
    }
    SshServersTableModel {
        id: serversTableModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: '%1:'.arg(QI18n.Get('device'))
            }
            ComboBox {
                id: deviceComboBox
                Layout.fillWidth: true
                model: deviceListModel
                textRole: 'deviceName'
                onActivated: sshServersWindow.reloadServers()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            HorizontalHeaderView {
                Layout.fillWidth: true
                syncView: serversTableView
                clip: true
                model: [QI18n.Get('ssh_host'), QI18n.Get('user'), QI18n.Get('ssh_key_path')]
                delegate: Rectangle {
                    color: sshServersWindow.color
                    border.width: 1
                    implicitHeight: 40
                    Label {
                        anchors.centerIn: parent
                        color: Material.foreground
                        text: modelData
                    }
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120

                TableView {
                    id: serversTableView
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: serversTableModel
                    ScrollBar.vertical: ScrollBar {}

                    property int selectedRow: -1
                    readonly property var columnWidths: [0.3, 0.2, 0.5]
                    columnWidthProvider: function (column) {
                        return serversTableView.width * serversTableView.columnWidths[column];
                    }
                    delegate: ItemDelegate {
                        highlighted: row === serversTableView.selectedRow
                        text: model.tableData
                        ToolTip.text: text
                        ToolTip.delay: 800
                        ToolTip.visible: hovered && text !== '-'
                        onClicked: serversTableView.selectedRow = row
                    }
                }
                Label {
                    anchors.centerIn: parent
                    opacity: 0.6
                    visible: serversTableView.rows === 0
                    text: deviceListModel.size() === 0 ? QI18n.Get('not_paired') : QI18n.Get('ssh_no_servers')
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: QI18n.Get('ssh_add')
                enabled: sshServersWindow.selectedDeviceId !== ''
                onClicked: addDialog.open()
            }
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: QI18n.Get('ssh_remove')
                enabled: serversTableView.selectedRow >= 0
                onClicked: {
                    let server = serversTableModel.get(serversTableView.selectedRow);
                    SshServersWindow.RemoveServer(server[0], Number(server[1]));
                    sshServersWindow.reloadServers();
                }
            }
        }
    }

    SshAddServerDialog {
        id: addDialog
        deviceId: sshServersWindow.selectedDeviceId
        onServerAdded: sshServersWindow.reloadServers()
    }

    Component.onCompleted: reloadServers()
    onClosing: SshServersWindow.CancelAdd()
}
