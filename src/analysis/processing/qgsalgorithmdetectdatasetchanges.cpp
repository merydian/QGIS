/***************************************************************************
                         qgsalgorithmdetectdatasetchanges.cpp
                         -----------------------------------------
    begin                : December 2019
    copyright            : (C) 2019 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsalgorithmdetectdatasetchanges.h"

#include "qgsspatialindex.h"
#include "qgsvectorlayer.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

QString QgsDetectVectorChangesAlgorithm::name() const
{
  return u"detectvectorchanges"_s;
}

QString QgsDetectVectorChangesAlgorithm::displayName() const
{
  return QObject::tr( "Detect dataset changes" );
}

QStringList QgsDetectVectorChangesAlgorithm::tags() const
{
  return QObject::tr( "added,dropped,new,deleted,features,geometries,difference,delta,revised,original,version" ).split( ',' );
}

QString QgsDetectVectorChangesAlgorithm::group() const
{
  return QObject::tr( "Vector general" );
}

QString QgsDetectVectorChangesAlgorithm::groupId() const
{
  return u"vectorgeneral"_s;
}

void QgsDetectVectorChangesAlgorithm::initAlgorithm( const QVariantMap & )
{
  addParameter( new QgsProcessingParameterFeatureSource( u"ORIGINAL"_s, QObject::tr( "Original layer" ) ) );
  addParameter( new QgsProcessingParameterFeatureSource( u"REVISED"_s, QObject::tr( "Revised layer" ) ) );

  auto compareAttributesParam = std::make_unique<
    QgsProcessingParameterField>( u"COMPARE_ATTRIBUTES"_s, QObject::tr( "Attributes to consider for match (or none to compare geometry only)" ), QVariant(), u"ORIGINAL"_s, Qgis::ProcessingFieldParameterDataType::Any, true, true );
  compareAttributesParam->setDefaultToAllFields( true );
  addParameter( compareAttributesParam.release() );

  std::unique_ptr<QgsProcessingParameterDefinition> matchTypeParam = std::make_unique<
    QgsProcessingParameterEnum>( u"MATCH_TYPE"_s, QObject::tr( "Geometry comparison behavior" ), QStringList() << QObject::tr( "Exact Match" ) << QObject::tr( "Tolerant Match (Topological Equality)" ), false, 1 );
  matchTypeParam->setFlags( matchTypeParam->flags() | Qgis::ProcessingParameterFlag::Advanced );
  addParameter( matchTypeParam.release() );

  addParameter( new QgsProcessingParameterFeatureSink( u"UNCHANGED"_s, QObject::tr( "Unchanged features" ), Qgis::ProcessingSourceType::VectorAnyGeometry, QVariant(), true, true ) );
  addParameter( new QgsProcessingParameterFeatureSink( u"ADDED"_s, QObject::tr( "Added features" ), Qgis::ProcessingSourceType::VectorAnyGeometry, QVariant(), true, true ) );
  addParameter( new QgsProcessingParameterFeatureSink( u"DELETED"_s, QObject::tr( "Deleted features" ), Qgis::ProcessingSourceType::VectorAnyGeometry, QVariant(), true, true ) );
  addParameter( new QgsProcessingParameterFeatureSink( u"CHANGED"_s, QObject::tr( "Changed features" ), Qgis::ProcessingSourceType::VectorAnyGeometry, QVariant(), true, true ) );

  addOutput( new QgsProcessingOutputNumber( u"UNCHANGED_COUNT"_s, QObject::tr( "Count of unchanged features" ) ) );
  addOutput( new QgsProcessingOutputNumber( u"ADDED_COUNT"_s, QObject::tr( "Count of features added in revised layer" ) ) );
  addOutput( new QgsProcessingOutputNumber( u"DELETED_COUNT"_s, QObject::tr( "Count of features deleted from original layer" ) ) );
  addOutput( new QgsProcessingOutputNumber( u"CHANGED_COUNT"_s, QObject::tr( "Count of features changed between original and revised layers" ) ) );
}

QString QgsDetectVectorChangesAlgorithm::shortHelpString() const
{
  return QObject::tr(
    "This algorithm compares two vector layers, and determines which features are unchanged, added or deleted between "
    "the two. It is designed for comparing two different versions of the same dataset.\n\n"
    "When comparing features, the original and revised feature geometries will be compared against each other. Depending "
    "on the Geometry Comparison Behavior setting, the comparison will either be made using an exact comparison (where "
    "geometries must be an exact match for each other, including the order and count of vertices) or a topological "
    "comparison only (where geometries are considered equal if all of their component edges overlap. E.g. "
    "lines with the same vertex locations but opposite direction will be considered equal by this method). If the topological "
    "comparison is selected then any z or m values present in the geometries will not be compared.\n\n"
    "By default, the algorithm compares all attributes from the original and revised features. If the Attributes to Consider for Match "
    "parameter is changed, then only the selected attributes will be compared (e.g. allowing users to ignore a timestamp or ID field "
    "which is expected to change between the revisions).\n\n"
    "If any features in the original or revised layers do not have an associated geometry, then care must be taken to ensure "
    "that these features have a unique set of attributes selected for comparison. If this condition is not met, warnings will be "
    "raised and the resultant outputs may be misleading.\n\n"
    "The algorithm outputs three layers, one containing all features which are considered to be unchanged between the revisions, "
    "one containing features deleted from the original layer which are not present in the revised layer, and one containing features "
    "added to the revised layer which are not present in the original layer."
  );
}

QString QgsDetectVectorChangesAlgorithm::shortDescription() const
{
  return QObject::tr( "Calculates features which are unchanged, added or deleted between two dataset versions." );
}

QgsDetectVectorChangesAlgorithm *QgsDetectVectorChangesAlgorithm::createInstance() const
{
  return new QgsDetectVectorChangesAlgorithm();
}

bool QgsDetectVectorChangesAlgorithm::prepareAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  mOriginal.reset( parameterAsSource( parameters, u"ORIGINAL"_s, context ) );
  if ( !mOriginal )
    throw QgsProcessingException( invalidSourceError( parameters, u"ORIGINAL"_s ) );

  mRevised.reset( parameterAsSource( parameters, u"REVISED"_s, context ) );
  if ( !mRevised )
    throw QgsProcessingException( invalidSourceError( parameters, u"REVISED"_s ) );

  mMatchType = static_cast<GeometryMatchType>( parameterAsEnum( parameters, u"MATCH_TYPE"_s, context ) );

  switch ( mMatchType )
  {
    case Exact:
      if ( mOriginal->wkbType() != mRevised->wkbType() )
        throw QgsProcessingException(
          QObject::tr( "Geometry type of revised layer (%1) does not match the original layer (%2). Consider using the \"Tolerant Match\" option instead." )
            .arg( QgsWkbTypes::displayString( mRevised->wkbType() ), QgsWkbTypes::displayString( mOriginal->wkbType() ) )
        );
      break;

    case Topological:
      if ( QgsWkbTypes::geometryType( mOriginal->wkbType() ) != QgsWkbTypes::geometryType( mRevised->wkbType() ) )
        throw QgsProcessingException(
          QObject::tr( "Geometry type of revised layer (%1) does not match the original layer (%2)" )
            .arg( QgsWkbTypes::geometryDisplayString( QgsWkbTypes::geometryType( mRevised->wkbType() ) ), QgsWkbTypes::geometryDisplayString( QgsWkbTypes::geometryType( mOriginal->wkbType() ) ) )
        );
      break;
  }

  if ( mOriginal->sourceCrs() != mRevised->sourceCrs() )
    feedback->reportError(
      QObject::tr( "CRS for revised layer (%1) does not match the original layer (%2) - reprojection accuracy may affect geometry matching" )
        .arg( mOriginal->sourceCrs().userFriendlyIdentifier(), mRevised->sourceCrs().userFriendlyIdentifier() ),
      false
    );

  mFieldsToCompare = parameterAsStrings( parameters, u"COMPARE_ATTRIBUTES"_s, context );
  mOriginalFieldsToCompareIndices.reserve( mFieldsToCompare.size() );
  mRevisedFieldsToCompareIndices.reserve( mFieldsToCompare.size() );
  QStringList missingOriginalFields;
  QStringList missingRevisedFields;
  for ( const QString &field : mFieldsToCompare )
  {
    const int originalIndex = mOriginal->fields().lookupField( field );
    mOriginalFieldsToCompareIndices.append( originalIndex );
    if ( originalIndex < 0 )
      missingOriginalFields << field;

    const int revisedIndex = mRevised->fields().lookupField( field );
    if ( revisedIndex < 0 )
      missingRevisedFields << field;
    mRevisedFieldsToCompareIndices.append( revisedIndex );
  }

  if ( !missingOriginalFields.empty() )
    throw QgsProcessingException( QObject::tr( "Original layer missing selected comparison attributes: %1" ).arg( missingOriginalFields.join( ',' ) ) );
  if ( !missingRevisedFields.empty() )
    throw QgsProcessingException( QObject::tr( "Revised layer missing selected comparison attributes: %1" ).arg( missingRevisedFields.join( ',' ) ) );

  return true;
}

QVariantMap QgsDetectVectorChangesAlgorithm::processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  QString unchangedDestId;
  std::unique_ptr<QgsFeatureSink> unchangedSink( parameterAsSink( parameters, u"UNCHANGED"_s, context, unchangedDestId, mOriginal->fields(), mOriginal->wkbType(), mOriginal->sourceCrs() ) );
  if ( !unchangedSink && parameters.value( u"UNCHANGED"_s ).isValid() )
    throw QgsProcessingException( invalidSinkError( parameters, u"UNCHANGED"_s ) );

  QString addedDestId;
  std::unique_ptr<QgsFeatureSink> addedSink( parameterAsSink( parameters, u"ADDED"_s, context, addedDestId, mRevised->fields(), mRevised->wkbType(), mRevised->sourceCrs() ) );
  if ( !addedSink && parameters.value( u"ADDED"_s ).isValid() )
    throw QgsProcessingException( invalidSinkError( parameters, u"ADDED"_s ) );

  QString deletedDestId;
  std::unique_ptr<QgsFeatureSink> deletedSink( parameterAsSink( parameters, u"DELETED"_s, context, deletedDestId, mOriginal->fields(), mOriginal->wkbType(), mOriginal->sourceCrs() ) );
  if ( !deletedSink && parameters.value( u"DELETED"_s ).isValid() )
    throw QgsProcessingException( invalidSinkError( parameters, u"DELETED"_s ) );

  QString changedDestId;
  std::unique_ptr<QgsFeatureSink> changedSink( parameterAsSink( parameters, u"CHANGED"_s, context, changedDestId, mRevised->fields(), mRevised->wkbType(), mRevised->sourceCrs() ) );
  if ( !changedSink && parameters.value( u"CHANGED"_s ).isValid() )
    throw QgsProcessingException( invalidSinkError( parameters, u"CHANGED"_s ) );

  QSet<QgsFeatureId> unchangedIds;
  QSet<QgsFeatureId> addedIds;
  QSet<QgsFeatureId> deletedIds;
  QSet<QgsFeatureId> changedIds;

  QgsFeature f;
  QgsFeatureRequest req;

  QSet<QgsFeatureId> beforeIds;
  QgsFeatureIterator it = mOriginal->getFeatures( req );
  while ( it.nextFeature( f ) )
  {
    beforeIds.insert( f.id() );
  }

  QSet<QgsFeatureId> afterIds;
  it = mRevised->getFeatures( req );
  while ( it.nextFeature( f ) )
  {
    afterIds.insert( f.id() );
  }

  double step = mRevised->featureCount() > 0 ? 100.0 / mRevised->featureCount() : 0;
  QgsFeatureRequest revisedRequest = QgsFeatureRequest().setDestinationCrs( mOriginal->sourceCrs(), context.transformContext() );
  revisedRequest.setSubsetOfAttributes( mRevisedFieldsToCompareIndices );
  it = mRevised->getFeatures( revisedRequest );
  QgsFeature revisedFeature;
  long current = 0;

  while ( it.nextFeature( revisedFeature ) )
  {
    if ( feedback->isCanceled() )
      break;

    if ( beforeIds.contains( revisedFeature.id() ) )
    {
      // Feature exists in original layer, check if unchanged or changed
      bool equals = true;
      QgsFeatureRequest origRequest;
      origRequest.setFilterFid( revisedFeature.id() );
      origRequest.setDestinationCrs( mOriginal->sourceCrs(), context.transformContext() );
      origRequest.setSubsetOfAttributes( mOriginalFieldsToCompareIndices );
      QgsFeatureIterator origIt = mOriginal->getFeatures( origRequest );
      QgsFeature originalFeature;

      if ( origIt.nextFeature( originalFeature ) )
      {
        // Compare only the selected attributes
        for ( int i : mOriginalFieldsToCompareIndices )
        {
          if ( originalFeature.attribute( i ) != revisedFeature.attribute( mRevisedFieldsToCompareIndices.indexOf( i ) ) )
          {
            equals = false;
            break;
          }
        }

        // Also compare geometry if both have geometry
        if ( equals && revisedFeature.hasGeometry() && originalFeature.hasGeometry() )
        {
          bool geometryMatch = false;
          switch ( mMatchType )
          {
            case Topological:
              geometryMatch = revisedFeature.geometry().isTopologicallyEqual( originalFeature.geometry() );
              break;
            case Exact:
              geometryMatch = revisedFeature.geometry().isExactlyEqual( originalFeature.geometry() );
              break;
          }

          if ( !geometryMatch )
          {
            equals = false;
          }
        }

        if ( equals )
        {
          unchangedIds.insert( revisedFeature.id() );
          if ( unchangedSink )
          {
            if ( !unchangedSink->addFeature( revisedFeature, QgsFeatureSink::FastInsert ) )
              throw QgsProcessingException( writeFeatureError( unchangedSink.get(), parameters, u"UNCHANGED"_s ) );
          }
        }
        else
        {
          changedIds.insert( revisedFeature.id() );
          if ( changedSink )
          {
            if ( !changedSink->addFeature( revisedFeature, QgsFeatureSink::FastInsert ) )
              throw QgsProcessingException( writeFeatureError( changedSink.get(), parameters, u"CHANGED"_s ) );
          }
        }
      }
    }
    else if ( afterIds.contains( revisedFeature.id() ) )
    {
      // Added feature - in revised but not original
      addedIds.insert( revisedFeature.id() );
      if ( addedSink )
      {
        if ( !addedSink->addFeature( revisedFeature, QgsFeatureSink::FastInsert ) )
          throw QgsProcessingException( writeFeatureError( addedSink.get(), parameters, u"ADDED"_s ) );
      }
    }

    current++;
    feedback->setProgress( 100.0 * current * step );
  }

  // Process original features for deleted ones
  step = mOriginal->featureCount() > 0 ? 100.0 / mOriginal->featureCount() : 0;
  req = QgsFeatureRequest();
  it = mOriginal->getFeatures( req );
  current = 0;

  while ( it.nextFeature( f ) )
  {
    if ( feedback->isCanceled() )
      break;

    if ( !afterIds.contains( f.id() ) )
    {
      // Deleted feature - in original but not revised
      deletedIds.insert( f.id() );
      if ( deletedSink )
      {
        if ( !deletedSink->addFeature( f, QgsFeatureSink::FastInsert ) )
          throw QgsProcessingException( writeFeatureError( deletedSink.get(), parameters, u"DELETED"_s ) );
      }
    }

    current++;
    feedback->setProgress( 100.0 * current * step );
  }

  if ( unchangedSink )
    unchangedSink->finalize();
  if ( addedSink )
    addedSink->finalize();
  if ( deletedSink )
    deletedSink->finalize();
  if ( changedSink )
    changedSink->finalize();

  QVariantMap outputs;
  outputs.insert( u"UNCHANGED"_s, unchangedDestId );
  outputs.insert( u"ADDED"_s, addedDestId );
  outputs.insert( u"DELETED"_s, deletedDestId );
  outputs.insert( u"CHANGED"_s, changedDestId );
  outputs.insert( u"UNCHANGED_COUNT"_s, static_cast<long long>( unchangedIds.size() ) );
  outputs.insert( u"ADDED_COUNT"_s, static_cast<long long>( addedIds.size() ) );
  outputs.insert( u"DELETED_COUNT"_s, static_cast<long long>( deletedIds.size() ) );
  outputs.insert( u"CHANGED_COUNT"_s, static_cast<long long>( changedIds.size() ) );

  return outputs;
}

///@endcond

///@endcond
