#ifndef __plot3d_2003_06_09_12_14__
#define __plot3d_2003_06_09_12_14__

#include "qwt3d_coordsys.h"
#include "qwt3d_enrichment_std.h"

namespace Qwt3D
{
  
//! Base class for all plotting widgets
/*!
  Plot3D handles all the common features for plotting widgets - coordinate system, transformations, mouse/keyboard
  handling, labeling etc.. It contains some pure virtual functions and is, in so far, an abstract base class.
	The class provides interfaces for data handling and implements basic data controlled color allocation.
*/
class QWT3D_EXPORT Plot3D : public QGLWidget
{
    Q_OBJECT

public:
	
#if QT_VERSION < 0x040000
    Plot3D( QWidget* parent = 0, const char* name = 0 );
#else
    Plot3D ( QWidget * parent = 0, const QGLWidget * shareWidget = 0 );
#endif
    virtual ~Plot3D();

    QPixmap renderPixmap (int w=0, int h=0, bool useContext=false);	  
    void updateData(); //!< Recalculate data
		void createCoordinateSystem(Qwt3D::Triple beg, Qwt3D::Triple end);
		Qwt3D::CoordinateSystem* coordinates() { return &coordinates_p; } //!< Returns pointer to CoordinateSystem object
		Qwt3D::ColorLegend* legend() { return &legend_;} //!< Returns pointer to ColorLegend object
		
		double xRotation() const { return xRot_;}  //!< Returns rotation around X axis [-360..360] (some angles are equivalent)
		double yRotation() const { return yRot_;}  //!< Returns rotation around Y axis [-360..360] (some angles are equivalent)
		double zRotation() const { return zRot_;}  //!< Returns rotation around Z axis [-360..360] (some angles are equivalent)

		double xShift() const { return xShift_;} //!< Returns shift along X axis (object coordinates)
		double yShift() const { return yShift_;} //!< Returns shift along Y axis (object coordinates)
		double zShift() const { return zShift_;} //!< Returns shift along Z axis (object coordinates)
		
		double xViewportShift() const { return xVPShift_;} //!< Returns relative shift [-1..1] along X axis (view coordinates)
		double yViewportShift() const { return yVPShift_;} //!< Returns relative shift [-1..1] along Y axis (view coordinates)
		
		double xScale() const { return xScale_;} //!< Returns scaling for X values [0..inf]
		double yScale() const { return yScale_;} //!< Returns scaling for Y values [0..inf]
		double zScale() const { return zScale_;} //!< Returns scaling for Z values [0..inf]

		double zoom() const { return zoom_;} //!< Returns zoom (0..inf)

		bool ortho() const { return ortho_; } //!< Returns orthogonal (true) or perspective (false) projection
		void setPlotStyle( Qwt3D::PLOTSTYLE val);
		Qwt3D::Enrichment* setPlotStyle( Qwt3D::Enrichment const& val);
		Qwt3D::PLOTSTYLE plotStyle() const { return plotstyle_; }//!< Returns plotting style
    //! Returns current Enrichment object used for plotting styles (if set, zero else)
    Qwt3D::Enrichment* userStyle() const { return userplotstyle_p; }
    void		setShading( Qwt3D::SHADINGSTYLE val );
		Qwt3D::SHADINGSTYLE shading() const { return shading_; }//!< Returns shading style
		void setIsolines(int isolines);
		int isolines() const { return isolines_;} //!< Returns number of isolines
	
  	void setSmoothMesh(bool val) {smoothdatamesh_p = val;} //!< Enables/disables smooth data mesh lines. Default is false
    bool smoothDataMesh() const {return smoothdatamesh_p;} //!< True if mesh antialiasing is on
		void setBackgroundColor(Qwt3D::RGBA rgba); //!< Sets widgets background color
		Qwt3D::RGBA backgroundRGBAColor() const {return bgcolor_;} //!< Returns the widgets background color
		void setMeshColor(Qwt3D::RGBA rgba); //!< Sets color for data mesh
		Qwt3D::RGBA meshColor() const {return meshcolor_;} //!< Returns color for data mesh
		void setMeshLineWidth(double lw); //!< Sets line width for data mesh
		double meshLineWidth() const {return meshLineWidth_;} //!< Returns line width for data mesh
		void setDataColor(Color* col); //!< Sets new data color object
    const Color* dataColor() const {return datacolor_p;} //!< Returns data color object

    virtual Qwt3D::Enrichment* addEnrichment(Qwt3D::Enrichment const&); //!< Add an Enrichment
    virtual bool degrade(Qwt3D::Enrichment*); //!< Remove an Enrichment

		Qwt3D::ParallelEpiped hull() const { return hull_;} //!< Returns rectangular hull   

		void showColorLegend(bool);
		
