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

    property var logNames: Qt.platform.os === 'osx' ? ['desktop_logs', 'module_logs', 'elevator_logs', 'mac_agent_logs'] : ['desktop_logs', 'module_logs', 'elevator_logs']
    property var logTexts: logNames.map(function () { return QI18n.Get('loading'); })

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
                model: logsWindow.logNames
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

    function setLogs(desktopLogs, moduleLogs, elevatorLogs, macAgentLogs) {
        logTexts = [desktopLogs, moduleLogs, elevatorLogs, macAgentLogs].slice(0, logNames.length);
    }
}
