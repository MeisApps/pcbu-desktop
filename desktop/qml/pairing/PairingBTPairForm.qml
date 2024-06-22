import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    description: QI18n.Get('pairing_form_bt_pair_desc')
    nextBtn.enabled: false
    backBtn.enabled: false
    ProgressBar {
        Layout.fillWidth: true
        id: btPairProgressBar
        indeterminate: true
        to: 100
    }
}
