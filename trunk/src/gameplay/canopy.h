
#ifndef CANOPY_INCLUDED
#define CANOPY_INCLUDED

#include "headers.h"
#include "scene.h"
#include "callback.h"
#include "sensor.h"
#include "scene.h"
#include "imath.h"
#include "gear.h"
#include "database.h"

/**
 * canopy rendering: contains textures & callback methods
 */

struct CanopyRenderCallback
{
private:
    static engine::IAtomic* onRenderCanopy(engine::IAtomic* atomic, void* data);
    void restoreAtomic(engine::IAtomic* atomic);
public:
    engine::ITexture* canopyTexture;
public:
    CanopyRenderCallback() : canopyTexture(NULL) {}
    virtual ~CanopyRenderCallback();
public:
    void setTexture(const char* textureName, const char* resourceName);
    void setTexture(database::Canopy* gearRecord);
    void setTexture(Gear* gear);    
    void apply(engine::IClump* clump);
    void restore(engine::IClump* clump);
};

/**
 * canopy simulator
 */

#define WLO_WINDOW_NAME        "WLOWindow"
#define HOOK_KNIFE_WINDOW_NAME "HookKnifeWindow"

class CanopySimulator : public Actor
{
public:
    class CordSimulator
    {
    private:
        float           _cascade;
        engine::IFrame* _riserJoint;
        engine::IFrame* _innerJoint;
        engine::IFrame* _outerJoint;
        engine::IBatch* _cordBatch;
        unsigned int    _firstInstaceId;
    public:
        CordSimulator(
            float cascade, 
            engine::IFrame* riserJoint, 
            engine::IFrame* innerJoint, 
            engine::IFrame* outerJoint, 
            engine::IBatch* cordBatch,
            unsigned int&   inOutFirstInstanceId
        );
        ~CordSimulator();
    public:
        void update(float dt);
    };
public:
    class BrakeSimulator
    {
    private:
        float            _aspect;
        unsigned int     _numCanopyJoints;
        engine::IFrame*  _riserJoint;
        engine::IFrame** _canopyJoints;
        engine::IBatch*  _cordBatch;
        unsigned int     _firstInstaceId;
    public:
        BrakeSimulator(
            float aspect, 
            unsigned int numCanopyJoints, 
            engine::IFrame*  riserJoint, 
            engine::IFrame** canopyJoints,
            engine::IBatch*  cordBatch,
            unsigned int&    inOutFirstInstanceId
        );
        ~BrakeSimulator();
    public:
        void update(float dt);
    };
private:
    class Rope
    {
    private:
        unsigned int _numJoints;      // rope details factor
        float        _mass;           // rope mass
        float        _length;         // rope length
        NxScene*     _nxScene;        // simulation scene
        NxActor**    _nxSegmentBody;  // segment bodies ( _numJoints-1 )
        NxJoint**    _nxSegmentJoint; // segment joints ( _numJoints )
        NxActor*     _nxActor1;       // first connected actor
        NxActor*     _nxActor2;       // second connecred actor
        NxVec3       _anchor1;        // first actor local connection anchor
        NxVec3       _anchor2;        // first actor local connection anchor
    public:
        Rope(unsigned int numJoints, float mass, float length, NxScene* scene);
        ~Rope();
    public:
        void initialize(NxActor* actor1, NxVec3 anchor1, NxActor* actor2, NxVec3 anchor2);
        void setAnchor1(NxVec3 anchor1);
    };
private:
    struct CollapseArea
    {
    public:
        NxVec3 center;
        float  radius;
    public:
        void reset(void);
        void setup(NxVec3 areaCenter, float areaRadius);
        void unite(CollapseArea* anotherArea, database::Canopy* gear);
        void unite(NxVec3 anotherAreaCenter, float anotherAreaRadius, database::Canopy* gear);
        bool canUnite(CollapseArea* anotherArea, database::Canopy* gear);
        bool canUnite(NxVec3 anotherAreaCenter, float anotherAreaRadius, database::Canopy* gear);
    };
private:
    /**
     * abstract PAB 
     */
    class PAB
    {
    protected:
        engine::IFrame*     _frame;      // bone frame
        database::PABDesc*  _pabDesc;    // animation descriptor
        float               _angle;      // animation angle
        PAB*                _constraint; // constraint PAB
    protected:
        // abstraction
        virtual float getTargetAngle(void) = 0;
        virtual void onUpdate(float dt) = 0;
    public:
        // class implementation
        PAB(engine::IClump* clump, database::PABDesc* pabDesc);
        virtual ~PAB() {}
    public:
        // behaviour
        void update(float dt);
        void setupConstraint(unsigned int numPABs, PAB** pabs);
    public:
        // inlines
        inline database::PABDesc* getPABDesc(void) { return _pabDesc; }
        inline float getAngle(void) { return _angle; }
    };
    /**
     * flap PAB
     */
    class Flap : public PAB
    {
    protected:
        float* _deep;
    protected:
        virtual float getTargetAngle(void);
        virtual void onUpdate(float dt);
    public:
        Flap(engine::IClump* clump, database::PABDesc* pabDesc, float* deep) :
          PAB( clump, pabDesc ), _deep(deep)
        {}
    };
    /**
     * section PAB
     */
    class Section : public PAB
    {
    protected:
        CanopySimulator* _canopySimulator;
    protected:
        virtual float getTargetAngle(void);
        virtual void onUpdate(float dt);
    public:
        Section(engine::IClump* clump, database::PABDesc* pabDesc, CanopySimulator* cs) :
          PAB( clump, pabDesc ), _canopySimulator(cs)
        {}
    };
private:
    /**
     * WLO toggles simulator
     */
    class WLOToggles
    {
    private:
        bool             _trigger;
        float            _weight;
        gui::IGuiWindow* _window;
    public:
        // class implementation
        WLOToggles(bool gui);
        virtual ~WLOToggles();
        // class behaviour
        void setTrigger(bool trigger);
        void simulate(float dt, Virtues* virtues);
        void hideGui(void);
    public:
        inline float getWeight(void) { return _weight; }
    };
    /**
     * hook knife simulator
     */
    class HookKnife
    {
    private:
        bool             _trigger;
        float            _weight;
        gui::IGuiWindow* _window;
    public:
        // class implementation
        HookKnife(bool gui);
        virtual ~HookKnife();
        // class behaviour
        void setTrigger(bool trigger);
        void simulate(float dt, Virtues* virtues);
        void hideGui(void);
    public:
        inline float getWeight(void) { return _weight; }
    };
private:
    gui::IGuiWindow*  _signature;              // canopy signature
    gui::IGuiWindow*  _leftLineoverSignature;  // left lineower signature
    gui::IGuiWindow*  _rightLineoverSignature; // right lineower signature
    gui::IGuiWindow*  _linetwistsSignature;    // linetwists signature
    Gear*             _gear;
    database::Canopy* _gearRecord;
    bool              _sliderUp;
    float             _slidingTime;
    float             _sliderHD;
    float             _sliderPosFL;
    float             _sliderPosFR;
    float             _sliderPosRL;
    float             _sliderPosRR;
    Matrix4f          _iFL, _iFR;
    Matrix4f          _iRL, _iRR;
    engine::IClump*   _sliderClump;
    engine::IClump*   _sliderCordFL;
    engine::IClump*   _sliderCordFR;
    engine::IClump*   _sliderCordRL;
    engine::IClump*   _sliderCordRR;
    engine::IClump*   _canopyClump;
    engine::IBatch*   _cordBatch;
    NxActor*          _nxConnected;
    MatrixConversion  _mcCanopy;     // canopy matrix conversion
    NxActor*          _nxCanopy;     // canopy physics simulator    
    Rope*             _frontLeftRope;
    Rope*             _frontRightRope;
    Rope*             _rearLeftRope;
    Rope*             _rearRightRope;
    NxJoint*          _roughJoints[4]; // prevents excessive dist. btw. canopy & connected body
    NxVec3            _frontLeftAnchor[2];
    NxVec3            _frontRightAnchor[2];
    NxVec3            _rearLeftAnchor[2];
    NxVec3            _rearRightAnchor[2];
    NxVec3            _pilotAnchor;
    engine::IFrame*   _frontLeftRiser;
    engine::IFrame*   _frontRightRiser;
    engine::IFrame*   _rearLeftRiser;
    engine::IFrame*   _rearRightRiser;    
    unsigned int      _numCords;
    CordSimulator**   _cords;
    BrakeSimulator*   _leftBrake;
    BrakeSimulator*   _rightBrake;
    bool              _cutAway;
    float             _opening;
    float             _inflation;
    float             _leftDeep;      // deep of left brake
    float             _rightDeep;     // deep of right brake
    float             _leftWarpDeep;  // left warp deep
    float             _rightWarpDeep; // right warp deep
    float             _backLeftRiserDeep;      // deep of back left riser
    float             _backRightRiserDeep;     // deep of back right riser
    float             _frontLeftRiserDeep;      // deep of front left riser
    float             _frontRightRiserDeep;     // deep of front right riser
    bool              _enableWind;
    CollapseArea*     _collapseAreas;
    PAB**             _pabs;
    bool              _flightSoundEnabled;
    float             _mBlinkTime;           // incremental timer for blinking malfunction dots
    bool              _wloIsEffective;       // is WLO action effective to lineover removal?
    bool              _lineoverIsEliminated; // false if lineover still doesn't eliminated
    float             _leftLOW;              // left weight of line over effect 
    float             _rightLOW;             // right weight of line over effect
    engine::IFrame*   _leftLineoverJoint;
    engine::IFrame*   _rightLineoverJoint;
    WLOToggles*       _wloToggles;
    HookKnife*        _hookKnife;
    float             _linetwists;     // angle of linetwists
    unsigned int      _bcStep;         // burden calculation: step counter
    NxVec3            _bcPrevVel;      // burden calculation: previous step velocity
    NxVec3            _bcBurden;       // burden calculation: calculatred overburden
    bool              _cohesionState;  // cohesion state flag
    NxVec3            _cohesionPoint;  // cohesion worldspace point
    NxJoint*          _cohesionJoint;  // cohesion joint
    bool              _collideJumper;  // internal state flag
private:
    CanopyRenderCallback* _renderCallback;
protected:
    // Actor
    virtual void onUpdateActivity(float dt);
    virtual void onUpdatePhysics(void);
    virtual void onContact(NxContactPair &pair, NxU32 events);
    virtual void onEvent(Actor* initiator, unsigned int eventId, void* eventData);
    virtual Matrix4f getPose(void) { return _canopyClump->getFrame()->getLTM(); }
    virtual Vector3f getVel(void) { 
        if( _nxCanopy ) return wrap( _nxCanopy->getLinearVelocity() );
        else return Vector3f( 0,0,0 );
    }
protected:
    // class decomposition
    void updateInflation(void);
    void updateWarp(float dt);
    void updateSlider(float dt);
    void updateProceduralAnimation(float dt);
    void updateCollapse(float dt);
    void updateMalfunctionSignature(gui::IGuiWindow* signature, const wchar_t* description, float weight, const Vector3f& pos);
    void visualizeForce(NxVec3& pos, NxVec3& force);
public:
    // model management
    static engine::IAtomic* getCollisionGeometry(engine::IClump* clump);
    static engine::IFrame* getPhysicsJointFrontLeft(engine::IClump* clump);
    static engine::IFrame* getPhysicsJointFrontRight(engine::IClump* clump);
    static engine::IFrame* getPhysicsJointRearLeft(engine::IClump* clump);
    static engine::IFrame* getPhysicsJointRearRight(engine::IClump* clump);
    static engine::IFrame* getPilotCordJoint(engine::IClump* clump);
    static engine::IFrame* getSliderJointFrontLeft(engine::IClump* clump);
    static engine::IFrame* getSliderJointFrontRight(engine::IClump* clump);
    static engine::IFrame* getSliderJointRearLeft(engine::IClump* clump);
    static engine::IFrame* getSliderJointRearRight(engine::IClump* clump);
public:
    // class implementation
    CanopySimulator(Actor* jumper, Gear* gear, bool sliderUp);
    virtual ~CanopySimulator();
    // class behaviour
    void connect(
        NxActor* actor, // connected weight simulator
        NxVec3 fla, NxVec3 fra, NxVec3 rla, NxVec3 rra, // quartet of local anchors for physics simulation
        engine::IFrame* flr, engine::IFrame* frr, engine::IFrame* rlr, engine::IFrame* rrr  // quartet of joints to place cord connections
    );
    bool isCutAway() { return _cutAway; }
    void cutAway();
    void open(const NxMat34& pose, const NxVec3& velocity, float leftLOW, float rightLOW, float linetwists);
    void reset(void);
    void setLeftDeep(float value);
    void setRightDeep(float value);
    void setLeftWarpDeep(float value);
    void setRightWarpDeep(float value);
    void setBackLeftRiserDeep(float value);
    void setBackRightRiserDeep(float value);
    void setFrontLeftRiserDeep(float value);
    void setFrontRightRiserDeep(float value);
    void setWLOToggles(bool trigger);
    void setHookKnife(bool trigger);
    void enableWind(bool flag);
    void renderCollapseAreas(void);
    void entangle(const NxVec3& cohesionPoint);
    void rip(float force);
public:
    // canopy state
    inline bool isCohesionState(void) { return _cohesionState; }
    inline bool isConnected(void) { return ( _nxConnected != NULL ); }
    inline bool isOpened(void) { return ( _nxCanopy != NULL ); }
    inline NxActor* getNxActor(void) { return _nxCanopy; }
    inline engine::IClump* getClump(void) { return _canopyClump; }
    inline NxVec3 getPilotAnchor(void) { return _pilotAnchor; }
    inline float getInflation(void) { return _inflation; }
    inline NxActor* getActor(void) { return _nxCanopy; }
    inline Gear* getGear(void) { return _gear; }
    inline database::Canopy* getGearRecord(void) { return _gearRecord; }
    inline CollapseArea* getCollapseAreas(void) { return _collapseAreas; }
    inline float getLeftLOW(void) { return _leftLOW; }
    inline float getRightLOW(void) { return _rightLOW; }
    inline float getLinetwists(void) { return _linetwists; }
    inline void setLinetwists(float value) 
    { 
        _linetwists = value; 
    }
    inline void setLineover(float leftLOW, float rightLOW, bool wloIsEffective)
    {
        _wloIsEffective = wloIsEffective;
        _lineoverIsEliminated = false;
        _leftLOW = leftLOW;
        _rightLOW = rightLOW;
    }
    inline float getWingLoading(float weight)
    {
        return database::Canopy::getWingLoading( _gear->id, weight );
    }
};

#endif