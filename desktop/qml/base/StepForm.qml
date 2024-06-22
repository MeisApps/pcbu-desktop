import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PCBioUnlock

Rectangle {
    Layout.fillWidth: true
    Layout.fillHeight: true
    id: stepFormRect
    color: window.color

    property string description: "No description."
    property alias backTitle: stepButtons.backTitle
    property alias nextTitle: stepButtons.nextTitle
    property alias backBtn: stepButtons.backBtn
    property alias nextBtn: stepButtons.nextBtn

    ColumnLayout {
        anchors.fill: parent
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.bottomMargin: 20
            id: stepDescLabel
            text: stepFormRect.description
            wrapMode: Label.WordWrap
            font.pointSize: 18
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: stepFormContent
        }
        StepButtons {
            id: stepButtons
            onBackClicked: PairingForm.OnBackClicked(viewLoader, window)
            onNextClicked: PairingForm.OnNextClicked(viewLoader, window)
        }
    }

    default property alias content: stepFormContent.children
}
