//##########################################################################
//#                                                                        #
//#                CLOUDCOMPARE PLUGIN: LAS-IO Plugin                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                   COPYRIGHT: Thomas Montaigu                           #
//#                                                                        #
//##########################################################################
#include "LasSaveDialog.h"

#include <QPushButton>
#include <QStringListModel>

#include "LasFields.h"
#include "ui/ExtraScalarFieldRow.h"
#include "ui/ScalarFieldRow.h"

#include "ccLog.h"
#include "ccPointCloud.h"
#include "ccScalarField.h"

LasSaveDialog::LasSaveDialog(ccPointCloud *cloud, QWidget *parent)
    : QDialog(parent), m_cloud(cloud), m_comboBoxModel(new QStringListModel) {
  setupUi(this);
  origRadioButton_2->setEnabled(false);
  customScaleDoubleSpinBox_2->setEnabled(true);
  bestRadioButton_2->setChecked(true);

  connect(
      versionComboBox,
      (void(QComboBox::*)(const QString &))(&QComboBox::currentIndexChanged),
      this, &LasSaveDialog::handleSelectedVersionChange);

  connect(pointFormatComboBox,
          (void(QComboBox::*)(int))(&QComboBox::currentIndexChanged), this,
          &LasSaveDialog::handleSelectedPointFormatChange);

  for (const char *versionStr : GetAvailableVersion()) {
    versionComboBox->addItem(versionStr);
  }
  versionComboBox->setCurrentIndex(0);

  QStringList cloudScalarFieldsNames;
  cloudScalarFieldsNames << QString();
  for (unsigned int i = 0; i < m_cloud->getNumberOfScalarFields(); ++i) {
    if (strcmp(m_cloud->getScalarFieldName(i), "Default") != 0) {
      cloudScalarFieldsNames << m_cloud->getScalarFieldName(i);
    }
  }
  m_comboBoxModel->setStringList(cloudScalarFieldsNames);

  // optimal scale (for accuracy) --> 1e-9 because the maximum integer is
  // roughly +/-2e+9
  CCVector3 bbMax;
  CCVector3 bbMin;
  cloud->getBoundingBox(bbMin, bbMax);
  CCVector3d diag = bbMax - bbMin;
  CCVector3d optimalScale(
      1.0e-9 * std::max<double>(diag.x, CCCoreLib::ZERO_TOLERANCE_D),
      1.0e-9 * std::max<double>(diag.y, CCCoreLib::ZERO_TOLERANCE_D),
      1.0e-9 * std::max<double>(diag.z, CCCoreLib::ZERO_TOLERANCE_D));
  setOptimalScale(optimalScale);

  QStringList extraFieldsTypes;
  extraFieldsTypes << "uint8_t"
                   << "uint16_t"
                   << "uint32_t"
                   << "uint64_t"
                   << "int8_t"
                   << "int16_t"
                   << "int32_t"
                   << "int64_t"
                   << "float"
                   << "double";
  m_extraFieldsTypesModel = new QStringListModel;
  m_extraFieldsTypesModel->setStringList(extraFieldsTypes);
}

/// When the selected version changes, we need to update the combo box
/// of point format to match the ones supported by the version
void LasSaveDialog::handleSelectedVersionChange(const QString &version) {
  const std::vector<int> pointFormats =
      PointFormatsAvailableForVersion(qPrintable(version));
  pointFormatComboBox->clear();
  for (unsigned int fmt : pointFormats) {
    pointFormatComboBox->addItem(QString::number(fmt));
  }

  const int previousIndex = pointFormatComboBox->currentIndex();
  pointFormatComboBox->setCurrentIndex(0);
  if (previousIndex == 0) {
    // We have to force the call here
    // because the index did not change,
    // But the actual point format did
    handleSelectedPointFormatChange(0);
  }
}

