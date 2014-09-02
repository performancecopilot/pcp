/*
** Copyright (C) 2014 Red Hat.
** Copyright (C) 1999-2007 Trolltech AS.  All rights reserved.
**
** This file is derived from part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**/
#ifndef QED_COLORPICKER_H
#define QED_COLORPICKER_H

#include <QFrame>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QValidator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDragLeaveEvent>

static inline void rgb2hsv(QRgb rgb, int &h, int &s, int &v)
{
    QColor c;
    c.setRgb(rgb);
    c.getHsv(&h, &s, &v);
}

class QedColorPicker : public QFrame
{
    Q_OBJECT
public:
    QedColorPicker(QWidget* parent);
    ~QedColorPicker();

public slots:
    void setCol(int h, int s);

Q_SIGNALS:
    void newCol(int h, int s);

protected:
    QSize sizeHint() const;
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);

private:
    int hue;
    int sat;

    QPoint colPt();
    int huePt(const QPoint &pt);
    int satPt(const QPoint &pt);
    void setCol(const QPoint &pt);

    QPixmap *pix;
};

class QedColorLuminancePicker : public QWidget
{
    Q_OBJECT
public:
    QedColorLuminancePicker(QWidget* parent);
    ~QedColorLuminancePicker();

public slots:
    void setCol(int h, int s, int v);
    void setCol(int h, int s);

Q_SIGNALS:
    void newHsv(int h, int s, int v);

protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);

private:
    enum { foff = 3, coff = 4 }; //frame and contents offset
    int val;
    int hue;
    int sat;

    int y2val(int y);
    int val2y(int val);
    void setVal(int v);

    QPixmap *pix;
};

class QedColSpinBox : public QSpinBox
{
public:
    QedColSpinBox(QWidget *parent)
	: QSpinBox(parent) { setRange(0, 255); }
    void setValue(int i) {
	bool block = signalsBlocked();
	blockSignals(true);
	QSpinBox::setValue(i);
	blockSignals(block);
    }
};

class QedColLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    QedColLineEdit(QWidget *parent);

public slots:
    void setColor(QColor c);
    void setCol(int h, int s, int v);
    void textEdited(const QString &text);

Q_SIGNALS:
    void newColor(QColor c);

private:
    QColor col;
};

class QedColorShowLabel : public QFrame
{
    Q_OBJECT

public:
    QedColorShowLabel(QWidget *parent) : QFrame(parent) {
	setFrameStyle(QFrame::Panel|QFrame::Sunken);
	setAcceptDrops(true);
	mousePressed = false;
    }
    void setColor(QColor c) { col = c; update(); }

Q_SIGNALS:
    void colorDropped(QRgb);

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
#ifndef QT_NO_DRAGANDDROP
    void dragEnterEvent(QDragEnterEvent *e);
    void dragLeaveEvent(QDragLeaveEvent *e);
    void dropEvent(QDropEvent *e);
#endif

private:
    QColor col;
    bool mousePressed;
    QPoint pressPos;
};

#endif	/* QED_COLORPICKER_H */
