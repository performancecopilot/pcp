#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning ( disable : 4786 )
#endif

#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#ifdef _DEBUG
	#include <fstream>
#endif

#include <string>

#include "qwt3d_global.h"

#if defined(Q_WS_WIN)
	#include <windows.h>
#endif

#ifndef WHEEL_DELTA
	#define WHEEL_DELTA 120
#endif

#include "qwt3d_portability.h"
#include "qwt3d_helper.h"
#include "qwt3d_openglhelper.h"

//! Common namespace for all QwtPlot3D classes
namespace Qwt3D
{

const double PI = 3.14159265358979323846264338328;

//! Plotting style
enum PLOTSTYLE
{
	NOPLOT     , //!< No visible data
	WIREFRAME  , //!< Wireframe style
	HIDDENLINE , //!< Hidden Line style
	FILLED     , //!< Color filled polygons w/o edges 
	FILLEDMESH , //!< Color filled polygons w/ separately colored edges
  POINTS     , //!< User defined style (used by Enrichments)
  USER         //!< User defined style (used by Enrichments)
};

//! Shading style
enum SHADINGSTYLE
{
	FLAT,      //!< Flat shading (OpenGL)
	GOURAUD    //!< Gouraud Shading (OpenGL)
};

//! Style of Coordinate system
enum COORDSTYLE
{
	NOCOORD, //!< Coordinate system is not visible 
	BOX,     //!< Boxed
	FRAME		 //!< Frame - 3 visible axes
};

//! Different types of axis scales
enum SCALETYPE
{
	LINEARSCALE,//!< Linear scaling 
	LOG10SCALE,	//!< Logarithmic scaling (base 10)
  USERSCALE   //!< User-defined (for extensions)
};

//! Plotting style for floor data (projections)
enum FLOORSTYLE
{
	NOFLOOR,   //!< Empty floor
	FLOORISO,  //!< Isoline projections visible
	FLOORDATA //!< Projected polygons visible
};

//! Mesh type
enum DATATYPE
{
  GRID,		//!< Rectangular grid
	POLYGON //!< Convex polygon
};	

//! The 12 axes
/**
\image html axes.png 
*/
enum AXIS
{
	X1 = 0,   //!<  1st x-axis
	X2 = 3,   //!<  2nd x-axis
	X3 = 4,   //!<  3th x-axis
	X4 = 5,   //!<  4th x-axis
	Y1 = 1,   //!<  1st y-axis
	Y2 = 8,   //!<  2nd y-axis
	Y3 = 7,   //!<  3th y-axis
	Y4 = 6,   //!<  4th y-axis
	Z1 = 2,   //!<  1st z-axis
	Z2 = 9,   //!<  2nd z-axis
	Z3 = 11,  //!<  3th z-axis
	Z4 = 10   //!<  4th z-axis
};

//! The 6 sides
enum SIDE
{
  NOSIDEGRID = 0,
  LEFT   = 1 << 0,
  RIGHT  = 1 << 1,
  CEIL   = 1 << 2,
  FLOOR  = 1 << 3,
  FRONT  = 1 << 4,
  BACK   = 1 << 5
};

//! Possible anchor points for drawing operations
enum ANCHOR
{
	BottomLeft,
	BottomRight,
	BottomCenter,
	TopLeft,
	TopRight,
	TopCenter,
	CenterLeft,
	CenterRight,
	Center
};


//! Tuple <tt>[x,y]</tt>
struct QWT3D_EXPORT Tuple
{
	Tuple() : x(0), y(0) {} //!< Calls Tuple(0,0)
	Tuple(double X, double Y) : x(X), y(Y) {} //!< Initialize Tuple with x and y
	//! Tuple coordinates
  double x,y; 
};

//! Triple <tt>[x,y,z]</tt>
/**
Consider Triples also as vectors in R^3
*/
struct QWT3D_EXPORT Triple
{
	//! Initialize Triple with x,y and z
	explicit Triple(double xv = 0,double yv = 0,double zv = 0) 
		: x(xv), y(yv), z(zv)
	{
	}
	
#ifndef QWT3D_NOT_FOR_DOXYGEN
#ifdef Q_OS_IRIX
  Triple(const Triple& val)
  {
    if (&val == this)
       return;
    x = val.x;
    y = val.y;
    z = val.z;
  }
  const Triple& operator=(const Triple& val)
  {
    if (&val == this)
      return *this;
    x = val.x;
    y = val.y;
    z = val.z;
    return *this;
  }
#endif 
#endif // QWT3D_NOT_FOR_DOXYGEN