/// When the user changes the point format, we need to redo the
/// scalar field form.
void LasSaveDialog::handleSelectedPointFormatChange(int index) {
  if (index < 0) {
    return;
  }

  const std::vector<int> pointFormats = PointFormatsAvailableForVersion(
      qPrintable(versionComboBox->currentText()));

  int selectedPointFormat = pointFormats[index];
  const LasField::Vector lasScalarFields =
      LasField::FieldsForPointFormat(selectedPointFormat);

  // Hide all the rows of scalar fields
  for (int i{0}; i < scalarFieldLayout->count(); ++i) {
    auto *item = scalarFieldFormLayout->itemAt(i);
    if (item && item->widget()) {
      item->widget()->hide();
      item->widget()->setEnabled(false);
    }
  }

  const QStringList cloudScalarFieldsNames = m_comboBoxModel->stringList();
  for (size_t i{0}; i < lasScalarFields.size(); ++i) {
    const LasField &field = lasScalarFields[i];
    int matchingCCScalarFieldIndex =
        cloudScalarFieldsNames.indexOf(field.name.c_str());
    ScalarFieldRow *row;
    if (i < scalarFieldLayout->count()) {
      // Reuse existing row
      auto *item = scalarFieldLayout->itemAt(static_cast<int>(i));
      if (!item || !item->widget()) {
        continue;
      }

      row = qobject_cast<ScalarFieldRow *>(item->widget());
      if (!row) {
        continue;
      }

      row->show();
      row->setEnabled(true);
    } else {
      // Create a new row
      row = new ScalarFieldRow;
      row->setLasSaveDialog(this);
      scalarFieldLayout->addWidget(row);
    }
    row->setName(QString::fromStdString(field.name));
    row->setCurrentIndex(matchingCCScalarFieldIndex);
  }

  // Handle the special fields RGB, Waveform
  if (!HasRGB(selectedPointFormat) && !HasWaveform(selectedPointFormat)) {
    specialScalarFieldFrame->hide();
    waveformCheckBox->setCheckState(Qt::Unchecked);
    rgbCheckBox->setCheckState(Qt::Unchecked);
  } else {
    specialScalarFieldFrame->show();
    if (HasRGB(selectedPointFormat)) {
      rgbCheckBox->show();
      rgbCheckBox->setEnabled(m_cloud->hasColors());
      rgbCheckBox->setChecked(m_cloud->hasColors());
    } else {
      rgbCheckBox->hide();
    }

    if (HasWaveform(selectedPointFormat)) {
      waveformCheckBox->show();
      waveformCheckBox->setEnabled(m_cloud->hasFWF());
      waveformCheckBox->setChecked(m_cloud->hasFWF());
    } else {
      waveformCheckBox->hide();
    }
  }

  // Extra bytes
  for (int i{0}; i < vertLayout->count(); ++i) {
    auto *item = vertLayout->itemAt(i);
    if (item != nullptr && item->widget() != nullptr) {
      item->widget()->hide();
      item->widget()->setEnabled(false);
    }
  }

  int currentRow{0};
  for (const QString &ccSfName : cloudScalarFieldsNames) {
    bool selectedInAnything{false};
    ExtraScalarFieldRow *row;
    for (int i{0}; i < scalarFieldLayout->count(); ++i) {
      auto *item = scalarFieldLayout->itemAt(static_cast<int>(i));
      if (!item || !item->widget() || item->widget()->isHidden()) {
        continue;
      }

      auto *standardRow = qobject_cast<ScalarFieldRow *>(item->widget());
      if (!standardRow) {
        continue;
      }

      if (ccSfName == standardRow->currentScalarFieldName()) {
        selectedInAnything = true;
        break;
      }
    }
    if (selectedInAnything) {
      continue;
    }
    if (currentRow < vertLayout->count()) {
      // Reuse existing row
      auto *item = vertLayout->itemAt(currentRow);
      if (!item || !item->widget()) {
        continue;
      }

      row = qobject_cast<ExtraScalarFieldRow *>(item->widget());
      if (!row) {
        continue;
      }

      row->show();
      row->setEnabled(true);
      row->setCurrentScalarFieldIndex(cloudScalarFieldsNames.indexOf(ccSfName));
    } else {
      // Create a new row
      row = new ExtraScalarFieldRow;
      row->setScalarFieldNamesModel(m_comboBoxModel);
      row->setTypeComboBoxModel(m_extraFieldsTypesModel);
      row->setCloud(m_cloud);
      row->show();
      vertLayout->addWidget(row);
    }
    row->setCurrentScalarFieldIndex(cloudScalarFieldsNames.indexOf(ccSfName));
    auto it =
        std::find_if(m_savedExtraFields.begin(), m_savedExtraFields.end(),
                     [ccSfName](const SavedExtraField &savedExtraField) {
                       return savedExtraField.name == ccSfName.toStdString();
                     });
    if (it != m_savedExtraFields.end()) {
      row->setType(it->type);
    }
    currentRow++;
  }
}

void LasSaveDialog::setVersionAndPointFormat(const QString &version,
                                             unsigned int pointFormat) {
  int i = versionComboBox->findText(version);
  if (i >= 0) {
    QString fmtStr = QString::number(pointFormat);
    versionComboBox->setCurrentIndex(i);
    int j = pointFormatComboBox->findText(fmtStr);
    if (j >= 0) {
      pointFormatComboBox->setCurrentIndex(j);
    }
  }
}