		void setCoordinateStyle(Qwt3D::COORDSTYLE st); //!< Sets style of coordinate system.
		void setPolygonOffset(double d);
		double polygonOffset() const {return polygonOffset_;} //!< Returns relative value for polygon offset [0..1]
		
		void setTitlePosition(double rely, double relx = 0.5, Qwt3D::ANCHOR = Qwt3D::TopCenter);
		void setTitleFont(const QString& family, int pointSize, int weight = QFont::Normal, bool italic = false);
		void setTitleColor(Qwt3D::RGBA col) {title_.setColor(col);} //!< Set caption color
		void setTitle(const QString& title) {title_.setString(title);} //!< Set caption text (one row only)

		
		void assignMouse(MouseState xrot, MouseState yrot, MouseState zrot,
										 MouseState xscale, MouseState yscale, MouseState zscale,
										 MouseState zoom, MouseState xshift, MouseState yshift);
		
		bool mouseEnabled() const; //!< Returns true, if the widget accept mouse input from the user
		void assignKeyboard(
       KeyboardState xrot_n, KeyboardState xrot_p
      ,KeyboardState yrot_n, KeyboardState yrot_p
      ,KeyboardState zrot_n, KeyboardState zrot_p
			,KeyboardState xscale_n, KeyboardState xscale_p 
      ,KeyboardState yscale_n, KeyboardState yscale_p
      ,KeyboardState zscale_n, KeyboardState zscale_p
			,KeyboardState zoom_n, KeyboardState zoom_p
      ,KeyboardState xshift_n, KeyboardState xshift_p
      ,KeyboardState yshift_n, KeyboardState yshift_p
      );
		
		bool keyboardEnabled() const; //!< Returns true, if the widget accept keyboard input from the user
    //! Sets speed for keyboard driven transformations
    void setKeySpeed(double rot, double scale, double shift); 
    //! Gets speed for keyboard driven transformations
    void keySpeed(double& rot, double& scale, double& shift) const; 
      
    bool lightingEnabled() const; //!< Returns true, if Lighting is enabled, false else
    //! Turn light on
    void illuminate(unsigned light = 0);
    //! Turn light off
    void blowout(unsigned light = 0);

    void setMaterialComponent(GLenum property, double r, double g, double b, double a = 1.0);    
    void setMaterialComponent(GLenum property, double intensity);    
    void setShininess(double exponent);
    void setLightComponent(GLenum property, double r, double g, double b, double a = 1.0, unsigned light=0);    
    void setLightComponent(GLenum property, double intensity, unsigned light=0);    

    //! Returns Light 'idx' rotation around X axis [-360..360] (some angles are equivalent)
    double xLightRotation(unsigned idx = 0) const { return (idx<8) ? lights_[idx].rot.x : 0;}
    //! Returns Light 'idx' rotation around Y axis [-360..360] (some angles are equivalent)
    double yLightRotation(unsigned idx = 0) const { return (idx<8) ? lights_[idx].rot.y : 0;}
    //! Returns Light 'idx' rotation around Z axis [-360..360] (some angles are equivalent)
    double zLightRotation(unsigned idx = 0) const { return (idx<8) ? lights_[idx].rot.z : 0;}

    //! Returns shift of Light 'idx 'along X axis (object coordinates)
    double xLightShift(unsigned idx = 0) const {return (idx<8) ? lights_[idx].shift.x : 0;} 
    //! Returns shift of Light 'idx 'along Y axis (object coordinates)
    double yLightShift(unsigned idx = 0) const {return (idx<8) ? lights_[idx].shift.y : 0;} 
    //! Returns shift of Light 'idx 'along Z axis (object coordinates)
    double zLightShift(unsigned idx = 0) const {return (idx<8) ? lights_[idx].shift.z : 0;}
	  //! Returns true if valid data available, false else
    bool hasData() const { return (actualData_p) ? !actualData_p->empty() : false;}

    
signals:
		
		//! Emitted, if the rotation is changed
    void rotationChanged( double xAngle, double yAngle, double zAngle ); 
		//! Emitted, if the shift is changed
		void shiftChanged( double xShift, double yShift, double zShift );
		//! Emitted, if the viewport shift is changed
		void vieportShiftChanged( double xShift, double yShift );
		//! Emitted, if the scaling is changed
    void scaleChanged( double xScale, double yScale, double zScale );
		//! Emitted, if the zoom is changed
		void zoomChanged(double);
		//! Emitted, if the projection mode is changed
    void projectionChanged(bool);

public slots:

		void	setRotation( double xVal, double yVal, double zVal ); 																														
		void	setShift( double xVal, double yVal, double zVal );    																														
		void	setViewportShift( double xVal, double yVal );         																														
		void	setScale( double xVal, double yVal, double zVal );    																														
		void	setZoom( double );                                    																														
		                                                              																														
		void  setOrtho(bool);                                       																														
    
