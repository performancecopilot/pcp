#include "qwt3d_gridmapping.h"
#include "qwt3d_surfaceplot.h"

using namespace Qwt3D;

GridMapping::GridMapping()
{
	plotwidget_p = 0;
	setMesh(0,0);
	setDomain(0,0,0,0);
  restrictRange(ParallelEpiped(Triple(-DBL_MAX,-DBL_MAX,-DBL_MAX),Triple(DBL_MAX,DBL_MAX,DBL_MAX)));
}

void GridMapping::setMesh(unsigned int columns,unsigned int rows)
{
	umesh_p = columns;
	vmesh_p = rows;
}

void GridMapping::setDomain(double minu, double maxu, double minv, double maxv)
{
	minu_p = minu;
	maxu_p = maxu;
	minv_p = minv;
	maxv_p = maxv;
}

void GridMapping::restrictRange(Qwt3D::ParallelEpiped const& p)
{
	range_p = p;
}

