
#include "headers.h"
#include "jumper.h"
#include "imath.h"

/**
 * related animations
 */

static engine::AnimSequence openingSequence = 
{
    FRAMETIME(1038),
    FRAMETIME(1075),
    engine::ltNone, 
    0.0f
};
static engine::AnimSequence openingSequence2 = 
{
    FRAMETIME(1075),
    FRAMETIME(1112),
    engine::ltNone, 
    0.0f
};

/**
 * action
 */

Jumper::CanopyOpening::CanopyOpening(Jumper* jumper, NxActor* phFreeFall, NxActor* phFlight, MatrixConversion* mcFlight, PilotchuteSimulator* pc, CanopySimulator* c, NxVec3 fla, NxVec3 fra, NxVec3 rla, NxVec3 rra) :
    JumperAction( jumper )
{
    // set action properties
    _actionTime = 0.0f;
    _blendTime = 0.2f;
    _endOfAction = false;
    _phActor = phFlight;
    _matrixConversion = mcFlight;
    _pilotchute = pc;
    _canopy = c;
    _frontLeftAnchor  = fla;
    _frontRightAnchor = fra;
    _rearLeftAnchor   = rla;
    _rearRightAnchor  = rra;
    _initialLD = phFlight->getLinearDamping();

    // activale jumper body simulator
    Matrix4f sampleLTM = Jumper::getCollisionFC( _clump )->getFrame()->getLTM();
    phFlight->setGlobalPose( wrap( sampleLTM ) );
    phFlight->wakeUp();
    phFlight->setLinearVelocity( phFreeFall->getLinearVelocity() );
    phFlight->setAngularVelocity( phFreeFall->getAngularVelocity() );    

    // connect & open canopy    
    _canopy->connect(
        _phActor, 
        _frontLeftAnchor,
        _frontRightAnchor,
        _rearLeftAnchor,
        _rearRightAnchor,
        Jumper::getFrontLeftRiser( _clump ),
        Jumper::getFrontRightRiser( _clump ),
        Jumper::getRearLeftRiser( _clump ),
        Jumper::getRearRightRiser( _clump )
    );

    // retrieve pilotchute velocity
    float pcVel = _jumper->getPilotchuteSimulator()->getPhActor()->getLinearVelocity().magnitude();
    // retrieve pilotchute reference velocity
    database::Canopy* canopyInfo = database::Canopy::getRecord( _jumper->getVirtues()->equipment.canopy.id );
    assert( canopyInfo->numPilots > _jumper->getVirtues()->equipment.pilotchute );
    database::Pilotchute* pcInfo = canopyInfo->pilots + _jumper->getVirtues()->equipment.pilotchute;
    // probability of lineover
    float lineoverProb  = _jumper->getVirtues()->getLineoverProbability( pcVel / pcInfo->Vrec );
    bool  leftLineover  = ( getCore()->getRandToolkit()->getUniform( 0, 1 ) < lineoverProb );
    bool  rightLineover = !leftLineover && ( getCore()->getRandToolkit()->getUniform( 0, 1 ) < lineoverProb );
    float leftLOW  = leftLineover ? getCore()->getRandToolkit()->getUniform( 0.5, 1.0f ) : 0;
    float rightLOW = rightLineover ? getCore()->getRandToolkit()->getUniform( 0.5, 1.0f ) : 0;

    // probability of linetwists
    float linetwistsProb = _jumper->getVirtues()->getLinetwistsProbability( pcVel );
    float linetwistsDice = getCore()->getRandToolkit()->getUniform( 0.0f, 1.0f );
    bool  linetwists = ( linetwistsDice <= linetwistsProb );
    if( _jumper->isPlayer() ) getCore()->logMessage( "linetwists dice: %2.1f", linetwistsDice );

    // generate linetwists (positive is righttwist, negative is lefttwist)
    float linetwistsAngle = getCore()->getRandToolkit()->getUniform( 720, 1440 );
    float sign = getCore()->getRandToolkit()->getUniform( -1, 1 );
    sign = sign < 0.0f ? -1.0f : ( sign > 0.0f ? 1.0f : 0.0f );
    linetwistsAngle *= sign;

    if( !linetwists ) linetwistsAngle = 0.0f;

    // offheading : canopy turns by specified angle
    float minTurn = 170.0f; // rigging skill = 0.0 
    float maxTurn = 90.0f;  // rigging skill = 1.0 
    float rigging = _jumper->getVirtues()->getRiggingSkill(); assert( rigging >= 0 && rigging <= 1 );
    float turn    = minTurn * ( 1 - rigging ) + maxTurn * rigging;
    float angle = getCore()->getRandToolkit()->getUniform( -turn, turn );
    if( _jumper->isPlayer() ) getCore()->logMessage( "additional turn (offheading): %2.1f", angle );
    if( !_jumper->isPlayer() ) angle = 0;
    Vector3f sampleX( sampleLTM[0][0], sampleLTM[0][1], sampleLTM[0][2] );
    Vector3f sampleY( sampleLTM[1][0], sampleLTM[1][1], sampleLTM[1][2] );
    Vector3f sampleZ( sampleLTM[2][0], sampleLTM[2][1], sampleLTM[2][2] );
    Vector3f sampleP( sampleLTM[3][0], sampleLTM[3][1], sampleLTM[3][2] );
    // orient canopy towards jumper velocity
    sampleZ = wrap( phFlight->getLinearVelocity() );
    sampleZ *= -1;
    sampleZ.normalize();
    sampleY = _jumper->getClump()->getFrame()->getAt() * -1;
    sampleX.cross( sampleY, sampleZ );
    sampleX.normalize();
    sampleY.cross( sampleZ, sampleX );
    sampleY.normalize();
    sampleLTM.set( 
        sampleX[0], sampleX[1], sampleX[2], 0.0f,
        sampleY[0], sampleY[1], sampleY[2], 0.0f,
        sampleZ[0], sampleZ[1], sampleZ[2], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
    // turn canopy by random angle
    sampleLTM = Gameplay::iEngine->rotateMatrix( sampleLTM, sampleZ, angle );
    // move clump behind jumper
    sampleP += sampleZ * 100;
    sampleLTM[3][0] = sampleP[0];
    sampleLTM[3][1] = sampleP[1];
    sampleLTM[3][2] = sampleP[2];

    _canopy->open( wrap( sampleLTM ), _phActor->getLinearVelocity(), leftLOW, rightLOW, linetwistsAngle );

    // reconnect pilotchute to canopy
    _canopy->getClump()->getFrame()->getLTM();
    _pilotchute->connect( 
        _canopy->getNxActor(), 
        CanopySimulator::getPilotCordJoint( _canopy->getClump() ),
        _canopy->getPilotAnchor()
    );

    // put to sleep freefall simulator
    phFreeFall->putToSleep();
    phFreeFall->raiseActorFlag( NX_AF_DISABLE_COLLISION );

    // show risers
    Jumper::getRisers( _clump )->setFlags( engine::afRender );

    // animation controller
    engine::IAnimationController* animCtrl = _clump->getAnimationController();

    // capture blend source
    animCtrl->captureBlendSrc();

    // reset animation mixer
    for( unsigned int i=0; i<engine::maxAnimationTracks; i++ )
    {
        if( animCtrl->getTrackAnimation( i ) ) animCtrl->setTrackActivity( i, false );
    }

    // setup animation
    animCtrl->setTrackAnimation( 0, &openingSequence );
    animCtrl->setTrackActivity( 0, true );
    animCtrl->setTrackSpeed( 0, 0.75f );
    animCtrl->setTrackWeight( 0, 1.0f );
    animCtrl->resetTrackTime( 0 );
    animCtrl->advance( 0.0f );

    // capture blend destination
    animCtrl->captureBlendDst();
    animCtrl->blend( 0.0f );
}

Jumper::CanopyOpening::~CanopyOpening()
{
    _phActor->setLinearDamping( _initialLD );
}

static float getAirResistancePower(float i)
{
    return ( exp( pow( i, 1.4f ) )  - 1.0f ) / 1.718f;
}

void Jumper::CanopyOpening::update(float dt)
{
    if( _canopy->getInflation() > 0 )
    {
        updateAnimation( dt );
    }

    if(
        _clump->getAnimationController()->isEndOfAnimation( 0 )
        && (_clump->getAnimationController()->getTrackAnimation(0) == &openingSequence)
        && (_jumper->getSpinalCord()->left > 0.0f)
        && (_jumper->getSpinalCord()->right > 0.0f)
    ) {
        engine::IAnimationController* animCtrl = _clump->getAnimationController();
        // setup animation
        animCtrl->setTrackAnimation( 0, &openingSequence2 );
        animCtrl->setTrackActivity( 0, true );
        animCtrl->setTrackSpeed( 0, 0.75f );
        animCtrl->setTrackWeight( 0, 1.0f );
        animCtrl->resetTrackTime( 0 );
        animCtrl->advance( 0.0f );

        // capture blend destination
        animCtrl->captureBlendDst();
        animCtrl->blend( 0.0f );
    }

    if( 
        _clump->getAnimationController()->isEndOfAnimation( 0 ) 
        && (_clump->getAnimationController()->getTrackAnimation(0) == &openingSequence2)
    ) {
        _endOfAction = true;
    }

    // synchronize physics & render
    _clump->getFrame()->setMatrix( _matrixConversion->convert( wrap( _phActor->getGlobalPose() ) ) );
    _clump->getFrame()->getLTM();
}

void Jumper::CanopyOpening::updatePhysics(void)
{
    // velocity of base jumper's body
    NxVec3 velocity = _phActor->getLinearVelocity();

    // local coordinate system of base jumper
    NxMat34 pose = _phActor->getGlobalPose();
    NxVec3 x = pose.M.getColumn(0);
    NxVec3 y = pose.M.getColumn(1);
    NxVec3 z = pose.M.getColumn(2);

    // air resistance force
    float AR = _jumper->getVirtues()->getTrackingAirResistance();

    // terminal velocity
    float Vt = sqrt( 9.8f * _phActor->getMass() / AR );
    float It = velocity.magnitude() / Vt;

    // air resistance force
    NxVec3 Far = NxVec3(0,1,0) * getAirResistancePower( velocity.magnitude() / Vt ) * _phActor->getMass() * 9.8f;

    // finalize motion equation    
    _phActor->addForce( Far );

    // linear damping is function of jumper velocity    
    // this is prevents calculation errors due to high speed rates
    float minVel     = 50.0f;
    float minDamping = _initialLD;
    float maxVel     = 70.0f;
    float maxDamping = 2.5f;
    float factor = ( velocity.magnitude() - minVel ) / ( maxVel - minVel );
    factor = factor < 0 ? 0 : ( factor > 1 ? 1 : factor );
    float damping = minDamping * ( factor - 1 ) + maxDamping * ( factor );
    _phActor->setLinearDamping( damping );

    // shallow brake setting
    _canopy->setLeftDeep( 0.7f );
    _canopy->setRightDeep( 0.7f );
}

bool Jumper::CanopyOpening::isCriticalAnimationRange(void)
{
    return ( _actionTime - _blendTime ) < ( FRAMETIME(1062) - FRAMETIME(1038) );
}