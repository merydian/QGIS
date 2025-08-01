/***************************************************************************
    qgsmeshrendererscalarsettingswidget.cpp
    ---------------------------------------
    begin                : June 2018
    copyright            : (C) 2018 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmeshrendererscalarsettingswidget.h"
#include "moc_qgsmeshrendererscalarsettingswidget.cpp"

#include "QDialogButtonBox"

#include "qgis.h"
#include "qgsmeshlayer.h"
#include "qgsmeshvariablestrokewidthwidget.h"
#include "qgsmapcanvas.h"
#include <QPointer>
#include "qgsmessagelog.h"

QgsMeshRendererScalarSettingsWidget::QgsMeshRendererScalarSettingsWidget( QWidget *parent )
  : QWidget( parent )

{
  setupUi( this );

  mScalarMinSpinBox->setClearValueMode( QgsDoubleSpinBox::ClearValueMode::MinimumValue );
  mScalarMinSpinBox->setSpecialValueText( QString() );
  mScalarMaxSpinBox->setClearValueMode( QgsDoubleSpinBox::ClearValueMode::MinimumValue );
  mScalarMaxSpinBox->setSpecialValueText( QString() );

  mScalarMinSpinBox->setEnabled( true );
  mScalarMaxSpinBox->setEnabled( true );

  // add items to data interpolation combo box
  mScalarInterpolationTypeComboBox->addItem( tr( "No Resampling" ), QgsMeshRendererScalarSettings::NoResampling );
  mScalarInterpolationTypeComboBox->addItem( tr( "Neighbour Average" ), QgsMeshRendererScalarSettings::NeighbourAverage );
  mScalarInterpolationTypeComboBox->setCurrentIndex( 0 );

  mMinMaxValueTypeComboBox->addItem( tr( "Whole Mesh" ), QVariant::fromValue( Qgis::MeshRangeExtent::WholeMesh ) );
  mMinMaxValueTypeComboBox->addItem( tr( "Current Canvas" ), QVariant::fromValue( Qgis::MeshRangeExtent::FixedCanvas ) );
  mMinMaxValueTypeComboBox->addItem( tr( "Updated Canvas" ), QVariant::fromValue( Qgis::MeshRangeExtent::UpdatedCanvas ) );
  mMinMaxValueTypeComboBox->setCurrentIndex( 0 );

  mUserDefinedRadioButton->setChecked( true );
  mMinMaxValueTypeComboBox->setEnabled( false );

  mScalarEdgeStrokeWidthUnitSelectionWidget->setUnits(
    {
      Qgis::RenderUnit::Millimeters,
      Qgis::RenderUnit::MetersInMapUnits,
      Qgis::RenderUnit::Pixels,
      Qgis::RenderUnit::Points,
    }
  );

  // connect
  connect( mScalarRecalculateMinMaxButton, &QPushButton::clicked, this, &QgsMeshRendererScalarSettingsWidget::recalculateMinMaxButtonClicked );
  connect( mScalarMinSpinBox, qOverload<double>( &QgsDoubleSpinBox::valueChanged ), this, [this]( double ) { minMaxChanged(); } );
  connect( mScalarMaxSpinBox, qOverload<double>( &QgsDoubleSpinBox::valueChanged ), this, [this]( double ) { minMaxChanged(); } );
  connect( mScalarEdgeStrokeWidthVariableRadioButton, &QRadioButton::toggled, this, &QgsMeshRendererScalarSettingsWidget::onEdgeStrokeWidthMethodChanged );

  connect( mScalarColorRampShaderWidget, &QgsColorRampShaderWidget::widgetChanged, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mOpacityWidget, &QgsOpacityWidget::opacityChanged, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mScalarInterpolationTypeComboBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );

  connect( mScalarEdgeStrokeWidthUnitSelectionWidget, &QgsUnitSelectionWidget::changed, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mScalarEdgeStrokeWidthSpinBox, qOverload<double>( &QgsDoubleSpinBox::valueChanged ), this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mScalarEdgeStrokeWidthVariableRadioButton, &QCheckBox::toggled, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mScalarEdgeStrokeWidthFixedRadioButton, &QCheckBox::toggled, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
  connect( mScalarEdgeStrokeWidthVariablePushButton, &QgsMeshVariableStrokeWidthButton::widgetChanged, this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );

  connect( mUserDefinedRadioButton, &QRadioButton::toggled, this, &QgsMeshRendererScalarSettingsWidget::mUserDefinedRadioButton_toggled );
  connect( mMinMaxRadioButton, &QRadioButton::toggled, this, &QgsMeshRendererScalarSettingsWidget::mMinMaxRadioButton_toggled );

  connect( mMinMaxValueTypeComboBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, &QgsMeshRendererScalarSettingsWidget::recalculateMinMax );
  connect( mMinMaxValueTypeComboBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, &QgsMeshRendererScalarSettingsWidget::widgetChanged );
}

void QgsMeshRendererScalarSettingsWidget::setLayer( QgsMeshLayer *layer )
{
  mMeshLayer = layer;
  mScalarInterpolationTypeComboBox->setEnabled( !dataIsDefinedOnEdges() );
}

void QgsMeshRendererScalarSettingsWidget::setActiveDatasetGroup( int groupIndex )
{
  mActiveDatasetGroup = groupIndex;
  mScalarInterpolationTypeComboBox->setEnabled( !dataIsDefinedOnEdges() );
}

QgsMeshRendererScalarSettings QgsMeshRendererScalarSettingsWidget::settings() const
{
  QgsMeshRendererScalarSettings settings;
  settings.setClassificationMinimumMaximum( spinBoxValue( mScalarMinSpinBox ), spinBoxValue( mScalarMaxSpinBox ) );
  settings.setColorRampShader( mScalarColorRampShaderWidget->shader() );
  settings.setOpacity( mOpacityWidget->opacity() );
  settings.setDataResamplingMethod( dataIntepolationMethod() );

  settings.setExtent( mMinMaxValueTypeComboBox->currentData().value<Qgis::MeshRangeExtent>() );

  if ( mUserDefinedRadioButton->isChecked() )
  {
    settings.setLimits( Qgis::MeshRangeLimit::NotSet );
  }
  else
  {
    settings.setLimits( Qgis::MeshRangeLimit::MinimumMaximum );
  }

  const bool hasEdges = ( mMeshLayer->contains( QgsMesh::ElementType::Edge ) );
  if ( hasEdges )
  {
    QgsInterpolatedLineWidth edgeStrokeWidth = mScalarEdgeStrokeWidthVariablePushButton->variableStrokeWidth();
    edgeStrokeWidth.setIsVariableWidth( mScalarEdgeStrokeWidthVariableRadioButton->isChecked() );
    edgeStrokeWidth.setFixedStrokeWidth( mScalarEdgeStrokeWidthSpinBox->value() );
    settings.setEdgeStrokeWidth( edgeStrokeWidth );
    settings.setEdgeStrokeWidthUnit( mScalarEdgeStrokeWidthUnitSelectionWidget->unit() );
  }

  return settings;
}

void QgsMeshRendererScalarSettingsWidget::syncToLayer()
{
  if ( !mMeshLayer )
    return;

  if ( mActiveDatasetGroup < 0 )
    return;

  const QgsMeshRendererSettings rendererSettings = mMeshLayer->rendererSettings();
  const QgsMeshRendererScalarSettings settings = rendererSettings.scalarSettings( mActiveDatasetGroup );
  const QgsColorRampShader shader = settings.colorRampShader();

  const double min = settings.classificationMinimum();
  const double max = settings.classificationMaximum();

  if ( std::abs( max ) < 1e-2 )
  {
    mScalarMinSpinBox->setDecimals( 8 );
    mScalarMaxSpinBox->setDecimals( 8 );
  }
  else
  {
    mScalarMinSpinBox->setDecimals( 2 );
    mScalarMaxSpinBox->setDecimals( 2 );
  }

  whileBlocking( mScalarMinSpinBox )->setValue( min );
  whileBlocking( mScalarMaxSpinBox )->setValue( max );

  whileBlocking( mMinMaxValueTypeComboBox )->setCurrentIndex( mMinMaxValueTypeComboBox->findData( QVariant::fromValue( settings.extent() ) ) );

  if ( settings.limits() == Qgis::MeshRangeLimit::MinimumMaximum )
  {
    whileBlocking( mUserDefinedRadioButton )->setChecked( false );
    whileBlocking( mMinMaxRadioButton )->setChecked( true );
    mScalarMinSpinBox->setEnabled( false );
    mScalarMaxSpinBox->setEnabled( false );
    mMinMaxValueTypeComboBox->setEnabled( true );
  }
  else
  {
    whileBlocking( mUserDefinedRadioButton )->setChecked( true );
    whileBlocking( mMinMaxRadioButton )->setChecked( false );
    mScalarMinSpinBox->setEnabled( true );
    mScalarMaxSpinBox->setEnabled( true );
    mMinMaxValueTypeComboBox->setEnabled( false );
  }

  whileBlocking( mScalarColorRampShaderWidget )->setFromShader( shader );
  whileBlocking( mScalarColorRampShaderWidget )->setMinimumMaximum( min, max );
  whileBlocking( mOpacityWidget )->setOpacity( settings.opacity() );
  const int index = mScalarInterpolationTypeComboBox->findData( settings.dataResamplingMethod() );
  whileBlocking( mScalarInterpolationTypeComboBox )->setCurrentIndex( index );

  const bool hasEdges = ( mMeshLayer->contains( QgsMesh::ElementType::Edge ) );
  const bool hasFaces = ( mMeshLayer->contains( QgsMesh::ElementType::Face ) );

  mScalarResamplingWidget->setVisible( hasFaces );

  mEdgeWidthGroupBox->setVisible( hasEdges );

  if ( hasEdges )
  {
    const QgsInterpolatedLineWidth edgeStrokeWidth = settings.edgeStrokeWidth();
    whileBlocking( mScalarEdgeStrokeWidthVariablePushButton )->setVariableStrokeWidth( edgeStrokeWidth );
    whileBlocking( mScalarEdgeStrokeWidthSpinBox )->setValue( edgeStrokeWidth.fixedStrokeWidth() );
    whileBlocking( mScalarEdgeStrokeWidthVariableRadioButton )->setChecked( edgeStrokeWidth.isVariableWidth() );
    whileBlocking( mScalarEdgeStrokeWidthUnitSelectionWidget )->setUnit( settings.edgeStrokeWidthUnit() );
    if ( !hasFaces )
      mOpacityContainerWidget->setVisible( false );

    const QgsMeshDatasetGroupMetadata metadata = mMeshLayer->datasetGroupMetadata( mActiveDatasetGroup );
    const double min = metadata.minimum();
    const double max = metadata.maximum();
    mScalarEdgeStrokeWidthVariablePushButton->setDefaultMinMaxValue( min, max );
  }

  onEdgeStrokeWidthMethodChanged();
}

double QgsMeshRendererScalarSettingsWidget::spinBoxValue( const QgsDoubleSpinBox *spinBox ) const
{
  if ( spinBox->value() == spinBox->clearValue() )
  {
    return std::numeric_limits<double>::quiet_NaN();
  }

  return spinBox->value();
}

void QgsMeshRendererScalarSettingsWidget::minMaxChanged()
{
  const double min = spinBoxValue( mScalarMinSpinBox );
  const double max = spinBoxValue( mScalarMaxSpinBox );
  mScalarColorRampShaderWidget->setMinimumMaximumAndClassify( min, max );
}

void QgsMeshRendererScalarSettingsWidget::recalculateMinMaxButtonClicked()
{
  const QgsMeshDatasetGroupMetadata metadata = mMeshLayer->datasetGroupMetadata( mActiveDatasetGroup );
  const double min = metadata.minimum();
  const double max = metadata.maximum();

  if ( std::abs( max ) < 1e-2 )
  {
    mScalarMinSpinBox->setDecimals( 8 );
    mScalarMaxSpinBox->setDecimals( 8 );
  }
  else
  {
    mScalarMinSpinBox->setDecimals( 2 );
    mScalarMaxSpinBox->setDecimals( 2 );
  }

  whileBlocking( mScalarMinSpinBox )->setValue( min );
  whileBlocking( mScalarMaxSpinBox )->setValue( max );
  mScalarColorRampShaderWidget->setMinimumMaximumAndClassify( min, max );
}

void QgsMeshRendererScalarSettingsWidget::onEdgeStrokeWidthMethodChanged()
{
  const bool variableWidth = mScalarEdgeStrokeWidthVariableRadioButton->isChecked();
  mScalarEdgeStrokeWidthVariablePushButton->setVisible( variableWidth );
  mScalarEdgeStrokeWidthSpinBox->setVisible( !variableWidth );
}

QgsMeshRendererScalarSettings::DataResamplingMethod QgsMeshRendererScalarSettingsWidget::dataIntepolationMethod() const
{
  const int data = mScalarInterpolationTypeComboBox->currentData().toInt();
  const QgsMeshRendererScalarSettings::DataResamplingMethod method = static_cast<QgsMeshRendererScalarSettings::DataResamplingMethod>( data );
  return method;
}

bool QgsMeshRendererScalarSettingsWidget::dataIsDefinedOnFaces() const
{
  if ( !mMeshLayer )
    return false;

  if ( mActiveDatasetGroup < 0 )
    return false;

  const QgsMeshDatasetGroupMetadata meta = mMeshLayer->datasetGroupMetadata( mActiveDatasetGroup );
  const bool onFaces = ( meta.dataType() == QgsMeshDatasetGroupMetadata::DataOnFaces );
  return onFaces;
}

bool QgsMeshRendererScalarSettingsWidget::dataIsDefinedOnEdges() const
{
  if ( !mMeshLayer )
    return false;

  if ( mActiveDatasetGroup < 0 )
    return false;

  const QgsMeshDatasetGroupMetadata meta = mMeshLayer->datasetGroupMetadata( mActiveDatasetGroup );
  const bool onEdges = ( meta.dataType() == QgsMeshDatasetGroupMetadata::DataOnEdges );
  return onEdges;
}

void QgsMeshRendererScalarSettingsWidget::setCanvas( QgsMapCanvas *canvas )
{
  mCanvas = canvas;
}

void QgsMeshRendererScalarSettingsWidget::recalculateMinMax()
{
  QgsRectangle searchExtent;

  Qgis::MeshRangeExtent extentRange = mMinMaxValueTypeComboBox->currentData().value<Qgis::MeshRangeExtent>();

  switch ( extentRange )
  {
    case Qgis::MeshRangeExtent::WholeMesh:
    {
      searchExtent = mMeshLayer->extent();
      break;
    }
    case Qgis::MeshRangeExtent::FixedCanvas:
    case Qgis::MeshRangeExtent::UpdatedCanvas:
    {
      QgsCoordinateTransform ct = QgsCoordinateTransform( mCanvas->mapSettings().destinationCrs(), mMeshLayer->crs(), QgsProject::instance() );
      searchExtent = mCanvas->extent();
      try
      {
        searchExtent = ct.transform( searchExtent );
      }
      catch ( const QgsCsException &e )
      {
        QgsMessageLog::logMessage( QObject::tr( "Transform error caught: %1" ).arg( e.what() ), QObject::tr( "CRS" ) );
        // can properly transform canvas extent to meshlayer crs, use full mesh layer extent
        searchExtent = mMeshLayer->extent();
      }
      break;
    }
    default:
      break;
  }

  if ( !searchExtent.isEmpty() )
  {
    QgsMeshDatasetIndex datasetIndex = mMeshLayer->activeScalarDatasetAtTime( mCanvas->temporalRange(), mActiveDatasetGroup );
    double min, max;
    bool found;

    found = mMeshLayer->minimumMaximumActiveScalarDataset( searchExtent, datasetIndex, min, max );
    if ( found )
    {
      whileBlocking( mScalarMinSpinBox )->setValue( min );
      whileBlocking( mScalarMaxSpinBox )->setValue( max );
      minMaxChanged();
    }
  }
}

void QgsMeshRendererScalarSettingsWidget::mUserDefinedRadioButton_toggled( bool toggled )
{
  mMinMaxValueTypeComboBox->setEnabled( !toggled );
  mScalarMinSpinBox->setEnabled( toggled );
  mScalarMaxSpinBox->setEnabled( toggled );
  mScalarRecalculateMinMaxButton->setEnabled( toggled );
  emit widgetChanged();
}

void QgsMeshRendererScalarSettingsWidget::mMinMaxRadioButton_toggled( bool toggled )
{
  mMinMaxValueTypeComboBox->setEnabled( toggled );
  mScalarMinSpinBox->setEnabled( !toggled );
  mScalarMaxSpinBox->setEnabled( !toggled );
  mScalarRecalculateMinMaxButton->setEnabled( !toggled );
  emit widgetChanged();
}
