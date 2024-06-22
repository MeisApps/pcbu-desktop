import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import PCBioUnlock

ApplicationWindow {
    id: updaterWindow
    width: 400
    height: 250
    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height
    title: QI18n.Get('updater')
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    property bool canClose: false
    onClosing: function(close) { close.accepted = updaterWindow.canClose }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25
        Label {
            text: QI18n.Get('update_available')
            font.pointSize: 18
        }
        Label {
            text: '%1: %2'.arg(QI18n.Get('your_version')).arg(UpdaterWindow.GetAppVersion())
        }
        Label {
            text: '%1: %2'.arg(QI18n.Get('latest_version')).arg(UpdaterWindow.GetLatestVersion())
        }
        Label {
            Layout.topMargin: 15
            id: downloadProgressLabel
            text: '0%'
        }
        ProgressBar {
            Layout.bottomMargin: 15
            Layout.fillWidth: true
            id: downloadProgressBar
            to: 100
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom | Qt.AlignRight
            Button {
                id: downloadButton
                text: QI18n.Get('download')
                onClicked: {
                    downloadButton.enabled = false;
                    ignoreButton.enabled = false;
                    UpdaterWindow.OnDownloadClicked(updaterWindow)
                }
            }
            Button {
                id: ignoreButton
                text: QI18n.Get('ignore')
                onClicked: {
                    updaterWindow.canClose = true;
                    updaterWindow.close();
                }
            }
        }
    }

    MessageDialog {
        id: updaterMessageDialog
        title: QI18n.Get('error')
        text: 'Text'
    }

    function updateDownloadProgress(percent, progressText) {
        downloadProgressBar.value = percent;
        downloadProgressLabel.text = progressText;
    }
    function closeUpdaterWindow(errorText) {
        updaterWindow.canClose = true;
        updaterWindow.close();
        messageDialog.text = errorText;
        messageDialog.visible = true;
    }
}
