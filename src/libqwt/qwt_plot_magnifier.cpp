/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

// vim: expandtab

#include <math.h>
#include <qevent.h>
#include "qwt_plot.h"
#include "qwt_plot_canvas.h"
#include "qwt_scale_div.h"
#include "qwt_plot_magnifier.h"

class QwtPlotMagnifier::PrivateData
{
public:
    PrivateData():
        isEnabled(false),
        wheelFactor(0.9),
        wheelButtonState(Qt::NoButton),
        mouseFactor(0.95),
        mouseButton(Qt::RightButton),
        mouseButtonState(Qt::NoButton),
        keyFactor(0.9),
        zoomInKey(Qt::Key_Plus),
        zoomOutKey(Qt::Key_Minus),
#if QT_VERSION < 0x040000
        zoomInKeyButtonState(Qt::NoButton),
        zoomOutKeyButtonState(Qt::NoButton),
#else
        zoomInKeyButtonState(Qt::NoModifier),
        zoomOutKeyButtonState(Qt::NoModifier),
#endif
        mousePressed(false)
    {
        for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
            isAxisEnabled[axis] = true;
    }

    bool isEnabled;

    double wheelFactor;
    int wheelButtonState;

    double mouseFactor;
    int mouseButton;
    int mouseButtonState;

    double keyFactor;
    int zoomInKey;
    int zoomOutKey;
    int zoomInKeyButtonState;
    int zoomOutKeyButtonState;

    bool isAxisEnabled[QwtPlot::axisCnt];

    bool mousePressed;
    bool hasMouseTracking;
    QPoint mousePos;
};

QwtPlotMagnifier::QwtPlotMagnifier(QwtPlotCanvas *canvas):
    QObject(canvas)
{
    d_data = new PrivateData();
    setEnabled(true);
}

QwtPlotMagnifier::~QwtPlotMagnifier()
{
    delete d_data;
}

void QwtPlotMagnifier::setEnabled(bool on)
{
    if ( d_data->isEnabled != on )
    {
        d_data->isEnabled = on;

        QObject *o = parent();
        if ( o )
        {
            if ( d_data->isEnabled )
                o->installEventFilter(this);
            else
                o->removeEventFilter(this);
        }
    }
}

bool QwtPlotMagnifier::isEnabled() const
{
    return d_data->isEnabled;
}

void QwtPlotMagnifier::setWheelFactor(double factor)
{
    d_data->wheelFactor = factor;
}

double QwtPlotMagnifier::wheelFactor() const
{
    return d_data->wheelFactor;
}

void QwtPlotMagnifier::setWheelButtonState(int buttonState)
{
    d_data->wheelButtonState = buttonState;
}

int QwtPlotMagnifier::wheelButtonState() const
{
    return d_data->wheelButtonState;
}

void QwtPlotMagnifier::setMouseFactor(double factor)
{
    d_data->mouseFactor = factor;
}

double QwtPlotMagnifier::mouseFactor() const
{
    return d_data->mouseFactor;
}

void QwtPlotMagnifier::setMouseButton(int button, int buttonState)
{
    d_data->mouseButton = button;
    d_data->mouseButtonState = buttonState;
}

void QwtPlotMagnifier::getMouseButton(
    int &button, int &buttonState) const
{
    button = d_data->mouseButton;
    buttonState = d_data->mouseButtonState;
}

void QwtPlotMagnifier::setKeyFactor(double factor)
{
    d_data->keyFactor = factor;
}

double QwtPlotMagnifier::keyFactor() const
{
    return d_data->keyFactor;
}

void QwtPlotMagnifier::setZoomInKey(int key, int buttonState)
{
    d_data->zoomInKey = key;
    d_data->zoomInKeyButtonState = buttonState;
}

void QwtPlotMagnifier::getZoomInKey(int &key, int &buttonState)
{
    key = d_data->zoomInKey;
    buttonState = d_data->zoomInKeyButtonState;
}

void QwtPlotMagnifier::setZoomOutKey(int key, int buttonState)
{
    d_data->zoomOutKey = key;
    d_data->zoomOutKeyButtonState = buttonState;
}

void QwtPlotMagnifier::getZoomOutKey(int &key, int &buttonState)
{
    key = d_data->zoomOutKey;
    buttonState = d_data->zoomOutKeyButtonState;
}

void QwtPlotMagnifier::setAxisEnabled(int axis, bool on)
{
    if ( axis >= 0 && axis < QwtPlot::axisCnt )
        d_data->isAxisEnabled[axis] = on;
}

bool QwtPlotMagnifier::isAxisEnabled(int axis) const
{
    if ( axis >= 0 && axis < QwtPlot::axisCnt )
        return d_data->isAxisEnabled[axis];

    return true;
}

//! Return observed plot canvas
QwtPlotCanvas *QwtPlotMagnifier::canvas()
{
    QObject *w = parent();
    if ( w && w->inherits("QwtPlotCanvas") )
        return (QwtPlotCanvas *)w;

    return NULL;
}

//! Return Observed plot canvas
const QwtPlotCanvas *QwtPlotMagnifier::canvas() const
{
    return ((QwtPlotMagnifier *)this)->canvas();
}

