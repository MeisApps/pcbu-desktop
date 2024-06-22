import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import PCBioUnlock

ApplicationWindow {
    id: logsWindow
    width: 800
    height: 600
    title: QI18n.Get('logs')
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25
        ColumnLayout {
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height / 2
            Label {
                text: '%1:'.arg(QI18n.Get('desktop_logs'))
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: desktopLogScrollView
                TextArea {
                    readOnly: true
                    text: LogsWindow.GetDesktopLogs()
                }
            }
        }
        ColumnLayout {
            Layout.topMargin: 25
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height / 2
            Label {
                text: '%1:'.arg(QI18n.Get('module_logs'))
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: moduleLogScrollView
                TextArea {
                    readOnly: true
                    text: LogsWindow.GetModuleLogs()
                }
            }
        }
    }
    Component.onCompleted: {
        desktopLogScrollView.ScrollBar.vertical.position = 1.0 - desktopLogScrollView.ScrollBar.vertical.size;
        moduleLogScrollView.ScrollBar.vertical.position = 1.0 - moduleLogScrollView.ScrollBar.vertical.size;
    }
}
