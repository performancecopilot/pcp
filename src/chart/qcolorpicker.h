/*
 * Copyright (C) 1999-2005 Trolltech AS.  All rights reserved.
 *
 * This file was derived from the QT QColorDialog class
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
 * licenses may use this file in accordance with the Qt Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef QCOLORPICKER_H
#define QCOLORPICKER_H

#include <qframe.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qvalidator.h>

static inline void rgb2hsv(QRgb rgb, int &h, int &s, int &v)
{
    QColor c;
    c.setRgb(rgb);
    c.getHsv(h,s,v);
}

class QColorPicker : public QFrame
{
    Q_OBJECT
public:
    QColorPicker(QWidget* parent=0, const char* name=0);
    ~QColorPicker();

public slots:
    void setCol( int h, int s );

signals:
    void newCol( int h, int s );

protected:
    QSize sizeHint() const;
    void drawContents(QPainter* p);
    void mouseMoveEvent( QMouseEvent * );
    void mousePressEvent( QMouseEvent * );

private:
    int hue;
    int sat;

    QPoint colPt();
    int huePt( const QPoint &pt );
    int satPt( const QPoint &pt );
    void setCol( const QPoint &pt );

    QPixmap *pix;
};

class QColorLuminancePicker : public QWidget
{
    Q_OBJECT
public:
    QColorLuminancePicker(QWidget* parent=0, const char* name=0);
    ~QColorLuminancePicker();

public slots:
    void setCol( int h, int s, int v );
    void setCol( int h, int s );

signals:
    void newHsv( int h, int s, int v );

protected:
    void paintEvent( QPaintEvent*);
    void mouseMoveEvent( QMouseEvent * );
    void mousePressEvent( QMouseEvent * );

private:
    enum { foff = 3, coff = 4 }; //frame and contents offset
    int val;
    int hue;
    int sat;

    int y2val( int y );
    int val2y( int val );
    void setVal( int v );

    QPixmap *pix;
};

class QColIntValidator: public QIntValidator
{
public:
    QColIntValidator( int bottom, int top,
		   QWidget * parent, const char *name = 0 )
	:QIntValidator( bottom, top, parent, name ) {}

    QValidator::State validate( QString &, int & ) const;
};

class QColNumLineEdit : public QLineEdit
{
public:
    QColNumLineEdit( QWidget *parent, const char* name=0 )
	: QLineEdit( parent, name ) { setMaxLength( 3 );}
    QSize sizeHint() const {
	return QSize( fontMetrics().width( "999" ) + 2 * ( margin() + frameWidth() ),
		      QLineEdit::sizeHint().height() ); }
    void setNum( int i ) {
	QString s;
	s.setNum(i);
	bool block = signalsBlocked();
	blockSignals(TRUE);
	setText( s );
	blockSignals(block);
    }
    int val() const { return text().toInt(); }
};

class QColorShowLabel : public QFrame
{
    Q_OBJECT
public:
    QColorShowLabel( QWidget *parent, const char *name=0)
	: QFrame( parent, name ) {
	setFrameStyle( QFrame::Panel|QFrame::Sunken );
	setBackgroundMode( PaletteBackground );
	setAcceptDrops( TRUE );
	mousePressed = FALSE;
    }
    void setColor( QColor c ) { col = c; }

signals:
    void colorDropped( QRgb );

protected:
    void drawContents( QPainter *p );
    void mousePressEvent( QMouseEvent *e );
    void mouseMoveEvent( QMouseEvent *e );
    void mouseReleaseEvent( QMouseEvent *e );
#ifndef QT_NO_DRAGANDDROP
    void dragEnterEvent( QDragEnterEvent *e );
    void dragLeaveEvent( QDragLeaveEvent *e );
    void dropEvent( QDropEvent *e );
#endif

private:
    QColor col;
    bool mousePressed;
    QPoint pressPos;
};

#endif	/* QCOLORPICKER_H */
