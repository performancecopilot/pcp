#ifndef __PLANE_H__
#define __PLANE_H__

#include "qwt3d_global.h"
#include "qwt3d_drawable.h"
#include "qwt3d_axis.h"
#include "qwt3d_color.h"

namespace Qwt3D
{

//! A flat color legend
/**
	The class visualizes a ColorVector together with a scale (axis)  and a caption. ColorLegends are vertical 
	or horizontal
*/
class QWT3D_EXPORT ColorLegend : public Drawable
{

public:
	
	//! Possible anchor points for caption and axis
	enum SCALEPOSITION
	{
		Top,      //!< scale on top
    Bottom,   //!< scale on bottom
		Left,     //!< scale left
		Right     //!< scale right
	};
	
	//! Orientation of the legend
	enum ORIENTATION
	{
    BottomTop, //!< Positionate the legend vertically, the lowest color index is on the bottom
		LeftRight  //!< Positionate the legend horizontally, the lowest color index is on left side
	};

	ColorLegend(); //!< Standard constructor

	void draw(); //!< Draws the object. You should not use this explicitely - the function is called by updateGL().
	
	void setRelPosition(Qwt3D::Tuple relMin, Qwt3D::Tuple relMax); //!< Sets the relative position of the legend inside widget
	void setOrientation(ORIENTATION, SCALEPOSITION); //!< Sets legend orientation and scale position
	void setLimits(double start, double stop); //!< Sets the limit of the scale.
	void setMajors(int); //!< Sets scale major tics.
	void setMinors(int); //!< Sets scale minor tics.
	void drawScale(bool val) { showaxis_ = val; } //!< Sets whether a scale will be drawn.
	void drawNumbers(bool val) { axis_.setNumbers(val); } //!< Sets whether the scale will have scale numbers.
	void setAutoScale(bool val); //!< Sets, whether the axis is autoscaled or not.
  void setScale(Qwt3D::Scale *scale); //!< Sets another scale
  void setScale(Qwt3D::SCALETYPE); //!< Sets one of the predefined scale types

	void setTitleString(QString const& s); //!< Sets the legends caption string.
	
	//! Sets the legends caption font.
	void setTitleFont(QString const& family, int pointSize, int weight = QFont::Normal, bool italic = false); 

	Qwt3D::ColorVector colors; //!< The color vector

private:
	
	Qwt3D::Label caption_;
	Qwt3D::ParallelEpiped geometry() const { return pe_;}
	void setGeometryInternal();

	Qwt3D::ParallelEpiped pe_;
	Qwt3D::Tuple relMin_, relMax_;
	Qwt3D::Axis axis_;
	SCALEPOSITION axisposition_;
	ORIENTATION orientation_;

	bool showaxis_;
};

} // ns

#endif
