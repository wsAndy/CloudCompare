#include "ScalarFieldRow.h"

#include "LasSaveDialog.h"

void ScalarFieldRow::setLasSaveDialog(const LasSaveDialog *saveDialog) {

  m_saveDialog = saveDialog;
  m_ccScalarFieldNameComboBox->setModel(saveDialog->m_comboBoxModel);
}

void ScalarFieldRow::handleSelectedScalarFieldChanged(
    const QString &ccScalarFieldName) {
  if (ccScalarFieldName.isEmpty()) {
    m_warningLabel->setPixmap(QPixmap());
    return;
  }

  const ccPointCloud *cloud = m_saveDialog->m_cloud;

  int sfIdx =
      cloud->getScalarFieldIndexByName(ccScalarFieldName.toStdString().c_str());
  if (sfIdx == -1) {
    qCritical() << "Somehow the cloud doesn't have the same fields as it "
                   "used to ?\n";
    return;
  }
  const CCCoreLib::ScalarField *scalarField = cloud->getScalarField(sfIdx);

  // TODO id should be a member of this class ?
  LasField::Id scalarFieldId =
      LasField::IdFromName(m_nameLabel->text().toStdString().c_str(),
                           m_saveDialog->selectedPointFormat());
  LasField::Range range = LasField::ValueRange(scalarFieldId);

  if (scalarField->getMin() < range.min || scalarField->getMax() > range.max) {
    ccLog::PrintDebug(
        "Field %s with range [%f, %f] does not fit in range [%f, %f]",
        scalarField->getName(), scalarField->getMin(), scalarField->getMax(),
        range.min, range.max);
    m_warningLabel->setToolTip(QStringLiteral(
        "Some of the values of the selected scalar field are out "
        "of range for the selected type"));
    SetWarningIcon(*m_warningLabel);
  } else {
    SetOkIcon(*m_warningLabel);
  }
}
