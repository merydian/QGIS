/***************************************************************************
                         qgsalgorithmlayoutatlastoimage.h
                         ---------------------
    begin                : June 2020
    copyright            : (C) 2020 by Mathieu Pellerin
    email                : nirvn dot asia at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSALGORITHMLAYOUTATLASTOIMAGE_H
#define QGSALGORITHMLAYOUTATLASTOIMAGE_H

#define SIP_NO_FILE

#include "qgis_sip.h"
#include "qgsprocessingalgorithm.h"

///@cond PRIVATE

/**
 * Native export layout to image algorithm.
 */
class QgsLayoutAtlasToImageAlgorithm : public QgsProcessingAlgorithm
{
  public:
    QgsLayoutAtlasToImageAlgorithm() = default;
    void initAlgorithm( const QVariantMap &configuration = QVariantMap() ) override;
    Qgis::ProcessingAlgorithmFlags flags() const override;
    QString name() const override;
    QString displayName() const override;
    QStringList tags() const override;
    QString group() const override;
    QString groupId() const override;
    QString shortDescription() const override;
    QString shortHelpString() const override;
    QgsLayoutAtlasToImageAlgorithm *createInstance() const override SIP_FACTORY;

  protected:
    QVariantMap processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback ) override;
};

///@endcond PRIVATE

#endif // QGSALGORITHMLAYOUTATLASTOIMAGE_H
