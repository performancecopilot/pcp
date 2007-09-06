/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#ifndef QWT_PLOT_MAGNIFIER_H
#define QWT_PLOT_MAGNIFIER_H 1

#include "qwt_global.h"
#include <qobject.h>

class QwtPlotCanvas;
class QwtPlot;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

/*!
  \brief QwtPlotMagnifier provides zooming, by magnifying in steps.

  Together with QwtPlotZoomer and QwtPlotPanner it is possible to implement
  individual and powerful navigation of the plot canvas.

  \sa QwtPlotZoomer, QwtPlotPanner, QwtPlot
*/
class QWT_EXPORT QwtPlotMagnifier: public QObject
{
    Q_OBJECT

public:
    explicit QwtPlotMagnifier(QwtPlotCanvas *);
    virtual ~QwtPlotMagnifier();

    void setEnabled(bool);
    bool isEnabled() const;

    void setAxisEnabled(int axis, bool on);
    bool isAxisEnabled(int axis) const;

    // mouse
    void setMouseFactor(double);
    double mouseFactor() const;

    void setMouseButton(int button, int buttonState = Qt::NoButton);
    void getMouseButton(int &button, int &buttonState) const;

    // mouse wheel
    void setWheelFactor(double);
    double wheelFactor() const;

    void setWheelButtonState(int buttonState);
    int wheelButtonState() const;

    // keyboard
    void setKeyFactor(double);
    double keyFactor() const;

    void setZoomInKey(int key, int buttonState);
    void getZoomInKey(int &key, int &buttonState);

    void setZoomOutKey(int key, int buttonState);
    void getZoomOutKey(int &key, int &buttonState);

    QwtPlotCanvas *canvas();
    const QwtPlotCanvas *canvas() const;

    QwtPlot *plot();
    const QwtPlot *plot() const;

    virtual bool eventFilter(QObject *, QEvent *);

protected:
    virtual void rescale(double factor);

    virtual void widgetMousePressEvent(QMouseEvent *);
    virtual void widgetMouseReleaseEvent(QMouseEvent *);
    virtual void widgetMouseMoveEvent(QMouseEvent *);
    virtual void widgetWheelEvent(QWheelEvent *);
    virtual void widgetKeyPressEvent(QKeyEvent *);
    virtual void widgetKeyReleaseEvent(QKeyEvent *);

private:
    class PrivateData;
    PrivateData *d_data;
};

#endif