void LasSaveDialog::setOptimalScale(const CCVector3d &optimalScale) {
  bestAccuracyLabel_2->setText(QString("(%1, %2,%3)")
                                   .arg(optimalScale.x)
                                   .arg(optimalScale.y)
                                   .arg(optimalScale.z));
}

void LasSaveDialog::setSavedScale(const CCVector3d &savedScale) {
  origAccuracyLabel_2->setText(QString("(%1, %2, %3)")
                                   .arg(savedScale.x)
                                   .arg(savedScale.y)
                                   .arg(savedScale.z));
  origRadioButton_2->setEnabled(true);
  origRadioButton_2->setChecked(true);
}

unsigned int LasSaveDialog::selectedPointFormat() const {
  std::string s = pointFormatComboBox->currentText().toStdString();
  return pointFormatComboBox->currentText().toUInt();
}

unsigned int LasSaveDialog::selectedVersionMinor() const {
  return versionComboBox->currentText().splitRef('.').at(1).toUInt();
}

bool LasSaveDialog::shouldSaveColors() const {
  return !rgbCheckBox->isHidden() && rgbCheckBox->isChecked();
}
//
// bool LasSaveDialog::shouldSaveWaveform() const
//{
//    return waveformCheckBox->isChecked();
//}
//
CCVector3d LasSaveDialog::chosenScale() const {
  const auto vectorFromString = [](const QString &string) -> CCVector3d {
    QVector<QStringRef> splits = string.splitRef(',');
    if (splits.size() == 3) {
      double x = splits[0].right(splits[0].size() - 1).toDouble();
      double y = splits[1].toDouble();
      double z = splits[2].left(splits[2].size() - 1).toDouble();
      return {x, y, z};
    }
    return {};
  };
  if (bestRadioButton_2->isChecked()) {
    QString text = bestAccuracyLabel_2->text();
    return vectorFromString(text);
  } else if (origRadioButton_2->isChecked()) {
    QString text = origAccuracyLabel_2->text();
    return vectorFromString(text);
  } else if (customRadioButton_2->isChecked()) {
    double value = customScaleDoubleSpinBox_2->value();
    return {value, value, value};
  }
  return {};
}

LasField::Vector LasSaveDialog::fieldsToSave() const {
  LasField::Vector fields;
  fields.reserve(scalarFieldFormLayout->rowCount());
  unsigned int pointFormat = selectedPointFormat();

  for (int rowIndex{0}; rowIndex < scalarFieldLayout->count(); ++rowIndex) {
    auto *item = scalarFieldLayout->itemAt(rowIndex);
    if (!item || !item->widget() || item->widget()->isHidden()) {
      break;
    }

    auto *row = qobject_cast<ScalarFieldRow *>(item->widget());
    if (!row) {
      break;
    }

    int ccSfIdx = m_cloud->getScalarFieldIndexByName(
        row->currentScalarFieldName().toStdString().c_str());
    if (ccSfIdx == -1) {
      continue;
    }
    auto *sf = static_cast<ccScalarField *>(m_cloud->getScalarField(ccSfIdx));
    const std::string name = row->lasScalarFieldName().toStdString();
    LasField::Id id = LasField::IdFromName(name.c_str(), pointFormat);
    LasField field(id, pointFormat >= 6, sf);
    fields.push_back(field);
  }

  for (int rowIndex{0}; rowIndex < vertLayout->count(); ++rowIndex) {
    auto *item = vertLayout->itemAt(rowIndex);
    if (!item || !item->widget() || item->widget()->isHidden()) {
      break;
    }

    auto *row = qobject_cast<ExtraScalarFieldRow *>(item->widget());
    if (!row) {
      break;
    }
    const std::string ccScalarFieldName =
        row->currentScalarFieldName().toStdString();
    auto *sf = static_cast<ccScalarField *>(m_cloud->getScalarField(
        m_cloud->getScalarFieldIndexByName(ccScalarFieldName.c_str())));

    LasField field;
    field.pdalId = pdal::Dimension::Id::Unknown; // It's not registered yet
    field.name = ccScalarFieldName;
    field.sf = sf;
    field.pdalType = row->pdalType();
    field.range = row->range();

    fields.push_back(field);
  }
  return fields;
}
void LasSaveDialog::setExtraScalarFields(
    const std::vector<SavedExtraField> &savedExtraFields) {
  m_savedExtraFields = savedExtraFields;
  // we need to refresh
  handleSelectedPointFormatChange(selectedPointFormat());
}