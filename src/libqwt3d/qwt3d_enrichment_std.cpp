#include <math.h>
#include "qwt3d_color.h"
#include "qwt3d_plot.h"
#include "qwt3d_enrichment_std.h"

using namespace Qwt3D;


/////////////////////////////////////////////////////////////////
//
//   CrossHair
//
/////////////////////////////////////////////////////////////////

CrossHair::CrossHair()
{
  configure(0, 1, false, false);
}

CrossHair::CrossHair(double rad, double linewidth, bool smooth, bool boxed)
{
  configure(rad, linewidth, smooth, boxed);
}

void CrossHair::configure(double rad, double linewidth, bool smooth, bool boxed)
{
  plot = 0;
  radius_ = rad;
  linewidth_ = linewidth;
  smooth_ = smooth;
  boxed_ = boxed;
}

void CrossHair::drawBegin()
{
  setDeviceLineWidth( linewidth_ );
  oldstate_ = glIsEnabled(GL_LINE_SMOOTH);
  if (smooth_)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);
	glBegin( GL_LINES );
}

void CrossHair::drawEnd()
{
  glEnd();

  if (oldstate_)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);
}

void CrossHair::draw(Qwt3D::Triple const& pos)
{
	RGBA rgba = (*plot->dataColor())(pos);
	glColor4d(rgba.r,rgba.g,rgba.b,rgba.a);

  double diag = (plot->hull().maxVertex-plot->hull().minVertex).length() * radius_;

  glVertex3d( pos.x - diag, pos.y, pos.z); 
	glVertex3d( pos.x + diag, pos.y, pos.z); 
	
  glVertex3d( pos.x, pos.y - diag, pos.z); 
	glVertex3d( pos.x, pos.y + diag, pos.z); 
  
  glVertex3d( pos.x, pos.y, pos.z - diag); 
	glVertex3d( pos.x, pos.y, pos.z + diag); 

  // hull
  
  if (!boxed_)
    return;

  glVertex3d( pos.x - diag, pos.y - diag, pos.z + diag); 
	glVertex3d( pos.x + diag, pos.y - diag, pos.z + diag); 
  glVertex3d( pos.x - diag, pos.y - diag, pos.z - diag); 
	glVertex3d( pos.x + diag, pos.y - diag, pos.z - diag); 
  
  glVertex3d( pos.x - diag, pos.y + diag, pos.z + diag); 
	glVertex3d( pos.x + diag, pos.y + diag, pos.z + diag); 
  glVertex3d( pos.x - diag, pos.y + diag, pos.z - diag); 
	glVertex3d( pos.x + diag, pos.y + diag, pos.z - diag); 

  glVertex3d( pos.x - diag, pos.y - diag, pos.z + diag); 
	glVertex3d( pos.x - diag, pos.y + diag, pos.z + diag); 
  glVertex3d( pos.x - diag, pos.y - diag, pos.z - diag); 
	glVertex3d( pos.x - diag, pos.y + diag, pos.z - diag); 
  
  glVertex3d( pos.x + diag, pos.y - diag, pos.z + diag); 
	glVertex3d( pos.x + diag, pos.y + diag, pos.z + diag); 
  glVertex3d( pos.x + diag, pos.y - diag, pos.z - diag); 
	glVertex3d( pos.x + diag, pos.y + diag, pos.z - diag); 

  glVertex3d( pos.x - diag, pos.y - diag, pos.z - diag); 
	glVertex3d( pos.x - diag, pos.y - diag, pos.z + diag); 
  glVertex3d( pos.x + diag, pos.y - diag, pos.z - diag); 
	glVertex3d( pos.x + diag, pos.y - diag, pos.z + diag); 
  
  glVertex3d( pos.x - diag, pos.y + diag, pos.z - diag); 
	glVertex3d( pos.x - diag, pos.y + diag, pos.z + diag); 
  glVertex3d( pos.x + diag, pos.y + diag, pos.z - diag); 
	glVertex3d( pos.x + diag, pos.y + diag, pos.z + diag); 
}

/////////////////////////////////////////////////////////////////
//
//   Dot
//
/////////////////////////////////////////////////////////////////

Dot::Dot()
{
  configure(1, false);
}

Dot::Dot(double pointsize, bool smooth)
{
  configure(pointsize, smooth);
}

void Dot::configure(double pointsize, bool smooth)
{
  plot = 0;
  pointsize_ = pointsize;
  smooth_ = smooth;
}

void Dot::drawBegin()
{
  setDevicePointSize( pointsize_ );
  oldstate_ = glIsEnabled(GL_POINT_SMOOTH);
  if (smooth_)
    glEnable(GL_POINT_SMOOTH);
  else
    glDisable(GL_POINT_SMOOTH);

  //glPointSize(10);
	glBegin( GL_POINTS );
}

void Dot::drawEnd()
{
  glEnd();

  if (oldstate_)
    glEnable(GL_POINT_SMOOTH);
  else
    glDisable(GL_POINT_SMOOTH);
}

void Dot::draw(Qwt3D::Triple const& pos)
{
	RGBA rgba = (*plot->dataColor())(pos);
  glColor4d(rgba.r,rgba.g,rgba.b,rgba.a);
  glVertex3d( pos.x, pos.y, pos.z);   
}


/////////////////////////////////////////////////////////////////
//
//   Cone
//
/////////////////////////////////////////////////////////////////

Cone::Cone()
{
	hat      = gluNewQuadric();
	disk     = gluNewQuadric();

  configure(0, 3);
}

