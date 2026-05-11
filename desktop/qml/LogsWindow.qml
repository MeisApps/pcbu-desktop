import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import PCBioUnlock

ApplicationWindow {
    id: logsWindow
    width: 800
    height: 800
    title: QI18n.Get('logs')
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25
        ColumnLayout {
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height / 3
            Label {
                text: '%1:'.arg(QI18n.Get('desktop_logs'))
            }
            ScrollView {
                id: desktopLogScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: desktopLogTextArea
                    readOnly: true
                    wrapMode: Text.WordWrap
                    text: QI18n.Get('loading')
                }
            }
        }
        ColumnLayout {
            Layout.topMargin: 25
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height / 3
            Label {
                text: '%1:'.arg(QI18n.Get('module_logs'))
            }
            ScrollView {
                id: moduleLogScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: moduleLogTextArea
                    readOnly: true
                    wrapMode: Text.WordWrap
                    text: QI18n.Get('loading')
                }
            }
        }
        ColumnLayout {
            Layout.topMargin: 25
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height / 3
            Label {
                text: '%1:'.arg(QI18n.Get('elevator_logs'))
            }
            ScrollView {
                id: elevatorLogScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: elevatorLogTextArea
                    readOnly: true
                    wrapMode: Text.WordWrap
                    text: QI18n.Get('loading')
                }
            }
        }
    }
    Component.onCompleted: {
        LogsWindow.LoadLogs(logsWindow);
    }

    function setLogs(desktopLogs, moduleLogs, elevatorLogs) {
        desktopLogTextArea.text = desktopLogs;
        moduleLogTextArea.text = moduleLogs;
        elevatorLogTextArea.text = elevatorLogs;
        desktopLogTextArea.cursorPosition = desktopLogTextArea.length;
        moduleLogTextArea.cursorPosition = moduleLogTextArea.length;
        elevatorLogTextArea.cursorPosition = elevatorLogTextArea.length;
    }
}