//! Return plot widget, containing the observed plot canvas
QwtPlot *QwtPlotMagnifier::plot()
{
    QObject *w = canvas();
    if ( w )
    {
        w = w->parent();
        if ( w && w->inherits("QwtPlot") )
            return (QwtPlot *)w;
    }

    return NULL;
}

//! Return plot widget, containing the observed plot canvas
const QwtPlot *QwtPlotMagnifier::plot() const
{
    return ((QwtPlotMagnifier *)this)->plot();
}

bool QwtPlotMagnifier::eventFilter(QObject *o, QEvent *e)
{
    if ( o && o == parent() )
    {
        switch(e->type() )
        {
            case QEvent::MouseButtonPress:
            {
                widgetMousePressEvent((QMouseEvent *)e);
                break;
            }
            case QEvent::MouseMove:
            {
                widgetMouseMoveEvent((QMouseEvent *)e);
                break;
            }
            case QEvent::MouseButtonRelease:
            {
                widgetMouseReleaseEvent((QMouseEvent *)e);
                break;
            }
            case QEvent::Wheel:
            {
                widgetWheelEvent((QWheelEvent *)e);
                break;
            }
            case QEvent::KeyPress:
            {
                widgetKeyPressEvent((QKeyEvent *)e);
                break;
            }
            case QEvent::KeyRelease:
            {
                widgetKeyReleaseEvent((QKeyEvent *)e);
                break;
            }
            default:;
        }
    }
    return QObject::eventFilter(o, e);
}

void QwtPlotMagnifier::widgetMousePressEvent(QMouseEvent *me)
{
    if ( me->button() != d_data->mouseButton )
        return;

#if QT_VERSION < 0x040000
    if ( (me->state() & Qt::KeyButtonMask) !=
        (d_data->mouseButtonState & Qt::KeyButtonMask) )
#else
    if ( (me->modifiers() & Qt::KeyboardModifierMask) !=
        (int)(d_data->mouseButtonState & Qt::KeyboardModifierMask) )
#endif
    {
        return;
    }

    d_data->hasMouseTracking = canvas()->hasMouseTracking();
    canvas()->setMouseTracking(true);
    d_data->mousePos = me->pos();
    d_data->mousePressed = true;
}

void QwtPlotMagnifier::widgetMouseReleaseEvent(QMouseEvent *)
{
    if ( d_data->mousePressed )
    {
        d_data->mousePressed = false;
        canvas()->setMouseTracking(d_data->hasMouseTracking);
    }
}

void QwtPlotMagnifier::widgetMouseMoveEvent(QMouseEvent *me)
{
    if ( !d_data->mousePressed )
        return;

    const int dy = me->pos().y() - d_data->mousePos.y();
    if ( dy != 0 )
    {
        double f = d_data->mouseFactor;
        if ( dy < 0 )
            f = 1 / f;

        rescale(f);
    }

    d_data->mousePos = me->pos();
}

void QwtPlotMagnifier::widgetWheelEvent(QWheelEvent *we)
{
#if QT_VERSION < 0x040000
    if ( (we->state() & Qt::KeyButtonMask) !=
        (d_data->wheelButtonState & Qt::KeyButtonMask) )
#else
    if ( (we->modifiers() & Qt::KeyboardModifierMask) !=
        (int)(d_data->wheelButtonState & Qt::KeyboardModifierMask) )
#endif
    {
        return;
    }

    if ( d_data->wheelFactor != 0.0 )
    {
       /*
           A positive delta indicates that the wheel was 
           rotated forwards away from the user; a negative 
           value indicates that the wheel was rotated 
           backwards toward the user.
           Most mouse types work in steps of 15 degrees, 
           in which case the delta value is a multiple 
           of 120 (== 15 * 8).
        */
        double f = ::pow(d_data->wheelFactor, 
            qwtAbs(we->delta() / 120));
        if ( we->delta() > 0 )
            f = 1 / f;

        rescale(f);
    }
}

void QwtPlotMagnifier::widgetKeyPressEvent(QKeyEvent *ke)
{
    const int key = ke->key();
#if QT_VERSION < 0x040000
    const int state = ke->state();
#else
    const int state = ke->modifiers();
#endif

    if ( key == d_data->zoomInKey && 
        state == d_data->zoomInKeyButtonState )
    {
        rescale(d_data->keyFactor);
    }
    else if ( key == d_data->zoomOutKey && 
        state == d_data->zoomOutKeyButtonState )
    {
        rescale(1.0 / d_data->keyFactor);
    }
}

void QwtPlotMagnifier::widgetKeyReleaseEvent(QKeyEvent *)
{
}

void QwtPlotMagnifier::rescale(double factor)
{
    if ( factor == 1.0 || factor == 0.0 )
        return;

    bool doReplot = false;
    QwtPlot* plt = plot();

    const bool autoReplot = plt->autoReplot();
    plt->setAutoReplot(false);

    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        const QwtScaleDiv *scaleDiv = plt->axisScaleDiv(axisId);
        if ( isAxisEnabled(axisId) && scaleDiv->isValid() )
        {
            const double center =
                scaleDiv->lBound() + scaleDiv->range() / 2;
            const double width_2 = scaleDiv->range() / 2 * factor;

            plt->setAxisScale(axisId, center - width_2, center + width_2);
            doReplot = true;
        }
    }

    plt->setAutoReplot(autoReplot);

    if ( doReplot )
        plt->replot();
}