		void	enableMouse(bool val=true); //!< Enable mouse input   																														
		void	disableMouse(bool val =true); //!< Disable mouse input																														
		void	enableKeyboard(bool val=true); //!< Enable keyboard input   																														
		void	disableKeyboard(bool val =true); //!< Disable keyboard input																														

    void enableLighting(bool val = true); //!< Turn Lighting on or off
    void disableLighting(bool val = true); //!< Turn Lighting on or off

    void	setLightRotation( double xVal, double yVal, double zVal, unsigned int idx = 0 ); 																														
		void	setLightShift( double xVal, double yVal, double zVal, unsigned int idx = 0 );    																														

    virtual bool savePixmap(QString const& fileName, QString const& format); //!<  Saves content to pixmap format
		//!  Saves content to vector format
    virtual bool saveVector(QString const& fileName, QString const& format, VectorWriter::TEXTMODE text, VectorWriter::SORTMODE sortmode);
		virtual bool save(QString const& fileName, QString const& format); //!<  Saves content

protected:
    typedef std::list<Qwt3D::Enrichment*> EnrichmentList;
    typedef EnrichmentList::iterator ELIT;
    
		void	initializeGL();
    void	paintGL();
    void	resizeGL( int w, int h );

		void mousePressEvent( QMouseEvent *e );
		void mouseReleaseEvent( QMouseEvent *e );
		void mouseMoveEvent( QMouseEvent *e );
		void wheelEvent( QWheelEvent *e );		

    void keyPressEvent( QKeyEvent *e );

    Qwt3D::CoordinateSystem coordinates_p;
		Qwt3D::Color* datacolor_p;
    Qwt3D::Enrichment* userplotstyle_p;
    EnrichmentList elist_p;

		virtual void calculateHull() = 0;
		virtual void createData() = 0;
    virtual void createEnrichment(Qwt3D::Enrichment&){}
    virtual void createEnrichments();

		void createCoordinateSystem();
		void setHull(Qwt3D::ParallelEpiped p) {hull_ = p;}

    bool initializedGL() const {return initializedGL_;}

		enum OBJECTS
		{
			DataObject,
			LegendObject,
			NormalObject,
			DisplayListSize // only to have a vector length ...
		};
		std::vector<GLuint> displaylists_p;
    Qwt3D::Data* actualData_p;


private:
    struct Light
    {  
      Light() : unlit(true){}
      bool unlit;  
      Qwt3D::Triple rot;
      Qwt3D::Triple shift;
    };
    std::vector<Light> lights_;

    GLdouble xRot_, yRot_, zRot_, xShift_, yShift_, zShift_, zoom_
             , xScale_, yScale_, zScale_, xVPShift_, yVPShift_;
		
		Qwt3D::RGBA meshcolor_;
		double meshLineWidth_;
		Qwt3D::RGBA bgcolor_;
		Qwt3D::PLOTSTYLE plotstyle_;
		Qwt3D::SHADINGSTYLE shading_;
		Qwt3D::FLOORSTYLE floorstyle_;
		bool ortho_;
		double polygonOffset_;
		int isolines_;
		bool displaylegend_;
    bool smoothdatamesh_p;

		Qwt3D::ParallelEpiped hull_;

    Qwt3D::ColorLegend legend_;
 
		Label title_;
		Qwt3D::Tuple titlerel_;
		Qwt3D::ANCHOR titleanchor_;

		
    // mouse
    
    QPoint lastMouseMovePosition_;
		bool mpressed_;

		MouseState xrot_mstate_, 
				yrot_mstate_, 
				zrot_mstate_, 
				xscale_mstate_, 
				yscale_mstate_, 
				zscale_mstate_,
        zoom_mstate_,
				xshift_mstate_,
				yshift_mstate_;

		bool mouse_input_enabled_;

		void setRotationMouse(MouseState bstate, double accel, QPoint diff);
		void setScaleMouse(MouseState bstate, double accel, QPoint diff);
		void setShiftMouse(MouseState bstate, double accel, QPoint diff);

    // keyboard

		bool kpressed_;

		KeyboardState xrot_kstate_[2], 
				yrot_kstate_[2], 
				zrot_kstate_[2], 
				xscale_kstate_[2], 
				yscale_kstate_[2], 
				zscale_kstate_[2],
        zoom_kstate_[2],
				xshift_kstate_[2],
				yshift_kstate_[2];

		bool kbd_input_enabled_;
    double kbd_rot_speed_, kbd_scale_speed_, kbd_shift_speed_;

		void setRotationKeyboard(KeyboardState kseq, double speed);
		void setScaleKeyboard(KeyboardState kseq, double speed);
		void setShiftKeyboard(KeyboardState kseq, double speed);

    

    bool lighting_enabled_;
    void applyLight(unsigned idx);
    void applyLights();

    bool initializedGL_;
    bool renderpixmaprequest_;
};


} // ns
 

#endif