  //! Triple coordinates
	double x,y,z; 

	Triple& operator+=(Triple t)
	{
		x += t.x;
		y += t.y;
		z += t.z;

		return *this;
	}
	
	Triple& operator-=(Triple t)
	{
		x -= t.x;
		y -= t.y;
		z -= t.z;

		return *this;
	}
	Triple& operator*=(double d)
	{
		x *= d;
		y *= d;
		z *= d;

		return *this;
	}
	Triple& operator/=(double d)
	{
		x /= d;
		y /= d;
		z /= d;

		return *this;
	}
	Triple& operator*=(Triple t) // scale
	{
		x *= t.x;
		y *= t.y;
		z *= t.z;

		return *this;
	}

	bool operator!=(Triple t) const
	{
		return !isPracticallyZero(x,t.x) || !isPracticallyZero(y,t.y) || !isPracticallyZero(z,t.z);
	}
	
	bool operator==(Triple t) const
	{
		return !operator!=(t);
	}

	double length() const
	{
		double l2 = x*x + y*y + z*z;
		return (isPracticallyZero(l2)) ? 0 :sqrt(l2);
	}
	
	void normalize()
	{
		double l = length();
		if (l)
			*this /= l;
	}
};

inline const Triple operator+(const Triple& t, const Triple& t2)
{
	return Triple(t) += t2;
}
inline const Triple operator-(const Triple& t, const Triple& t2)
{
	return Triple(t) -= t2;
}
inline const Triple operator*(double d, const Triple& t)
{
	return Triple(t) *= d;
}
inline const Triple operator*(const Triple& t, double d)
{
	return Triple(t) *= d;
}
inline const Triple operator/(double d, const Triple& t)
{
	return Triple(t) /= d;
}
inline const Triple operator/(const Triple& t, double d)
{
	return Triple(t) /= d;
}
inline const Triple operator*(const Triple& t, const Triple& t2)
{
	return Triple(t) *= t2;
}

//! Parallelepiped spanned by 2 Triples
/**
Please use \em normalized Parallelepipeds:\n\n
minVertex.x <= maxVertex.x\n
minVertex.y <= maxVertex.y\n
minVertex.z <= maxVertex.z\n
*/
struct QWT3D_EXPORT ParallelEpiped
{
	//! Construct non-initialized Parallelepiped
	ParallelEpiped()
	{
	}

	//! Construct initialized Parallelepiped
	/**
		minv -> minVertex\n
		maxv -> maxVertex\n
	*/
	ParallelEpiped(Triple minv, Triple maxv)
	: minVertex(minv), maxVertex(maxv)
	{
	}
	
	Triple minVertex;
	Triple maxVertex;
};

//! Free vector
/**
	FreeVectors represent objects like normal vectors and other vector fields inside R^3 
*/
struct QWT3D_EXPORT FreeVector
{
	FreeVector()
	{
	}

	//! Construct initialized vector
	/**
		b -> base\n
		e -> top\n
	*/
	FreeVector(Triple b, Triple t)
	: base(b), top(t)
	{
	}
	
