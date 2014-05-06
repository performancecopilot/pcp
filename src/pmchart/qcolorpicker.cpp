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

#include <QApplication>
#include <QPainter>
#include "qcolorpicker.h"

static int pWidth = 172;
static int pHeight = 172;

int QColorLuminancePicker::y2val(int y)
{
    int d = height() - 2*coff - 1;
    return 255 - (y - coff)*255/d;
}

int QColorLuminancePicker::val2y(int v)
{
    int d = height() - 2*coff - 1;
    return coff + (255-v)*d/255;
}

QColorLuminancePicker::QColorLuminancePicker(QWidget* parent)
    :QWidget(parent)
{
    hue = 100; val = 100; sat = 100;
    pix = 0;
    //    setAtribute(QA_NoErase, true);
}

QColorLuminancePicker::~QColorLuminancePicker()
{
    delete pix;
}

void QColorLuminancePicker::mouseMoveEvent(QMouseEvent *m)
{
    setVal(y2val(m->y()));
}
void QColorLuminancePicker::mousePressEvent( QMouseEvent *m )
{
    setVal(y2val(m->y()));
}

void QColorLuminancePicker::setVal(int v)
{
    if (val == v)
	return;
    val = qMax(0, qMin(v,255));
    delete pix; pix=0;
    repaint();
    Q_EMIT newHsv(hue, sat, val);
}

//receives from a hue,sat chooser and relays.
void QColorLuminancePicker::setCol(int h, int s)
{
    setCol(h, s, val);
    Q_EMIT newHsv(h, s, val);
}

void QColorLuminancePicker::paintEvent(QPaintEvent *)
{
    int w = width() - 5;

    QRect r(0, foff, w, height() - 2*foff);
    int wi = r.width() - 2;
    int hi = r.height() - 2;
    if (!pix || pix->height() != hi || pix->width() != wi) {
	delete pix;
	QImage img(wi, hi, QImage::Format_RGB32);
	int y;
	for (y = 0; y < hi; y++) {
	    QColor c;
	    c.setHsv(hue, sat, y2val(y+coff));
	    QRgb r = c.rgb();
	    int x;
	    for (x = 0; x < wi; x++)
		img.setPixel(x, y, r);
	}
	pix = new QPixmap(QPixmap::fromImage(img));
    }
    QPainter p(this);
    p.drawPixmap(1, coff, *pix);
    const QPalette &g = palette();
    qDrawShadePanel(&p, r, g, true);
    p.setPen(g.foreground().color());
    p.setBrush(g.foreground());
    QPolygon a;
    int y = val2y(val);
    a.setPoints(3, w, y, w+5, y+5, w+5, y-5);
    p.eraseRect(w, 0, 5, height());
    p.drawPolygon(a);
}

void QColorLuminancePicker::setCol( int h, int s , int v )
{
    val = v;
    hue = h;
    sat = s;
    delete pix; pix=0;
    repaint();
}

QPoint QColorPicker::colPt()
{ return QPoint((360-hue)*(pWidth-1)/360, (255-sat)*(pHeight-1)/255); }
int QColorPicker::huePt(const QPoint &pt)
{ return 360 - pt.x()*360/(pWidth-1); }
int QColorPicker::satPt(const QPoint &pt)
{ return 255 - pt.y()*255/(pHeight-1) ; }
void QColorPicker::setCol(const QPoint &pt)
{ setCol(huePt(pt), satPt(pt)); }

QColorPicker::QColorPicker(QWidget* parent)
    : QFrame(parent)
{
    hue = 0; sat = 0;
    setCol(150, 255);

    QImage img(pWidth, pHeight, QImage::Format_RGB32);
    int x,y;
    for (y = 0; y < pHeight; y++)
	for (x = 0; x < pWidth; x++) {
	    QPoint p(x, y);
	    QColor c;
	    c.setHsv(huePt(p), satPt(p), 200);
	    img.setPixel(x, y, c.rgb());
	}
    pix = new QPixmap(QPixmap::fromImage(img));
    setAttribute(Qt::WA_NoSystemBackground);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed) );
}

