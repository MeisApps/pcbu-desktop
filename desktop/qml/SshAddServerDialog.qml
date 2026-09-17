import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import PulseUnlock

Dialog {
    id: addDialog
    property string deviceId: ''
    signal serverAdded

    title: QI18n.Get('ssh_add_title')
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent.width - 80, 560)
    modal: true

    property bool unlocking: false
    property bool failed: false
    property string status: ''

    readonly property bool canSave: !unlocking && passwordField.text.length > 0 && (hostField.text.trim().length > 0 || keyPathField.text.trim().length > 0)

    function clearFields() {
        hostField.text = '';
        userField.text = '';
        keyPathField.text = '';
        passwordField.text = '';
        status = '';
        failed = false;
    }
    function submit() {
        if (!canSave)
            return;
        failed = false;
        status = QI18n.Get('please_wait');
        unlocking = true;
        if (!SshServersWindow.AddServer(addDialog, deviceId, hostField.text, userField.text, keyPathField.text, passwordField.text)) {
            unlocking = false;
            status = '';
        }
    }
    function setUnlockMessage(message) {
        status = message;
    }
    function finishAdd(success, message) {
        unlocking = false;
        if (success) {
            clearFields();
            addDialog.close();
            addDialog.serverAdded();
            return;
        }
        failed = true;
        status = message;
    }

    onOpened: {
        clearFields();
        hostField.forceActiveFocus();
    }
    onClosed: {
        clearFields();
        SshServersWindow.CancelAdd();
    }
    onApplied: submit()

    footer: DialogButtonBox {
        Button {
            flat: true
            text: QI18n.Get('cancel')
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            flat: true
            text: QI18n.Get('save')
            enabled: addDialog.canSave
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 8
            rowSpacing: 6
            enabled: !addDialog.unlocking

            Label {
                text: '%1:'.arg(QI18n.Get('ssh_host'))
            }
            TextField {
                id: hostField
                Layout.fillWidth: true
                Layout.columnSpan: 2
                onAccepted: addDialog.submit()
            }

            Label {
                text: '%1:'.arg(QI18n.Get('user'))
            }
            TextField {
                id: userField
                Layout.fillWidth: true
                Layout.columnSpan: 2
                onAccepted: addDialog.submit()
            }

            Label {
                text: '%1:'.arg(QI18n.Get('ssh_key_path'))
            }
            TextField {
                id: keyPathField
                Layout.fillWidth: true
                onAccepted: addDialog.submit()
            }
            Button {
                text: QI18n.Get('ssh_browse')
                onClicked: keyFileDialog.open()
            }

            Label {
                text: '%1:'.arg(QI18n.Get('password'))
            }
            TextField {
                id: passwordField
                Layout.fillWidth: true
                Layout.columnSpan: 2
                echoMode: TextField.Password
                onAccepted: addDialog.submit()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: addDialog.status !== ''
            BusyIndicator {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                visible: addDialog.unlocking
                running: visible
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: addDialog.failed ? Material.color(Material.Red, Material.Shade300) : Material.foreground
                text: addDialog.status
            }
        }
    }

    FileDialog {
        id: keyFileDialog
        fileMode: FileDialog.OpenFile
        currentFolder: SshServersWindow.GetSshDirUrl()
        onAccepted: keyPathField.text = SshServersWindow.GetPathFromUrl(selectedFile)
    }
}
