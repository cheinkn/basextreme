
#ifndef AIRPLANE_ACTORS_INCLUDED
#define AIRPLANE_ACTORS_INCLUDED

#include "headers.h"
#include "scene.h"
#include "../shared/audio.h"

/**
 * airplane contains flying exit points
 */

struct AirplaneDesc
{
public:
    typedef std::vector<std::string> Strings;
    typedef Strings::iterator StringI;
public:
    struct WayPoint
    {
    public:
        Vector3f pos;
        Vector3f dir;
        float    vel;
    public:
        WayPoint(Vector3f p, Vector3f d, float v) : pos(p), dir(d), vel(v) 
        {
            d.normalize();
        }
    };
    typedef std::vector<WayPoint> WayPoints;
    typedef WayPoints::iterator WayPointI;
public:
    engine::IClump* templateClump;   // create airplane from this template
    std::string     propellerFrame;  // name of propeller frame to synchronize sound
    std::string     propellerSound;  // sound resource
    Strings         exitPointFrames; // list of avaiable exit point frames
    Strings         cameraFrames;    // list of avaiable camera frames    
    float           animationSpeed;  // airplane animation speed
    Vector3f        initOffset;      // airplane initial offset
    Vector3f        initDirection;   // airplane initial direction
    float           initAltitude;    // airplane starts by barraging at this altitude
    float           lastAltitude;    // airplane lowering to this altitude after dropping bodies
    float           loweringSpeed;   // airplane lowering speed
    WayPoints       waypoints;       // airplane waypoint    
public:
    AirplaneDesc()
    {
        templateClump = NULL;
        propellerFrame = "";
        propellerSound = "";
        animationSpeed = 1.0f;
        initOffset.set( 0,0,0 );
        initDirection.set( 0,0,1 );
        initAltitude = 0.0f;
        lastAltitude = 0.0f;
        loweringSpeed = 0.0f;
    }
};

class Airplane : public Actor
{
private:
    typedef std::vector<engine::IFrame*> Frames;
    typedef Frames::iterator FrameI;
private:
    AirplaneDesc    _desc;
    engine::IClump* _clump;
    engine::IFrame* _propellerFrame;
    Vector3f        _prevPoint;
    Vector3f        _velocity;
    audio::ISound*  _propellerSound;
    bool            _roughMode;
    bool            _landingMode;
    Frames          _unoccupiedExitPoints;
    float           _restAltitude;
    unsigned int    _waypointId;
    float           _waypointFactor;
public:
    // actor abstracts
    virtual void onUpdateActivity(float dt);
    virtual void onUpdatePhysics();
    virtual void onEvent(Actor* initiator, unsigned int eventId, void* eventData);
    virtual Matrix4f getPose(void) { return _clump->getFrame()->getLTM(); }
    virtual Vector3f getVel(void) { return _velocity; }
public:
    // class implementation
    Airplane(Actor* parent, AirplaneDesc* desc);
    virtual ~Airplane();
public:
    // inlines
    inline bool isRoughMode(void) { return _roughMode; }
    inline bool isLandingMode(void) { return _landingMode; }
public:
    // class behaviour
    unsigned int getNumUnoccupiedExitPoints(void);
    engine::IFrame* occupyRandomExitPoint(void);
    void setRoughMode(bool mode);
    void setLandingMode(bool mode);
};

#endif