/***************************************************************************
    qgsstacsearchparametersdialog.h
    ---------------------
    begin                : November 2024
    copyright            : (C) 2024 by Stefanos Natsis
    email                : uclaros at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSTACSEARCHPARAMETERSDIALOG_H
#define QGSSTACSEARCHPARAMETERSDIALOG_H

///@cond PRIVATE
#include "qgsrange.h"
#include "qgsstaccollectionlist.h"
#define SIP_NO_FILE

#include "ui_qgsstacsearchparametersdialog.h"

#include <QDialog>

class QgsMapCanvas;
class QStandardItemModel;
class QSortFilterProxyModel;
class QgsStacController;

class QgsStacSearchParametersDialog : public QDialog, private Ui::QgsStacSearchParametersDialog
{
    Q_OBJECT

  public:
    QgsStacSearchParametersDialog( QgsStacController *stac, QgsMapCanvas *canvas, QWidget *parent = nullptr );
    ~QgsStacSearchParametersDialog();

    void accept() override;
    void reject() override;

    void setMapCanvas( QgsMapCanvas *canvas );
    void setCollectionsUrl( const QString &url );

    bool hasTemporalFilter() const;
    bool hasSpatialFilter() const;
    bool hasCollectionsFilter() const;

    QgsGeometry spatialExtent() const;
    QgsDateTimeRange temporalRange() const;
    QSet<QString> selectedCollections() const;

    //! clears model, deletes pointers
    void clearCollections();
    //! takes ownership
    void appendCollections( const QVector<QgsStacCollection *> &collections );

    //! ownership not transferred
    QVector<QgsStacCollection *> collections() const;

    QString activeFiltersPreview();

  private:
    void selectAllCollections();
    void deselectAllCollections();
    void onCollectionsListViewScroll( int value );

    void readTemporalExtentsFromProject();

    QString mCollectionsUrl;
    bool mSpatialFilterEnabled = false;
    bool mTemporalFilterEnabled = false;
    bool mCollectionsFilterEnabled = false;
    QgsRectangle mExtent;
    QgsCoordinateReferenceSystem mExtentCrs;
    QDateTime mTemporalFrom;
    QDateTime mTemporalTo;
    QSet<QString> mSelectedCollections;

    QgsStacController *mStac = nullptr;

    QMenu *mMenu = nullptr;
    QAction *mTemporalExtentFromProjectAction = nullptr;

    QVector<QgsStacCollection *> mCollections;
    QStandardItemModel *mCollectionsModel = nullptr;
    QSortFilterProxyModel *mCollectionsProxyModel = nullptr;
};

///@endcond

#endif // QGSSTACSEARCHPARAMETERSDIALOG_H
