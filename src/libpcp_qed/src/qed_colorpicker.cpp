/*
 * Copyright (C) 2014-2015 Red Hat.
 * Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 * Contact: http://www.qt-project.org/legal
 *
 * This file derives from the QtWidgets module of the Qt Toolkit.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include <QApplication>
#include <QPainter>
#include "qed_colorpicker.h"

static int pWidth = 172;
static int pHeight = 172;

int QedColorLuminancePicker::y2val(int y)
{
    int d = height() - 2*coff - 1;
    return 255 - (y - coff)*255/d;
}

int QedColorLuminancePicker::val2y(int v)
{
    int d = height() - 2*coff - 1;
    return coff + (255-v)*d/255;
}

QedColorLuminancePicker::QedColorLuminancePicker(QWidget* parent)
    :QWidget(parent)
{
    hue = 100; val = 100; sat = 100;
    pix = 0;
    //    setAtribute(QA_NoErase, true);
}

QedColorLuminancePicker::~QedColorLuminancePicker()
{
    delete pix;
}

void QedColorLuminancePicker::mouseMoveEvent(QMouseEvent *m)
{
    setVal(y2val(m->y()));
}
void QedColorLuminancePicker::mousePressEvent( QMouseEvent *m )
{
    setVal(y2val(m->y()));
}

void QedColorLuminancePicker::setVal(int v)
{
    if (val == v)
	return;
    val = qMax(0, qMin(v,255));
    delete pix; pix=0;
    repaint();
    Q_EMIT newHsv(hue, sat, val);
}

//receives from a hue,sat chooser and relays.
void QedColorLuminancePicker::setCol(int h, int s)
{
    setCol(h, s, val);
    Q_EMIT newHsv(h, s, val);
}

void QedColorLuminancePicker::paintEvent(QPaintEvent *)
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

void QedColorLuminancePicker::setCol( int h, int s , int v )
{
    val = v;
    hue = h;
    sat = s;
    delete pix; pix=0;
    repaint();
}

QPoint QedColorPicker::colPt()
{ return QPoint((360-hue)*(pWidth-1)/360, (255-sat)*(pHeight-1)/255); }

int QedColorPicker::huePt(const QPoint &pt)
{ return 360 - pt.x()*360/(pWidth-1); }

int QedColorPicker::satPt(const QPoint &pt)
{ return 255 - pt.y()*255/(pHeight-1) ; }

void QedColorPicker::setCol(const QPoint &pt)
{ setCol(huePt(pt), satPt(pt)); }

QedColorPicker::QedColorPicker(QWidget* parent)
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

QedColorPicker::~QedColorPicker()
{
    delete pix;
}

QSize QedColorPicker::sizeHint() const
{
    return QSize(pWidth + 2*frameWidth(), pHeight + 2*frameWidth());
}

void QedColorPicker::setCol(int h, int s)
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

void QedColorPicker::mouseMoveEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    Q_EMIT newCol(hue, sat);
}

void QedColorPicker::mousePressEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    Q_EMIT newCol(hue, sat);
}

void QedColorPicker::paintEvent(QPaintEvent *e)
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

void QedColorShowLabel::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
    QPainter p(this);
    p.fillRect(contentsRect()&e->rect(), col);
}

void QedColorShowLabel::mousePressEvent(QMouseEvent *e)
{
    mousePressed = true;
    pressPos = e->pos();
}

void QedColorShowLabel::mouseMoveEvent(QMouseEvent *e)
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
void QedColorShowLabel::dragEnterEvent(QDragEnterEvent *e)
{
    if (qvariant_cast<QColor>(e->mimeData()->colorData()).isValid())
	e->accept();
    else
	e->ignore();
}

void QedColorShowLabel::dragLeaveEvent(QDragLeaveEvent *)
{
}

void QedColorShowLabel::dropEvent(QDropEvent *e)
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

void QedColorShowLabel::mouseReleaseEvent( QMouseEvent * )
{
    if (!mousePressed)
	return;
    mousePressed = false;
}

QedColLineEdit::QedColLineEdit(QWidget *parent) : QLineEdit(parent)
{
    connect(this, SIGNAL(textEdited(const QString&)),
	    this, SLOT(textEdited(const QString&)));
}

void QedColLineEdit::textEdited(const QString &text)
{
    QColor c;
    c.setNamedColor(text);
    if (c.isValid()) {
	setColor(c);
	Q_EMIT newColor(c);
    }
}

void QedColLineEdit::setColor(QColor c)
{
    col = c;
    setText(col.name());
}

void QedColLineEdit::setCol(int h, int s, int v)
{
    col.setHsv(h, s, v);
    setText(col.name());
}
