#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning ( disable : 4305 )
#pragma warning ( disable : 4786 )
#endif

#include <float.h>
#include "qwt3d_plot.h"

using namespace Qwt3D;


/**
  Set the rotation angle of the object. If you look along the respective axis towards ascending values,
	the rotation is performed in mathematical \e negative sense 
	\param xVal angle in \e degree to rotate around the X axis
	\param yVal angle in \e degree to rotate around the Y axis
	\param zVal angle in \e degree to rotate around the Z axis
*/
void Plot3D::setRotation( double xVal, double yVal, double zVal )
{
  if (xRot_ == xVal && yRot_ == yVal && zRot_ == zVal)
		return;
	
	xRot_ = xVal;
	yRot_ = yVal;
	zRot_ = zVal;
  
	updateGL();
	emit rotationChanged(xVal, yVal, zVal);
}

/**
  Set the shift in object (world) coordinates.
	\param xVal shift along (world) X axis
	\param yVal shift along (world) Y axis
	\param zVal shift along (world) Z axis
	\see setViewportShift()
*/
void Plot3D::setShift( double xVal, double yVal, double zVal )
{
  if (xShift_ == xVal && yShift_ == yVal && zShift_ == zVal)
		return;
	
	xShift_ = xVal;
	yShift_ = yVal;
	zShift_ = zVal;
	updateGL();
	emit shiftChanged(xVal, yVal, zVal);
}

/**
  Performs shifting along screen axes. 
  The shift moves points inside a sphere, 
  which encloses the unscaled and unzoomed data
	by multiples of the spheres diameter
	
	\param xVal shift along (view) X axis
	\param yVal shift along (view) Y axis
	\see setShift()
*/
void Plot3D::setViewportShift( double xVal, double yVal )
{
  if (xVPShift_ == xVal && yVPShift_ == yVal)
		return;
	
  xVPShift_ = xVal;
	yVPShift_ = yVal;
		
	updateGL();
	emit vieportShiftChanged(xVPShift_, yVPShift_);
}

/**
  Set the scale in object (world) coordinates.
	\param xVal scaling for X values
	\param yVal scaling for Y values
	\param zVal scaling for Z values 

	A respective value of 1 represents no scaling;
*/
void Plot3D::setScale( double xVal, double yVal, double zVal )
{
  if (xScale_ == xVal && yScale_ == yVal && zScale_ == zVal)
		return;
	
	xScale_ = (xVal < DBL_EPSILON ) ? DBL_EPSILON : xVal;
	yScale_ = (yVal < DBL_EPSILON ) ? DBL_EPSILON : yVal;
	zScale_ = (zVal < DBL_EPSILON ) ? DBL_EPSILON : zVal;

	updateGL();
	emit scaleChanged(xVal, yVal, zVal);
}

/**
  Set the (zoom in addition to scale).
	\param val zoom value (value == 1 indicates no zooming)
*/
void Plot3D::setZoom( double val )
{
  if (zoom_ == val)
		return;

	zoom_ = (val < DBL_EPSILON ) ? DBL_EPSILON : val;
 	updateGL();
	emit zoomChanged(val);
}
