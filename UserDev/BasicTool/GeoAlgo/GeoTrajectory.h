/**
 * \file GeoObjects.h
 *
 * \ingroup GeoAlgo
 * 
 * \brief Class def header for a class Trajectory
 *
 * @author kazuhiro
 */

/** \addtogroup GeoAlgo

    @{*/
#ifndef BASICTOOL_GEOTRAJECTORY_H
#define BASICTOOL_GEOTRAJECTORY_H

#include "GeoVector.h"

namespace geoalgo {

  /**
     \class Trajectory
     This class represents a trajectory which is an ordered list of Point.
     It is a friend class w/ geoalgo::Point_t hence it has an access to protected functions that avoids
     dimensionality sanity checks for speed.
   */
  class Trajectory : public std::vector<geoalgo::Vector> {

  public:
    
    /// Ctor to specify # points and dimension of each point
    Trajectory(size_t npoints, size_t ndimension) : std::vector<geoalgo::Point_t>(npoints, Point_t(ndimension))
    {}

    /// Ctor using a vector of mere vector point expression
    Trajectory(const std::vector<std::vector<double> > &obj)
    {
      this->reserve(obj.size());
      for(auto const& p : obj) this->push_back(Point_t(p));
    }

    /// Ctor using a vector of point
    Trajectory(const std::vector<geoalgo::Point_t> &obj)
    {
      this->reserve(obj.size());
      for(auto const& p : obj) this->push_back(p);
    }

    /// Default dtor
    virtual ~Trajectory(){}

    /// Returns the cumulative distance along all trajectory points
    double Length() const {

      if(size()<2) return 0;

      double length = 0;
      for(size_t i=0; i<size()-1; ++i) {

	auto const& pt1 = (*this)[i];
	auto const& pt2 = (*this)[i+1];
	double length_sq = 0;
	for(size_t j=0; j<pt1.size(); ++j)

	  length_sq += (pt1[j]-pt2[j]) * (pt1[j]-pt2[j]);
	
	length += sqrt(length_sq);
      }
      
      return length;
    }

    /// push_back overrie w/ dimensionality check 
    void push_back(const Point_t& obj) {
      compat(obj); std::vector<geoalgo::Point_t>::push_back(obj);
    }

    /// Dimensionality check function w/ Trajectory
    void compat(const Point_t& obj) const {

      if(!size()) return;
      if( (*(this->begin())).size() != obj.size() ) {

	std::ostringstream msg;
	msg << "<<" << __FUNCTION__ << ">>"
	    << " size mismatch: "
	    << (*(this->begin())).size() << " != " << obj.size() << std::endl;
	throw GeoAlgoException(msg.str());
      }
    }

    /// Returns a direction vector at a specified trajectory point
    Vector Dir(size_t i=0) const {

      if(size() < (i+2)) {
	std::ostringstream msg;
	msg << "<<" << __FUNCTION__ << ">>"
	    << " length=" << size() << " is too short to find a direction @ index=" << i << std::endl;
	throw GeoAlgoException(msg.str());
      }
      return _Dir_(i);
    }

    /// Dimensionality check function w/ Point_t
    void compat(const Trajectory &obj) const {

      if(!size() || !(obj.size())) return;

      if( (*(this->begin())).size() != (*obj.begin()).size() ) {

	std::ostringstream msg;
	msg << "<<" << __FUNCTION__ << ">>"
	    << " size mismatch: "
	    << (*(this->begin())).size() << " != " << (*obj.begin()).size() << std::endl;
	throw GeoAlgoException(msg.str());

      }
    }

  public:

    /// Streamer
#ifndef __CINT__
    friend std::ostream& operator << (std::ostream &o, Trajectory const& a)
    { o << "Trajectory with " << a.size() << " points " << std::endl;
      for(auto const& p : a )
	o << " " << p << std::endl;
      return o;
    }
#endif
    
    /// Returns a direction vector at a specified trajectory point w/o size check
    Vector _Dir_(size_t i) const {

      return ((*this)[i+1] - (*this)[i]);
      
    }
  };

  typedef Trajectory Trajectory_t;

}

#endif
/** @} */ // end of doxygen group 
