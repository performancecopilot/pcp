#ifndef qwt3d_reader_h__2004_03_07_14_03_begin_guarded_code
#define qwt3d_reader_h__2004_03_07_14_03_begin_guarded_code

#include "qwt3d_io.h"

namespace Qwt3D
{

/*! 
Functor for reading of native files containing grid data. 
As a standart input functor associated with "mes" and "MES"
file extensions.   
*/
class QWT3D_EXPORT NativeReader : public IO::Functor
{
friend class IO;

public:		
  NativeReader();

private:
  //! Provides new NativeReader object. 
  IO::Functor* clone() const{return new NativeReader(*this);}
  //! Performs actual input
  bool operator()(Plot3D* plot, QString const& fname);
	static const char* magicstring;
  double minz_, maxz_;
	bool collectInfo(FILE*& file, QString const& fname, unsigned& xmesh, unsigned& ymesh, 
									 double& minx, double& maxx, double& miny, double& maxy);
};


} // ns

#endif