QColorPicker::~QColorPicker()
{
    delete pix;
}

QSize QColorPicker::sizeHint() const
{
    return QSize(pWidth + 2*frameWidth(), pHeight + 2*frameWidth());
}

void QColorPicker::setCol(int h, int s)
{
    int nhue = qMin(qMax(0,h), 359);
    int nsat = qMin(qMax(0,s), 255);
    if (nhue == hue && nsat == sat)
	return;
    QRect r(colPt(), QSize(20,20));
    hue = nhue; sat = nsat;
    r = r.unite(QRect(colPt(), QSize(20,20)));
    r.translate(contentsRect().x()-9, contentsRect().y()-9);
    //    update(r);
    repaint(r);
}

void QColorPicker::mouseMoveEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    Q_EMIT newCol(hue, sat);
}

void QColorPicker::mousePressEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    Q_EMIT newCol(hue, sat);
}

void QColorPicker::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
    QPainter p(this);
    QRect r = contentsRect();

    p.drawPixmap(r.topLeft(), *pix);
    QPoint pt = colPt() + r.topLeft();
    p.setPen(Qt::black);

    p.fillRect(pt.x()-9, pt.y(), 20, 2, Qt::black);
    p.fillRect(pt.x(), pt.y()-9, 2, 20, Qt::black);
}

void QColorShowLabel::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
    QPainter p(this);
    p.fillRect(contentsRect()&e->rect(), col);
}

void QColorShowLabel::mousePressEvent(QMouseEvent *e)
{
    mousePressed = true;
    pressPos = e->pos();
}

void QColorShowLabel::mouseMoveEvent(QMouseEvent *e)
{
#ifdef QT_NO_DRAGANDDROP
    Q_UNUSED(e);
#else
    if (!mousePressed)
	return;
    if ((pressPos - e->pos()).manhattanLength() > QApplication::startDragDistance()) {
	QMimeData *mime = new QMimeData;
	mime->setColorData(col);
	QPixmap pix(30, 20);
	pix.fill(col);
	QPainter p(&pix);
	p.drawRect(0, 0, pix.width(), pix.height());
	p.end();
	QDrag *drg = new QDrag(this);
	drg->setMimeData(mime);
	drg->setPixmap(pix);
	mousePressed = false;
	drg->start();
    }
#endif
}

#ifndef QT_NO_DRAGANDDROP
void QColorShowLabel::dragEnterEvent(QDragEnterEvent *e)
{
    if (qvariant_cast<QColor>(e->mimeData()->colorData()).isValid())
	e->accept();
    else
	e->ignore();
}

void QColorShowLabel::dragLeaveEvent(QDragLeaveEvent *)
{
}

void QColorShowLabel::dropEvent(QDropEvent *e)
{
    QColor color = qvariant_cast<QColor>(e->mimeData()->colorData());
    if (color.isValid()) {
	col = color;
	repaint();
	Q_EMIT colorDropped(col.rgb());
	e->accept();
    } else {
	e->ignore();
    }
}
#endif // QT_NO_DRAGANDDROP

void QColorShowLabel::mouseReleaseEvent( QMouseEvent * )
{
    if (!mousePressed)
	return;
    mousePressed = false;
}

QColLineEdit::QColLineEdit(QWidget *parent) : QLineEdit(parent)
{
    connect(this, SIGNAL(textEdited(const QString&)),
	    this, SLOT(textEdited(const QString&)));
}

void QColLineEdit::textEdited(const QString &text)
{
    QColor c;
    c.setNamedColor(text);
    if (c.isValid()) {
	setColor(c);
	Q_EMIT newColor(c);
    }
}

void QColLineEdit::setColor(QColor c)
{
    col = c;
    setText(col.name());
}

void QColLineEdit::setCol(int h, int s, int v)
{
    col.setHsv(h, s, v);
    setText(col.name());
}
