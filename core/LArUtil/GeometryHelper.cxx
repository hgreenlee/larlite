#ifndef GEOMETRYHELPER_CXX
#define GEOMETRYHELPER_CXX

#include "GeometryHelper.h"
#include "DetectorProperties.h"
#include "LArProperties.h"

namespace larutil {

  GeometryHelper* GeometryHelper::_me = 0;


  // Function to load any parameters for convenience :
  void GeometryHelper::Reconfigure()
  {
    // Need the geometry for wire pitch
    auto geom = larutil::Geometry::GetME();
    // Need detector properties and lar properties for time to cm conversion
    auto detp = larutil::DetectorProperties::GetME();
    auto larp = larutil::LArProperties::GetME();
    
    // fNPlanes = geom->Nplanes();
    // vertangle.resize(fNPlanes);
    // for(UInt_t ip=0;ip<fNPlanes;ip++)
    //   vertangle[ip]=geom->WireAngleToVertical(geom->PlaneToView(ip)) - TMath::Pi()/2; // wire angle
            
    
    fWireToCm = geom->WirePitch(0,1,0);
    fTimeToCm = detp->SamplingRate()/1000.0 * larp->DriftVelocity(larp->Efield(),larp->Temperature());
  }
  

  // The next set of functions is the collection of functions to convert 3D Point to 2D point
  // The first function is maintained, and the rest convert their arguments and call it
  PxPoint GeometryHelper::Point_3Dto2D(const TVector3 & _3D_position, unsigned int plane) const {
    
    auto geom = larutil::Geometry::GetME();

    // Make a check on the plane:
    if (plane > geom -> Nplanes()){
      throw larutil::LArUtilException(Form("Can't project 3D point to unknown plane %u",plane));
    }    

    // Verify that the point is in the TPC before trying to project:

    //initialize return value
    PxPoint returnPoint;
    
    // The wire position can be gotten with Geometry::NearestWire()
    // Convert result to centimeters as part of the unit convention
    returnPoint.w = geom->NearestWire(_3D_position, plane) * fWireToCm;

    // The time position is the X coordinate, corrected for trigger offset and the offset of the plane
    auto detp = DetectorProperties::GetME();
    returnPoint.t = _3D_position.X();
    // Add in the trigger offset:
    // (Trigger offset is time that the data is recorded before the actual spill.  
    // So, it moves the "0" farther away from the actual time and is an addition)
    returnPoint.t += detp -> TriggerOffset()*fTimeToCm;
    //Get the origin point of this plane:
    Double_t planeOrigin[3];
    geom -> PlaneOriginVtx(plane,planeOrigin);
    // Correct for the origin of the planes
    // X = 0 is at the very first wire plane, so the values of the coords of the planes are either 0 or negative
    // because the planes are negative, the extra distance beyond 0 needs to make the time coordinate larger
    // Therefore, subtract the offest (which is already in centimeters)
    returnPoint.t -= planeOrigin[0];

    // Set the plane of the PxPoint:
    returnPoint.plane = plane;
    
    return returnPoint;
  }

  PxPoint GeometryHelper::Point_3Dto2D(double * xyz, unsigned int plane) const{
    TVector3 vec(xyz);
    return Point_3Dto2D(vec,plane);
  }
  PxPoint GeometryHelper::Point_3Dto2D(float * xyz, unsigned int plane) const{
    TVector3 vec(xyz);
    return Point_3Dto2D(vec,plane);
  }
  PxPoint GeometryHelper::Point_3Dto2D(const std::vector<double> & xyz, unsigned int plane) const{
    TVector3 vec(&(xyz[0]));
    return Point_3Dto2D(vec,plane);
  }
  PxPoint GeometryHelper::Point_3Dto2D(const std::vector<float> & xyz, unsigned int plane) const{
    TVector3 vec(&(xyz[0]));
    return Point_3Dto2D(vec,plane);
  }
  