Cone::Cone(double rad, unsigned quality)
{
	hat      = gluNewQuadric();
	disk     = gluNewQuadric();

  configure(rad, quality);
}

Cone::~Cone()
{
	gluDeleteQuadric(hat);
	gluDeleteQuadric(disk);
}

void Cone::configure(double rad, unsigned quality)
{
  plot = 0;
  radius_ = rad;
  quality_ = quality;
  oldstate_ = GL_FALSE;

	gluQuadricDrawStyle(hat,GLU_FILL);
	gluQuadricNormals(hat,GLU_SMOOTH);
	gluQuadricOrientation(hat,GLU_OUTSIDE);
	gluQuadricDrawStyle(disk,GLU_FILL);
	gluQuadricNormals(disk,GLU_SMOOTH);
	gluQuadricOrientation(disk,GLU_OUTSIDE);
}

void Cone::draw(Qwt3D::Triple const& pos)
{  
	RGBA rgba = (*plot->dataColor())(pos);
  glColor4d(rgba.r,rgba.g,rgba.b,rgba.a);

  GLint mode;
	glGetIntegerv(GL_MATRIX_MODE, &mode);
	glMatrixMode( GL_MODELVIEW );
  glPushMatrix();

  glTranslatef(pos.x, pos.y, pos.z);

  gluCylinder(hat, 0.0, radius_, radius_*2, quality_, 1);
  glTranslatef(0, 0, radius_*2);
	gluDisk(disk, 0.0, radius_, quality_, 1);

  glPopMatrix();
	glMatrixMode(mode);
}


/////////////////////////////////////////////////////////////////
//
//   Arrow
//
/////////////////////////////////////////////////////////////////

Arrow::Arrow()
{	
	hat      = gluNewQuadric();
	disk     = gluNewQuadric();
	base    = gluNewQuadric();
	bottom   = gluNewQuadric();

	gluQuadricDrawStyle(hat,GLU_FILL);
	gluQuadricNormals(hat,GLU_SMOOTH);
	gluQuadricOrientation(hat,GLU_OUTSIDE);
	gluQuadricDrawStyle(disk,GLU_FILL);
	gluQuadricNormals(disk,GLU_SMOOTH);
	gluQuadricOrientation(disk,GLU_OUTSIDE);
	gluQuadricDrawStyle(base,GLU_FILL);
	gluQuadricNormals(base,GLU_SMOOTH);
	gluQuadricOrientation(base,GLU_OUTSIDE);
	gluQuadricDrawStyle(bottom,GLU_FILL);
	gluQuadricNormals(bottom,GLU_SMOOTH);
	gluQuadricOrientation(bottom,GLU_OUTSIDE);

	configure(3, 0.4, 0.06, 0.02);
}

Arrow::~Arrow()
{
	gluDeleteQuadric(hat);
	gluDeleteQuadric(disk);
	gluDeleteQuadric(base);
	gluDeleteQuadric(bottom);
}

/**
\param segs number of faces for the fields arrows (see the gallery for examples)
\param relconelength see picture
\param relconerad see picture
\param relstemrad see picture
\image html arrowanatomy.png 
*/
void Arrow::configure(int segs, double relconelength, double relconerad, double relstemrad)
{
	plot = 0;
  segments_ = segs;
  oldstate_ = GL_FALSE;
	rel_cone_length = relconelength;
	rel_cone_radius = relconerad;
	rel_stem_radius = relstemrad;	
}

void Arrow::draw(Qwt3D::Triple const& pos)
{	
	Triple end = top_;
	Triple beg = pos;
	Triple vdiff = end-beg;
	double length = vdiff.length();
	glColor4d(rgba_.r,rgba_.g,rgba_.b,rgba_.a);

	double radius[2];
	radius[0] = rel_cone_radius * length;
	radius[1] = rel_stem_radius * length;
	
	GLint mode;
	glGetIntegerv(GL_MATRIX_MODE, &mode);

	glMatrixMode( GL_MODELVIEW );
  glPushMatrix();


	Triple axis;
	double phi = calcRotation(axis, FreeVector(beg,end));
	
	glTranslatef(beg.x, beg.y, beg.z);
  glRotatef(phi, axis.x, axis.y, axis.z);

	double baseheight = (1-rel_cone_length) * length;
	
	glTranslatef(0, 0, baseheight);

	gluCylinder(hat, radius[0], 0.0, rel_cone_length * length, segments_,1);
	gluDisk(disk,radius[1],radius[0], segments_,1);
	
	glTranslatef(0, 0, -baseheight);
	
	gluCylinder(base, radius[1],radius[1], baseheight,segments_,1);
	gluDisk(disk,0,radius[1],segments_,1);

  glPopMatrix();
	glMatrixMode(mode);
}


//! transform a vector on the z axis with length |beg-end|, to get them in coincidence with the vector(beg,end)
/**
	\return Angle in degree to rotate
	\param axis   The axis to rotate around
	\param beg    result vector base point
	\param end    result vector top point
*/ 
double Arrow::calcRotation(Triple& axis, FreeVector const& vec)
{
	
	Triple end = vec.top;
	Triple beg = vec.base;

	Triple firstbeg(0.0,0.0,0.0);
	Triple firstend(0.0,0.0,(end-beg).length());
	
	Triple first = firstend - firstbeg;
	first.normalize();

	Triple second = end-beg;
	second.normalize();

	axis = normalizedcross(first,second);
	double cosphi = dotProduct(first,second);
	
	return 180 * acos(cosphi) / Qwt3D::PI;
}
