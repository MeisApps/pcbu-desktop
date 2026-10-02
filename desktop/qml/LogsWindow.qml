import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import PulseUnlock

ApplicationWindow {
    id: logsWindow
    width: 800
    height: 600
    title: QI18n.Get('logs')

    property var logTexts: [QI18n.Get('loading'), QI18n.Get('loading'), QI18n.Get('loading')]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25
        spacing: 10
        Label {
            text: QI18n.Get('logs')
            font.pointSize: 36
        }
        TabBar {
            id: logTabBar
            Layout.fillWidth: true
            Repeater {
                model: ['desktop_logs', 'module_logs', 'elevator_logs']
                TabButton {
                    text: QI18n.Get(modelData)
                }
            }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: logTabBar.currentIndex
            Repeater {
                model: logsWindow.logTexts
                ScrollView {
                    TextArea {
                        readOnly: true
                        wrapMode: Text.WordWrap
                        text: modelData
                        Component.onCompleted: cursorPosition = length
                    }
                }
            }
        }
    }
    Component.onCompleted: {
        LogsWindow.LoadLogs(logsWindow);
    }

    function setLogs(desktopLogs, moduleLogs, elevatorLogs) {
        logTexts = [desktopLogs, moduleLogs, elevatorLogs];
    }
}
