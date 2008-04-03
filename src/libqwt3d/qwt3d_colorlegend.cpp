#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning ( disable : 4305 )
#endif

#include "qwt3d_colorlegend.h"

using namespace Qwt3D;

/**
Contructs a legend object with an axis at the left
side. The legend resides in the top-right area 
and has no caption. Scale numbering is shown.
*/
ColorLegend::ColorLegend()
{
	axis_.setNumbers(true);
	axis_.setScaling(true);
	axis_.setNumberColor(RGBA(0,0,0,1));
	axis_.setNumberAnchor(CenterRight);
	axis_.setNumberFont(QFont("Courier",8));

	caption_.setFont("Courier", 10, QFont::Bold);
	caption_.setColor(RGBA(0,0,0,1));
	axisposition_ = ColorLegend::Left;
	orientation_ = ColorLegend::BottomTop;
	showaxis_ = true;
	setRelPosition(Tuple(0.94, 1-0.36),Tuple(0.97, 1-0.04));
}

void ColorLegend::setTitleString(QString const& s) 
{ 
	caption_.setString(s); 
}

void ColorLegend::setTitleFont(QString const& family, int pointSize, int weight, bool italic) 
{ 
	caption_.setFont(family, pointSize, weight, italic);
}

void ColorLegend::setLimits(double start, double stop)
{
	axis_.setLimits(start, stop);
}

void ColorLegend::setMajors(int majors)
{
	axis_.setMajors(majors);
}

void ColorLegend::setMinors(int minors)
{
	axis_.setMinors(minors);
}

void ColorLegend::setAutoScale(bool val)
{
	axis_.setAutoScale(val);
}

void ColorLegend::setScale(SCALETYPE val)
{
	axis_.setScale(val);
}

void ColorLegend::setScale(Scale* val)
{
	axis_.setScale(val);
}


void ColorLegend::setOrientation(ORIENTATION orientation, SCALEPOSITION pos)
{
	orientation_ = orientation;
	axisposition_ = pos;

	if (orientation_==BottomTop)
	{
		if (axisposition_ == Bottom || axisposition_ == Top)
			axisposition_ = Left;
	}
	else
	{
		if (axisposition_ == Left || axisposition_ == Right)
			axisposition_ = Bottom;
	}
}

void ColorLegend::setRelPosition(Tuple relMin, Tuple relMax)
{
	relMin_ = relMin;
	relMax_ = relMax;
}

void ColorLegend::setGeometryInternal()
{
	double ot = .99;

	getMatrices(modelMatrix, projMatrix, viewport);
	pe_.minVertex = relativePosition(Triple(relMin_.x, relMin_.y, ot));
	pe_.maxVertex = relativePosition(Triple(relMax_.x, relMax_.y, ot));

	double diff = 0;
	Triple b;
	Triple e;

	switch (axisposition_) 
	{
	case ColorLegend::Left:
		b = pe_.minVertex;
		e = pe_.maxVertex; e.x = b.x;
		axis_.setTicOrientation(-1,0,0);
		axis_.setNumberAnchor(CenterRight);
		diff = pe_.maxVertex.x - pe_.minVertex.x;
		break;
	case ColorLegend::Right:
		e = pe_.maxVertex;
		b = pe_.minVertex; b.x = e.x; 
		axis_.setTicOrientation(+1,0,0);
		axis_.setNumberAnchor(CenterLeft);
		diff = pe_.maxVertex.x - pe_.minVertex.x;
		break;
	case ColorLegend::Top:
		e = pe_.maxVertex;
		b = pe_.minVertex; b.z = e.z; 
		axis_.setTicOrientation(0,0,+1);
		axis_.setNumberAnchor(BottomCenter);
		diff = pe_.maxVertex.z - pe_.minVertex.z;
		break;
	case ColorLegend::Bottom:
		b = pe_.minVertex;
		e = pe_.maxVertex; e.z = b.z;
		axis_.setTicOrientation(0,0,-1);
		axis_.setNumberAnchor(TopCenter);
		diff = pe_.maxVertex.z - pe_.minVertex.z;
		break;
	default:
		break;
	}
	
	axis_.setPosition(b,e);
	diff /= 10;
	
	axis_.setTicLength(diff, 0.6*diff);

	Triple c;
	c.x = pe_.minVertex.x + ((pe_.maxVertex-pe_.minVertex) / 2).x;
	c.z = pe_.maxVertex.z;
	c.z += (pe_.maxVertex.z-pe_.minVertex.z)/20;
	c.y = pe_.maxVertex.y;

	caption_.setPosition(c, BottomCenter);	
}

void ColorLegend::draw()
{
	if (colors.empty())
		return;

	setGeometryInternal();

	saveGLState();
  	
	Triple one = pe_.minVertex;
	Triple two = pe_.maxVertex; 
	
	double h = (orientation_ == ColorLegend::BottomTop)
	         ? (two-one).z / colors.size()
					 : (two-one).x / colors.size(); 

	//glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GLStateBewarer(GL_POLYGON_OFFSET_FILL,true);
//	glPolygonOffset(0.0, 0.0);

	glColor4d(0, 0, 0, 1);
	glBegin(GL_LINE_LOOP);
		glVertex3d(one.x, one.y, one.z);
		glVertex3d(one.x, one.y, two.z);
		glVertex3d(two.x, one.y, two.z);
		glVertex3d(two.x, one.y, one.z);
	glEnd();


	unsigned size = colors.size();
	RGBA rgb;
	
	if (orientation_ == ColorLegend::BottomTop)
	{
		for (unsigned i=1; i<=size; ++i) 
		{
			rgb = colors[i-1];
			glColor4d(rgb.r,rgb.g,rgb.b,rgb.a);
	 		glBegin( GL_POLYGON );
				glVertex3d( one.x, one.y, one.z+(i-1)*h );
				glVertex3d( one.x, one.y, one.z+i*h );
				glVertex3d( two.x, one.y, one.z+i*h );
				glVertex3d( two.x, one.y, one.z+(i-1)*h );
			glEnd();
		}
	}
	else
	{
		for (unsigned i=1; i<=size; ++i) 
		{
			rgb = colors[i-1];
			glColor4d(rgb.r,rgb.g,rgb.b,rgb.a);
	 		glBegin( GL_POLYGON );
				glVertex3d( one.x+(i-1)*h, one.y, one.z );
				glVertex3d( one.x+i*h, one.y, one.z );
				glVertex3d( one.x+i*h, one.y, two.z );
				glVertex3d( one.x+(i-1)*h, one.y, two.z );
			glEnd();
		}
	}
			
	restoreGLState();
	
	if (showaxis_)
		axis_.draw();
	
	caption_.draw();
}
