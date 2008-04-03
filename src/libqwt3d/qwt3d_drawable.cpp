#include "qwt3d_drawable.h"

using namespace Qwt3D;

Drawable::~Drawable()
{
  detachAll();
}

void Drawable::saveGLState()
{
	glGetBooleanv(GL_LINE_SMOOTH, &ls);
	glGetBooleanv(GL_POLYGON_SMOOTH, &pols);
	glGetFloatv(GL_LINE_WIDTH, &lw);
	glGetIntegerv(GL_BLEND_SRC, &blsrc);
	glGetIntegerv(GL_BLEND_DST, &bldst);
	glGetDoublev(GL_CURRENT_COLOR, col);
	glGetIntegerv(GL_LINE_STIPPLE_PATTERN, &pattern);
	glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &factor);
	glGetBooleanv(GL_LINE_STIPPLE, &sallowed);
	glGetBooleanv(GL_TEXTURE_2D, &tex2d);
	glGetIntegerv(GL_POLYGON_MODE, polmode);
	glGetIntegerv(GL_MATRIX_MODE, &matrixmode);
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &poloffs[0]);
	glGetFloatv(GL_POLYGON_OFFSET_UNITS, &poloffs[1]);
	glGetBooleanv(GL_POLYGON_OFFSET_FILL, &poloffsfill);
}

void Drawable::restoreGLState()
{
	Enable(GL_LINE_SMOOTH, ls);
	Enable(GL_POLYGON_SMOOTH, pols);
	
	setDeviceLineWidth(lw);
	glBlendFunc(blsrc, bldst);
	glColor4dv(col);

	glLineStipple(factor,pattern);
	Enable(GL_LINE_STIPPLE,sallowed);
	Enable(GL_TEXTURE_2D,tex2d);
	glPolygonMode(polmode[0], polmode[1]);
	glMatrixMode(matrixmode);
	glPolygonOffset(poloffs[0], poloffs[1]);
	setDevicePolygonOffset(poloffs[0], poloffs[1]);

	Enable(GL_POLYGON_OFFSET_FILL, poloffsfill);
}

void Drawable::Enable(GLenum what, GLboolean val)
{
	if (val)
		glEnable(what);
  else
		glDisable(what);
}

void Drawable::attach(Drawable* dr)
{
	if ( dlist.end() == std::find( dlist.begin(), dlist.end(), dr ) )
		if (dr)
		{
			dlist.push_back(dr);
		}
}

void Drawable::detach(Drawable* dr)
{
	std::list<Drawable*>::iterator it = std::find(dlist.begin(), dlist.end(), dr);
	
	if ( it != dlist.end() )
	{
		dlist.erase(it);
	}
}
void Drawable::detachAll()
{
	dlist.clear();
}


//! simplified glut routine (glUnProject): windows coordinates_p --> object coordinates_p 
/**
	Don't rely on (use) this in display lists !
*/
Triple Drawable::ViewPort2World(Triple win, bool* err)
{
  Triple obj;
	
	getMatrices(modelMatrix, projMatrix, viewport);
	int res = gluUnProject(win.x, win.y, win.z, modelMatrix, projMatrix, viewport, &obj.x, &obj.y, &obj.z);

	if (err)
		*err = (res) ? false : true;
	return obj;
}

//! simplified glut routine (glProject): object coordinates_p --> windows coordinates_p 
/**
	Don't rely on (use) this in display lists !
*/
Triple Drawable::World2ViewPort(Triple obj,	bool* err)
{
  Triple win;
	
	getMatrices(modelMatrix, projMatrix, viewport);
	int res = gluProject(obj.x, obj.y, obj.z, modelMatrix, projMatrix, viewport, &win.x, &win.y, &win.z);

	if (err)
		*err = (res) ? false : true;
	return win;
}

/**
	Don't rely on (use) this in display lists !
*/
Triple Drawable::relativePosition(Triple rel)
{
	return ViewPort2World(Triple((rel.x-viewport[0])*viewport[2],(rel.y-viewport[1])*viewport[3],rel.z));
}

void Drawable::draw()
{
	saveGLState();

	for (std::list<Drawable*>::iterator it = dlist.begin(); it!=dlist.end(); ++it)
	{
		(*it)->draw();
	}
	restoreGLState();
}

void Drawable::setColor(double r, double g, double b, double a)
{
	color = RGBA(r,g,b,a);
}	

void Drawable::setColor(RGBA rgba)
{
	color = rgba;
}	
