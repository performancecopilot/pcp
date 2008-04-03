#include "qwt3d_parametricsurface.h"
#include "qwt3d_surfaceplot.h"

using namespace Qwt3D;

ParametricSurface::ParametricSurface()
:GridMapping()
{
}

ParametricSurface::ParametricSurface(SurfacePlot& pw)
:GridMapping()
{
	plotwidget_p = &pw;
  uperiodic_ = false;
  vperiodic_ = false;
}

ParametricSurface::ParametricSurface(SurfacePlot* pw)
:GridMapping()
{
	plotwidget_p = pw;
  uperiodic_ = false;
  vperiodic_ = false;
}

void ParametricSurface::setPeriodic(bool u, bool v)
{
  uperiodic_ = u;
  vperiodic_ = v;
}

void ParametricSurface::assign(SurfacePlot& plotWidget)
{
	if (&plotWidget != plotwidget_p)
		plotwidget_p = &plotWidget;
}

void ParametricSurface::assign(SurfacePlot* plotWidget)
{
	if (plotWidget != plotwidget_p)
		plotwidget_p = plotWidget;
}

/**
For plotWidget != 0 the function permanently assigns her argument (In fact, assign(plotWidget) is called)
*/
bool ParametricSurface::create()
{
	if ((umesh_p<=2) || (vmesh_p<=2) || !plotwidget_p)
		return false;
	
	/* allocate some space for the mesh */
 	Triple** data         = new Triple* [umesh_p] ;

	unsigned i,j;
	for ( i = 0; i < umesh_p; i++) 
	{
		data[i]         = new Triple [vmesh_p];
	}
	
	/* get the data */

	double du = (maxu_p - minu_p) / (umesh_p - 1);
	double dv = (maxv_p - minv_p) / (vmesh_p - 1);
	
  for (i = 0; i < umesh_p; ++i) 
	{
		for (j = 0; j < vmesh_p; ++j) 
		{
			data[i][j] = operator()(minu_p + i*du, minv_p + j*dv);
			
			if (data[i][j].x > range_p.maxVertex.x)
				data[i][j].x = range_p.maxVertex.x;
			else if (data[i][j].y > range_p.maxVertex.y)
				data[i][j].y = range_p.maxVertex.y;
			else if (data[i][j].z > range_p.maxVertex.z)
				data[i][j].z = range_p.maxVertex.z;
			else if (data[i][j].x < range_p.minVertex.x)
				data[i][j].x = range_p.minVertex.x;
			else if (data[i][j].y < range_p.minVertex.y)
				data[i][j].y = range_p.minVertex.y;
			else if (data[i][j].z < range_p.minVertex.z)
				data[i][j].z = range_p.minVertex.z;
		}
	}

	((SurfacePlot*)plotwidget_p)->loadFromData(data, umesh_p, vmesh_p, uperiodic_, vperiodic_);

	for ( i = 0; i < umesh_p; i++) 
	{
		delete [] data[i];
	}

	delete [] data;

	return true;
}

bool ParametricSurface::create(SurfacePlot& pl)
{
  assign(pl);
  return create();
}
