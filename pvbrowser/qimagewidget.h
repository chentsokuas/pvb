/***************************************************************************
                          QImageWidget.h  -  description
                             -------------------
    begin                : Fri Nov 17 2000
    copyright            : (C) 2000 by Rainer Lehrig
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _QIMAGE_WIDGET_H_
#define _QIMAGE_WIDGET_H_

#include <qwidget.h>
#include <qimage.h>
#include <qbitmap.h>
//Added by qt3to4:
#include <QPaintEvent>
#include <QMouseEvent>

class QImageWidget : public QWidget
{
    Q_OBJECT
public:
    QImageWidget(int *sock, int ident, QWidget *parent=0, const char *name=0, int wFlags=0 );
    ~QImageWidget();
    virtual void    setGeometry(int nx, int ny, int nw, int nh);
    void            setImage(const QImage *newimage);
    void            setImage(const char *filename);
    QImage          image; // the loaded image

protected:
    virtual void paintEvent(QPaintEvent *);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void enterEvent(QEvent *event);
    virtual void leaveEvent(QEvent *event);

private:
    void perhapsSetMask();
    int conversion_flags;
    int xx,yy,w,h,*s,id;
    QBitmap mask;
};


#endif