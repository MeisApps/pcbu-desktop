import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import PulseUnlock

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    minimumWidth: 1024
    minimumHeight: 768
    visible: true
    title: QI18n.Get('product_name')

    property bool canClose: false
    onClosing: function (close) {
        close.accepted = window.canClose;
    }

    Item {
        anchors.fill: parent
        enabled: window.hasInitialized
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 25
            Label {
                id: title
                text: QI18n.Get('product_name')
                font.pointSize: 36
            }
            Loader {
                id: viewLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                source: "qrc:/ui/forms/MainForm.qml"
            }
        }
    }

    Dialog {
        id: startupDialog
        title: QI18n.Get('product_name')
        anchors.centerIn: Overlay.overlay
        modal: true
        closePolicy: Popup.NoAutoClose
        visible: !window.hasInitialized
        RowLayout {
            spacing: 16
            BusyIndicator {
                running: startupDialog.visible
            }
            Label {
                text: QI18n.Get('please_wait')
                font.pointSize: 14
            }
        }
    }

    property var onMessageDialogAccept: function () {}
    property var onMessageDialogConfirmAccept: function () {}
    MessageDialog {
        id: messageDialog
        title: 'Title'
        text: 'Text'
        onAccepted: onMessageDialogAccept()
        onRejected: onMessageDialogAccept()
    }
    MessageDialog {
        id: confirmMessageDialog
        title: QI18n.Get('confirm')
        text: 'Text'
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: onMessageDialogConfirmAccept()
    }
    Dialog {
        id: skipPasswordCheckDialog
        title: QI18n.Get('error')
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 200, 600)
        modal: true
        standardButtons: Dialog.Ok
        property string message: ''
        onOpened: skipPasswordCheckBox.checked = false
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: skipPasswordCheckDialog.message
            }
            CheckBox {
                id: skipPasswordCheckBox
                text: QI18n.Get('skip_password_check')
            }
        }
        onAccepted: {
            if (!skipPasswordCheckBox.checked)
                return;
            PairingForm.SetSkipPasswordCheck(true);
            PairingForm.OnNextClicked(viewLoader, window);
        }
    }

    function showFatalErrorMessage(text) {
        messageDialog.title = QI18n.Get('error');
        messageDialog.text = text;
        messageDialog.visible = true;
        onMessageDialogAccept = function () {
            Qt.exit(1);
        };
    }
    function showErrorMessage(text) {
        messageDialog.title = QI18n.Get('error');
        messageDialog.text = text;
        messageDialog.visible = true;
    }
    function showInfoMessage(text) {
        messageDialog.title = QI18n.Get('notice');
        messageDialog.text = text;
        messageDialog.visible = true;
    }
    function showConfirmMessage(text, onAccept) {
        confirmMessageDialog.text = text;
        confirmMessageDialog.visible = true;
        onMessageDialogConfirmAccept = onAccept;
    }
    function showSkipPasswordCheckMessage(text) {
        skipPasswordCheckDialog.message = text;
        skipPasswordCheckDialog.open();
    }

    function showLoadingScreen(text) {
        viewLoader.source = "qrc:/ui/forms/LoadingForm.qml";
        viewLoader.item.okBtn.enabled = false;
        viewLoader.item.progressLbl.text = text;
        window.canClose = false;
    }
    function appendLoadingOutput(text) {
        viewLoader.item.outputTxtArea.text += text + '\n';
    }
    function finishLoadingScreen(text) {
        window.canClose = true;
        viewLoader.item.okBtn.enabled = true;
        viewLoader.item.progressBar.indeterminate = false;
        viewLoader.item.progressBar.value = 100;
        viewLoader.item.progressLbl.text = text;
    }

    function updateBluetoothDeviceList(devices) {
        let selItemName = undefined;
        try {
            selItemName = viewLoader.item.selectBTListModel.get(viewLoader.item.selectBTList.currentIndex).name;
        } catch (e) {}
        let selIdx = -1;
        viewLoader.item.selectBTListModel.clear();
        for (let i = 0; i < devices.length; i++) {
            viewLoader.item.selectBTListModel.append({
                name: devices[i].name,
                address: devices[i].address
            });
            if (devices[i].name === selItemName)
                selIdx = i;
        }
        viewLoader.item.selectBTList.currentIndex = selIdx;
    }
    function finishBluetoothPairing(isSuccess) {
        if (!isSuccess) {
            showErrorMessage(QI18n.Get('error_bluetooth_pairing'));
            PairingForm.OnBackClicked(viewLoader, window);
            return;
        }
        PairingForm.OnNextClicked(viewLoader, window);
    }

    function showUpdaterWindow() {
        let updaterWin = Qt.createComponent("qrc:/ui/UpdaterWindow.qml").createObject(window);
        updaterWin.show();
    }

    property bool hasInitialized: false
    property bool startupStarted: false
    function onStartupChecksFinished(success, needsReinstall) {
        hasInitialized = true;
        if (!success)
            return;
        if (needsReinstall) {
            MainWindow.OnReinstallClicked(window);
        } else {
            canClose = true;
            MainWindow.Show(viewLoader);
        }
        UpdaterWindow.CheckForUpdates(window);
    }
    onFrameSwapped: {
        if (!startupStarted) {
            startupStarted = true;
            MainWindow.PerformStartupChecks(window);
        }
    }
}