	Triple base;
	Triple top;
};

//! A free vector field in R^3
typedef std::vector<FreeVector> FreeVectorField;

//! A point field in R^3
typedef std::vector<Triple> TripleField;
//! Holds indices in a TripleField interpreted as counterclockwise node numbering for a convex polygon
typedef std::vector<unsigned> Cell;
//! Vector of convex polygons. You need a TripleField as base for the node data
typedef std::vector<Cell> CellField;
//! Returns the sum over the sizes of the single cells
unsigned tesselationSize(Qwt3D::CellField const& t);

//! Red-Green-Blue-Alpha value
struct QWT3D_EXPORT RGBA
{
	RGBA()
		: r(0), g(0), b(0), a(1)
		{}
	RGBA(double rr, double gg, double bb, double aa = 1)
		: r(rr), g(gg), b(bb), a(aa)
		{}
	double r,g,b,a;
};

//! A Color field
typedef std::vector<RGBA> ColorVector;

#ifndef QWT3D_NOT_FOR_DOXYGEN

QWT3D_EXPORT QColor GL2Qt(GLdouble r, GLdouble g, GLdouble b); //!< RGB -> QColor
QWT3D_EXPORT Qwt3D::RGBA Qt2GL(QColor col); //!< QColor -> RGBA

typedef double *Vertex;
typedef std::vector<Vertex> DataRow;
typedef std::vector<DataRow> DataMatrix;


class Data
{
public:
  Qwt3D::DATATYPE datatype;
  Data() {datatype= Qwt3D::POLYGON;}
  virtual ~Data() {}
  virtual void clear() = 0; //!< destroy content
  virtual bool empty() const = 0; //!< no data
  void setHull(Qwt3D::ParallelEpiped const& h) {hull_p = h;}
  Qwt3D::ParallelEpiped const& hull() const {return hull_p;} 

protected:
  Qwt3D::ParallelEpiped hull_p;
};


//! Implements a matrix of z-Values with limit access functions 
class GridData : public Data
{
public:
  GridData();
	GridData(unsigned int columns, unsigned int rows);//!< see setSize()
  ~GridData() { clear();}

	int columns() const;
	int rows() const;

	void clear(); //!< destroy content
	bool empty() const { return vertices.empty();}
	void setSize(unsigned int columns, unsigned int rows); //!< destroys content and set new size, elements are uninitialized
	
	DataMatrix vertices;		//!< mesh vertices
	DataMatrix normals;		//!< mesh normals
  void setPeriodic(bool u, bool v) {uperiodic_ = u; vperiodic_ = v;}
  bool uperiodic() const {return uperiodic_;} 
  bool vperiodic() const {return vperiodic_;} 

private:
  bool uperiodic_, vperiodic_;
};


//! Implements a graph-like cell structure with limit access functions 
class CellData : public Data
{
public:
  CellData() {datatype=Qwt3D::POLYGON;}
  ~CellData() { clear();}

	void clear(); //!< destroy content
	bool empty() const { return cells.empty();}
	
	Triple const& operator()(unsigned cellnumber, unsigned vertexnumber);
	
	CellField cells;   //!< polygon/cell mesh 
	TripleField    nodes;
	TripleField    normals; //!< mesh normals
};

inline Triple normalizedcross(Triple const& u, Triple const& v)
{
	Triple n;

  /* compute the cross product (u x v for right-handed [ccw]) */
  n.x = u.y * v.z - u.z * v.y;
  n.y = u.z * v.x - u.x * v.z;
  n.z = u.x * v.y - u.y * v.x;

  /* normalize */
  double l = n.length();
  if (l)
	{
		n /= l;
	}
	else
	{
		n = Triple(0,0,0);
	}
	
	return n;
}

inline double dotProduct(Triple const& u, Triple const& v)
{
	return u.x*v.x + u.y*v.y + u.z*v.z;
}

void convexhull2d( std::vector<unsigned>& idx, const std::vector<Qwt3D::Tuple>& src );


#endif // QWT3D_NOT_FOR_DOXYGEN 

} // ns

#endif
