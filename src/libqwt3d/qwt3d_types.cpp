#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning ( disable : 4786 )
#endif

#include <stdlib.h> // qsort
#include <algorithm>
#include <float.h>
#include "qwt3d_types.h"

using namespace Qwt3D;

#ifndef QWT3D_NOT_FOR_DOXYGEN

namespace {
  // convex hull
  
  typedef double coordinate_type;

  int ccw(coordinate_type **P, int i, int j, int k) {
    coordinate_type	a = P[i][0] - P[j][0],
      b = P[i][1] - P[j][1],
      c = P[k][0] - P[j][0],
      d = P[k][1] - P[j][1];
    return a*d - b*c <= 0;	   /* true if points i, j, k counterclockwise */
  }


#define CMPM(c,A,B) \
  v = (*(coordinate_type**)A)[c] - (*(coordinate_type**)B)[c];\
  if (v>0) return 1;\
  if (v<0) return -1;

  int cmpl(const void *a, const void *b) {
    double v;
    CMPM(0,a,b);
    CMPM(1,b,a);
    return 0;
  }

  int cmph(const void *a, const void *b) {return cmpl(b,a);}


  int make_chain(coordinate_type** V, int n, int (*cmp)(const void*, const void*)) {
    int i, j, s = 1;
    coordinate_type* t;

    qsort(V, n, sizeof(coordinate_type*), cmp);
    for (i=2; i<n; i++) {
      for (j=s; j>=1 && ccw(V, i, j, j-1); j--){}
      s = j+1;
      t = V[s]; V[s] = V[i]; V[i] = t;
    }
    return s;
  }

  int _ch2d(coordinate_type **P, int n)  {
    int u = make_chain(P, n, cmpl);		/* make lower hull */
    if (!n) return 0;
    P[n] = P[0];
    return u+make_chain(P+u, n-u+1, cmph);	/* make upper hull */
  }


} // ns anon


GridData::GridData()
{
  datatype = Qwt3D::GRID;
  setSize(0,0);
  setPeriodic(false,false);
}

GridData::GridData(unsigned int columns, unsigned int rows)
{
  datatype = Qwt3D::GRID;
	setSize(columns,rows);
  setPeriodic(false,false);
}

int GridData::columns() const 
{ 
	return (int)vertices.size();
}

int GridData::rows() const 
{ 
	return (empty()) ? 0 : (int)vertices[0].size();	
}

void GridData::clear()
{
	setHull(ParallelEpiped());
	{
		for (unsigned i=0; i!=vertices.size(); ++i)
		{	
			for (unsigned j=0; j!=vertices[i].size(); ++j)
			{	
				delete [] vertices[i][j];	
			}
			vertices[i].clear();
		}
	}

	vertices.clear();

	{
		for (unsigned i=0; i!=normals.size(); ++i)
		{	
			for (unsigned j=0; j!=normals[i].size(); ++j)
			{	
				delete [] normals[i][j];	
			}
			normals[i].clear();
		}
	}
	
	normals.clear();
}


void GridData::setSize(unsigned int columns, unsigned int rows)
{
	this->clear();
	vertices = std::vector<DataRow>(columns);
	{
		for (unsigned int i=0; i!=vertices.size(); ++i)
		{
			vertices[i] = DataRow(rows);
			for (unsigned int j=0; j!=vertices[i].size(); ++j)
			{
				vertices[i][j] = new GLdouble[3];
			}
		}
	}
	normals = std::vector<DataRow>(columns);
	{
		for (unsigned int i=0; i!=normals.size(); ++i)
		{
			normals[i] = DataRow(rows);
			for (unsigned int j=0; j!=normals[i].size(); ++j)
			{
				normals[i][j] = new GLdouble[3];
			}
		}
	}
}

Triple const& CellData::operator()(unsigned cellnumber, unsigned vertexnumber)
{
	return nodes[cells[cellnumber][vertexnumber]];
}

void CellData::clear()
{
	setHull(ParallelEpiped());
  cells.clear();
	nodes.clear();
	normals.clear();
}

QColor Qwt3D::GL2Qt(GLdouble r, GLdouble g, GLdouble b)
{
	return QColor(round(r * 255), round(g * 255), round(b * 255));	
}

RGBA Qwt3D::Qt2GL(QColor col)
{
	QRgb qrgb = col.rgb();
	RGBA rgba;
	rgba.r = qRed(qrgb) / 255.0;
	rgba.g = qGreen(qrgb) / 255.0;
	rgba.b = qBlue(qrgb) / 255.0;
	rgba.a = qAlpha(qrgb) / 255.0;
	return rgba;	
}


void Qwt3D::convexhull2d( std::vector<unsigned>& idx, const std::vector<Tuple>& src )
{
    idx.clear();
    if (src.empty())
        return;
    if (src.size()==1)
    {
        idx.push_back(0);
        return;
    }
    coordinate_type** points = new coordinate_type*[src.size()+1] ;
    coordinate_type* P = new coordinate_type[src.size()*2];

    int i;
		for (i=0; i<(int)src.size(); ++i)
    {
        points[i] = &P[2*i];
        points[i][0] = src[i].x;
        points[i][1] = src[i].y;
    }

    coordinate_type* start = points[0];
    int m = _ch2d( points, src.size() );
    idx.resize(m);
    
		for (i=0; i<m; ++i)
 		{
			idx[i] = (points[i] - start)/2;
		}
    delete [] points;
		delete [] P;
}

unsigned Qwt3D::tesselationSize(CellField const& t)
{
	unsigned ret = 0;
	
	for (unsigned i=0; i!=t.size(); ++i)
		ret += t[i].size();
	
	return ret;
}

#endif // QWT3D_NOT_FOR_DOXYGEN