  void GeometryHelper::Line_3Dto2D( const TVector3 & startPoint3D, const TVector3 & direction3D, unsigned int plane,
                                    PxPoint & startPoint2D, PxPoint & direction2D) const
  {
    // First step, project the start point into 2D
    startPoint2D = Point_3Dto2D(startPoint3D,plane);
    if (! Point_isInTPC( startPoint3D ) ){
      std::cerr << "ERROR - GeometryHelper::Line_3Dto2D: StartPoint3D must be in the TPC.\n";
      return;
    }
    // Next, get a second point in 3D:
    float alpha = 1.0;
    TVector3 secondPoint3D = startPoint3D + alpha*direction3D;
    while ( ! Point_isInTPC(secondPoint3D) ){
      alpha *= -0.75;
      secondPoint3D = startPoint3D + alpha*direction3D;
    }

    // Project the second point into 2D:
    PxPoint secondPoint2D = Point_3Dto2D(secondPoint3D,plane);

    // Now we have two points in 2D.  Get the direction by subtracting, and normalize
    TVector2 dir(secondPoint2D.w - startPoint2D.w,secondPoint2D.t - startPoint2D.t);
    dir *= 1.0 / dir.Mod();
    direction2D.w = dir.X();
    direction2D.t = dir.Y();
    direction2D.plane = plane;

    return;
  }

  float GeometryHelper::DistanceToLine2D( const PxPoint & pointOnLine, const PxPoint & directionOfLine, 
                                          const PxPoint & targetPoint) const
  {

    // Utilize the 3D function to make it easier to maintain
    TVector3 _pointOnLine(pointOnLine.w, pointOnLine.t, 0.0);
    TVector3 _directionOfLine(directionOfLine.w, directionOfLine.t, 0.0);
    TVector3 _targetPoint(targetPoint.w, targetPoint.t, 0.0);

    return DistanceToLine3D(_pointOnLine,_directionOfLine,_targetPoint);
  }

  float GeometryHelper::DistanceToLine2D( const TVector2 & pointOnLine, const TVector2 & directionOfLine, 
                                          const TVector2 & targetPoint) const
  {
    // Utilize the 3D function to make it easier to maintain
    TVector3 _pointOnLine(pointOnLine.X(), pointOnLine.Y(), 0.0);
    TVector3 _directionOfLine(directionOfLine.X(), directionOfLine.Y(), 0.0);
    TVector3 _targetPoint(targetPoint.X(), targetPoint.Y(), 0.0); 

    return DistanceToLine3D(_pointOnLine,_directionOfLine,_targetPoint);
  }

  float GeometryHelper::DistanceToLine3D( const TVector3 & pointOnLine, const TVector3 & directionOfLine, 
                                          const TVector3 & targetPoint) const
  {
    // This algorithm finds the distance between a point and a line by finding the closest point on the line
    // Using minimization techniques from calculus.

    // Line is defined by the vectors pointOnLine and directionOfLine.
    // So, any point on the line can be parametrized as (pointOnLine + t * directionOfLine)
    // This distance between any point on the line and the target point is thus:
    // L = |(pointOnLine + t*directionOfLine) . targetPoint |
    // 
    // Using this, minimize the distance with respect to t (actually, minimize the distance squared since it's easier):
    // d(L^2)/dt = 2 * ( (pointOnLine + t*directionOfLine) . targetPoint ) * directionOfLine
    // 
    // Set equal to 0 and solve for t:
    // pointOnLine . directionOfLine + t * directionOfLine^2 - targetPoint . directionOfLine = 0;
    // Therefore:
    float t = ( targetPoint.Dot(directionOfLine) - pointOnLine.Dot(directionOfLine) ) / (directionOfLine.Dot(directionOfLine));

    // Now, construct the closest approach point, subtract the target point, and return the mag
    TVector3 closestApproach = pointOnLine + t * directionOfLine;

    closestApproach -= targetPoint;
    return closestApproach.Mag();

  }


  bool GeometryHelper::Point_isInTPC(const TVector3 & pointIn3D) const{
    // Use the geometry class to determine if this point is in the TPC
    auto geom = larutil::Geometry::GetME();
    // Check against the 3 coordinates:
    if (pointIn3D.X() > geom -> DetHalfWidth() || pointIn3D.X() < - geom -> DetHalfWidth() )
    {
      return false;
    }
    if (pointIn3D.Y() > geom -> DetHalfHeight() || pointIn3D.Y() < - geom -> DetHalfHeight() )
    {
      return false;
    }
    if (pointIn3D.Z() > geom -> DetLength() || pointIn3D.Z() < 0.0)
    {
      return false;
    }
    return true;
  }





} // larutil

#endif