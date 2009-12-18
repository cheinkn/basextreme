
#include "headers.h"
#include "scene.h"
#include "imath.h"
#include "memstream.h"
#include "database.h"
#include "callback.h"
#include "xpp.h"
#include "../common/istring.h"
#include "mission.h"
#include "interrupt.h"
#include "version.h"
//#include "checkreg.h"
#include "forest.h"

/**
 * class implementation
 */

Scene::Scene(Career* career, Location* location, float holdingTime)
{
    assert( holdingTime > 0 );

    _isLoaded       = false;
    _endOfActivity  = false;
    _career         = career;
    _holdingTime    = holdingTime;
    _passedTime     = 0;
    _location       = location;
    _panoramaAsset  = NULL;
    _stageAsset     = NULL;
    _extrasAsset    = NULL;
    _panorama       = NULL;
    _stage          = NULL;
    _grassTexture   = NULL;
    _grass          = NULL;
    _rainTexture    = NULL;
    _rain           = NULL;

    _scenery = new Actor( this );    
    _camera = NULL;
    _lastCameraPose.set( 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 );
    _timeSpeed = _timeSpeedMultiplier = 1.0f;    
    _windTime = 0.0f;
    _modeQuery = NULL;

    _switchHUDTimeout = 0.0f;
    _isHUDEnabled     = true;

    _collisionGeometry = NULL;
    
    _phScene = NULL;
    _phTerrainVerts     = NULL;
    _phTerrainTriangles = NULL;
    _phTerrainMaterials = NULL;

    // database record for scene location 
    _locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );    
    _reverberation = NULL;
    if( _locationInfo->reverberation )
    {
        _reverberation = new database::LocationInfo::Reverberation;
        *_reverberation = *_locationInfo->reverberation;
    }
    
    // retrieve weather option
    if( _locationInfo->weathers )
    {
        _locationWeather = _locationInfo->weathers;
        while( _locationWeather->weather != _location->getWeather() &&
               _locationWeather->weather != ::wtDatabaseEnding )
        {
            _locationWeather++;
        }
        if( _locationWeather->weather == ::wtDatabaseEnding )
        {
            _locationWeather = NULL;
        }
    }
    else
    {
        _locationWeather = NULL;
    }

    setCamera( NULL );

    // gui stuff
    _loadingWindow = Gameplay::iGui->createWindow( "Loading" );

    // create clip sensor
    _clipRay = new Sensor;
}

Scene::~Scene()
{
    // clipping  helper
    delete _clipRay;

    // release loading window
    _loadingWindow->getPanel()->release();

    // release physics
    if( _phTerrainVerts ) delete[] _phTerrainVerts;
    if( _phTerrainTriangles ) delete[] _phTerrainTriangles;
    if( _phTerrainMaterials ) delete[] _phTerrainMaterials;

    // release modes
    while( _modes.size() )
    {
        _modes.top()->onSuspend();
        delete _modes.top();
        _modes.pop();
        if( _modes.size() ) _modes.top()->onResume();
    }

    // release scenery actors
    delete _scenery;

    if( _phScene ) NxGetPhysicsSDK()->releaseScene( *_phScene );

    for( EnclosureI enclosureI = _enclosures.begin();
                    enclosureI != _enclosures.end();
                    enclosureI++ )
    {
        delete enclosureI->second;
    }

    if( _grass ) 
    {
        _stage->remove( _grass );
        _grass->release();
    }
    if( _grassTexture ) _grassTexture->release();
    if( _rain ) 
    {
        _stage->remove( _rain );
        _rain->release();
    }
    if( _rainTexture ) _rainTexture->release();

    for( AssetI assetI=_localAssets.begin(); assetI!=_localAssets.end(); assetI++ )
    {
        (*assetI)->release();
    }
    if( _extrasAsset ) _extrasAsset->release();
    if( _stageAsset ) _stageAsset->release();
    if( _panoramaAsset ) _panoramaAsset->release();
    for( TextureI textureI = _localTextures.begin();
                  textureI != _localTextures.end(); 
                  textureI++ )
    {
        (*textureI)->release();
    }

    if( _reverberation ) delete _reverberation;

    assert( _camera == NULL );
}

/**
 * progress
 */

void Scene::progressCallback(const wchar_t* description, float progress, void* userData)
{
    Scene* __this = reinterpret_cast<Scene*>( userData );

    // update & render Gui
    gui::IGuiPanel* panel = __this->_loadingWindow->getPanel()->find( "LoadingMessage" );
    assert( panel && panel->getStaticText() );

    panel->getStaticText()->setText( wstrformat( L"%s...%2.1f%%", description, progress*100 ).c_str() );
    Gameplay::iEngine->getDefaultCamera()->beginScene( 
        engine::cmClearColor | engine::cmClearDepth,
    	  Vector4f( 0,0,0,0 )
    );
    Gameplay::iGui->render();
    Gameplay::iEngine->getDefaultCamera()->endScene();
    Gameplay::iEngine->present();

    // update streaming audio
    Gameplay::iAudio->updateStreamSounds();
}

/**
 * decomposition of class behaviour
 */

static engine::ILight* adjustSunLightLCB(engine::ILight* light, void* data)
{
    if( light->getType() != engine::ltAmbient )
    {
        float mute = *((float*)(data));
        light->setDiffuseColor( light->getDiffuseColor() * mute );
        light->setSpecularColor( light->getSpecularColor() * mute );
    }
    return light;
}

static engine::IClump* adjustSunLightCCB(engine::IClump* clump, void* data)
{
    clump->forAllLights( adjustSunLightLCB, data );
    return clump;
}

static engine::ILight* adjustAmbientLightLCB(engine::ILight* light, void* data)
{
    if( light->getType() == engine::ltAmbient )
    {
        float mute = *((float*)(data));
        light->setDiffuseColor( light->getDiffuseColor() * mute );
        light->setSpecularColor( light->getSpecularColor() * mute );
    }
    return light;
}

static engine::IClump* adjustAmbientLightCCB(engine::IClump* clump, void* data)
{
    clump->forAllLights( adjustAmbientLightLCB, data );
    return clump;
}

void Scene::load(void)
{
    #ifdef GAMEPLAY_EDITION_ND
        // cache default desktop texture
        engine::ITexture* desktopTexture = Gameplay::iGui->getDesktop()->getTexture();
        gui::Rect desktopTextureRect = Gameplay::iGui->getDesktop()->getTextureRect();
        // enumerate slides
        std::vector<std::string> slides;    
        WIN32_FIND_DATA findData;
        HANDLE findHandle = FindFirstFile( "./res/slides/*.dds", &findData );
        if( findHandle != INVALID_HANDLE_VALUE )
        {
            slides.push_back( findData.cFileName );
            while( FindNextFile( findHandle, &findData ) ) slides.push_back( findData.cFileName );        
            FindClose( findHandle );
        }
        // select slide texture
        engine::ITexture* slideTexture = NULL;
        if( slides.size() )
        {
            unsigned int slideId = getCore()->getRandToolkit()->getUniformInt() % slides.size();
            std::string resourceName = "./res/slides/" + slides[slideId];
            slideTexture = Gameplay::iEngine->createTexture( resourceName.c_str() );
            assert( slideTexture );        
        }
        // replace desktop texture
        if( slideTexture ) 
        {
            Gameplay::iGui->getDesktop()->setTexture( slideTexture );
            Gameplay::iGui->getDesktop()->setTextureRect( 
                gui::Rect( 0,0, slideTexture->getWidth(), slideTexture->getHeight() )
            );
        }
    #endif

    callback::BSPL   bsps;
    callback::ClumpL clumps;
    callback::ClumpI clumpI;
    callback::AtomicL atomicL;
    callback::AtomicI atomicI;

    // insert loading window in Gui
    Gameplay::iGui->getDesktop()->insertPanel( _loadingWindow->getPanel() );
    _loadingWindow->align( gui::atBottom, 4, gui::atCenter, 0 );

    // database record for scene location 
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );

    // load local textures
    if( locationInfo->localTextures )
    {
        const char** resourceName = locationInfo->localTextures;
        while( *resourceName != NULL )
        {
            engine::ITexture* texture = Gameplay::iEngine->createTexture( *resourceName );
            assert( texture );
            texture->addReference();
            texture->setMagFilter( engine::ftLinear );
            texture->setMinFilter( engine::ftLinear );
            texture->setMipFilter( engine::ftLinear );
            _localTextures.push_back( texture );
            resourceName++;
        }
    }

    // retrieve weather option
    if( _locationWeather )
    {
        // load panorama
        _panoramaNearClip = _locationWeather->panorama.zNear;
        _panoramaFarClip  = _locationWeather->panorama.zFar;
        _panoramaAsset    = Gameplay::iEngine->createAsset( engine::atBinary, _locationWeather->panorama.resource ); assert( _panoramaAsset );
        _panoramaAsset->forAllBSPs( callback::enumerateBSPs, &bsps ); assert( bsps.size() );
        _panoramaAsset->forAllClumps( callback::enumerateClumps, &clumps );
        _panorama = *( bsps.begin() );
        for( clumpI = clumps.begin(); clumpI != clumps.end(); clumpI++ ) 
        {
            _panorama->add( *( clumpI ) );
        }
        bsps.clear();
        clumps.clear();
    }

    // load stage
    _stageNearClip = locationInfo->stage.zNear;
    _stageFarClip = locationInfo->stage.zFar;
    _stageAsset = Gameplay::iEngine->createAsset( engine::atBinary, locationInfo->stage.resource ); assert( _stageAsset );
    _stageAsset->forAllBSPs( callback::enumerateBSPs, &bsps ); assert( bsps.size() );
    _stageAsset->forAllClumps( callback::enumerateClumps, &clumps );
    _stage = *( bsps.begin() );
    for( clumpI = clumps.begin(); clumpI != clumps.end(); clumpI++ ) 
    {
        _stage->add( *( clumpI ) );
    }
    bsps.clear();
    clumps.clear();   

    // retrieve weather options
    database::LocationInfo::Weather* weatherOption = NULL;
    if( locationInfo->weathers )
    {
        database::LocationInfo::Weather* currentOption = locationInfo->weathers;
        while( currentOption->weather != ::wtDatabaseEnding )
        {
            if( currentOption->weather == _location->getWeather() )
            {
                weatherOption = currentOption;
                break;
            }
            currentOption++;
        }
    }

    // modify rendering options
    if( weatherOption )
    {
        assert( weatherOption->fogType != engine::fogLinear );
        _stage->setFogType( weatherOption->fogType );
        _stage->setFogDensity( weatherOption->fogDensity );
        _stage->setFogColor( weatherOption->fogColor );
        _stage->forAllClumps( adjustSunLightCCB, &weatherOption->sunMute );
        _stage->forAllClumps( adjustAmbientLightCCB, &weatherOption->ambientMute );
    }

    // load extras
    _extrasAsset = Gameplay::iEngine->createAsset( engine::atBinary, locationInfo->extras.resource ); assert( _extrasAsset );

    // load local assets
    database::LocationInfo::AssetInfo* assetInfo = locationInfo->localAssets;
    while( assetInfo->name != NULL )
    {
        // load asset
        engine::IAsset* asset = Gameplay::iEngine->createAsset( engine::atXFile, assetInfo->resource );
        assert( asset );
        // rename clumps
        asset->forAllClumps( callback::enumerateClumps, &clumps );        
        for( clumpI = clumps.begin(); clumpI != clumps.end(); clumpI++ ) 
        {
            (*( clumpI ))->setName( assetInfo->name );
        }
        // preprocess asset
        xpp::preprocessXAsset( asset );
        // insert asset in scene storage
        _localAssets.push_back( asset );
        clumps.clear();
        assetInfo++;
    }

    // initialize afterfx
    _brightPass = locationInfo->afterFx.brightPass;
    _bloom = locationInfo->afterFx.bloom;

    // initialize grass
    TiXmlElement* details = Gameplay::iGameplay->getConfigElement( "details" );
    int grass = 0;
    details->Attribute( "grass", &grass );
    if( grass != 0 )
    {
        _grassTexture = NULL;
        if( locationInfo->grass.scheme != NULL )
        {
            // load grass texture
            _grassTexture = Gameplay::iEngine->getTexture( locationInfo->grass.textureName );
            if( !_grassTexture )
            {
                _grassTexture = Gameplay::iEngine->createTexture( locationInfo->grass.textureResource );
                assert( _grassTexture );
            }            
            _grassTexture->addReference();
            _grassTexture->setMagFilter( engine::ftLinear );
            _grassTexture->setMinFilter( engine::ftLinear );
            _grassTexture->setMipFilter( engine::ftLinear );
            _grassTexture->setMipmapLODBias( -2 );

            // locate template atomic
            callback::Locator locator;
            engine::IClump* templateClump = locator.locate( _extrasAsset, locationInfo->grass.templ ); 
            assert( templateClump );
            templateClump->getFrame()->translate( Vector3f(0,0,0) );
            templateClump->getFrame()->getLTM();
            templateClump->forAllAtomics( callback::enumerateAtomics, &atomicL );
            assert( atomicL.size() == 1 );

            // generate (load) grass
            _grass = Gameplay::iEngine->createGrass( 
                locationInfo->grass.cache, 
                *atomicL.begin(), 
                _grassTexture,
                locationInfo->grass.scheme, 
                locationInfo->grass.fadeStart, 
                locationInfo->grass.fadeEnd
            );
            assert( _grass );
            _stage->add( _grass );
        }
    }

    // generate rain settings
    unsigned int numParticles = 0;
    switch( _location->getWeather() )
    {
    case ::wtLightRain:
        numParticles = 1024;
        break;
    case ::wtHardRain:
        numParticles = 2048;
        break;
    case ::wtThunder:
        numParticles = 3072;
        break;
    }

    // initialize rain
    if( numParticles )
    {
        // load rain texture
        _rainTexture = Gameplay::iEngine->createTexture( "./res/particles/rain.dds" );
        assert( _rainTexture );
        _rainTexture->addReference();
        _rainTexture->setMagFilter( engine::ftLinear );
        _rainTexture->setMinFilter( engine::ftLinear );
        _rainTexture->setMipFilter( engine::ftLinear );
        _rainTexture->setMipmapLODBias( -2 );

        // create rain
        _rain = Gameplay::iEngine->createRain( numParticles, 1250.0f, _rainTexture, Vector4f( 1,1,1,1 ) );
        assert( _rain );
        _stage->add( _rain );
    }

    // initialize exit points
    clumps.clear();
    _extrasAsset->forAllClumps( callback::enumerateClumps, &clumps );
    database::LocationInfo::ExitPoint* exitPoint = locationInfo->exitPoints;
    while( exitPoint->nameId != 0 )
    {
        // search for enclosure clump
        bool creationFlag = false;
        for( clumpI = clumps.begin(); clumpI != clumps.end(); clumpI++ )
        {            
            if( strcmp( (*clumpI)->getName(), exitPoint->extras ) == 0 )
            {
                _enclosures.insert( EnclosureT( exitPoint, new Enclosure( *clumpI, exitPoint->delay ) ) );
                creationFlag = true;
                break;
            }
        }
        assert( creationFlag );
        // next exit point
        exitPoint++;
    }

    // initialize physics
    initializePhysics();

    // casting
    if( locationInfo->castingCallback ) locationInfo->castingCallback( getScenery() );

    // remove loading window from Gui
    Gameplay::iGui->getDesktop()->removePanel( _loadingWindow->getPanel() );
    
    #ifdef GAMEPLAY_EDITION_ND
        // remove slide
        if( slideTexture )
        {
            Gameplay::iGui->getDesktop()->setTexture( desktopTexture );
            Gameplay::iGui->getDesktop()->setTextureRect( desktopTextureRect );
            slideTexture->release();
        }
    #endif
}

void Scene::initializePhysics(void)
{
    // database record for scene location 
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );

    // find collision geometry
    engine::IClump* collisionClump = NULL;
    callback::ClumpL clumpL;    
    _extrasAsset->forAllClumps( callback::enumerateClumps, &clumpL );
    for( callback::ClumpI clumpI=clumpL.begin(); clumpI!=clumpL.end(); clumpI++ )
    {
        if( strcmp( (*clumpI)->getName(), "CollisionGeometry" ) == 0 )
        {
            collisionClump = (*clumpI);
            break;
        }        
    }
    assert( collisionClump != NULL );
    collisionClump->getFrame()->translate( Vector3f( 0,0,0 ) );
    collisionClump->getFrame()->getLTM();

    // determine physics scene bounds by collision geometry
    callback::AtomicL atomicL;
    collisionClump->forAllAtomics( callback::enumerateAtomics, &atomicL );
    assert( atomicL.size() == 1 );
    _collisionGeometry = *atomicL.begin();
    Vector3f sceneInf = _collisionGeometry->getAABBInf();
    Vector3f sceneSup = _collisionGeometry->getAABBSup();
    _phSceneBounds.set( sceneInf[0], sceneInf[1], sceneInf[2],
                        sceneSup[0], sceneSup[1], sceneSup[2] );

    // determine limits of scene
    _phSceneLimits.maxNbActors = locationInfo->physicsLimits.numActors;
    _phSceneLimits.maxNbBodies = locationInfo->physicsLimits.numBodies;
    _phSceneLimits.maxNbJoints = locationInfo->physicsLimits.numJoints;
    _phSceneLimits.maxNbDynamicShapes = locationInfo->physicsLimits.numDynamicShapes;
    _phSceneLimits.maxNbStaticShapes  = locationInfo->physicsLimits.numStaticShapes;

    // initialize physics scene
    _phSceneDesc.broadPhase = NX_BROADPHASE_COHERENT;
    _phSceneDesc.gravity.set( 0.0f, -9.8f, 0.0f );
    _phSceneDesc.userNotify = NULL;
    _phSceneDesc.userTriggerReport = this;
    _phSceneDesc.userContactReport = this;
    _phSceneDesc.maxTimestep = simulationStepTime;
    _phSceneDesc.maxIter = 1;
    _phSceneDesc.timeStepMethod = NX_TIMESTEP_VARIABLE;
    _phSceneDesc.maxBounds = &_phSceneBounds;
    _phSceneDesc.limits = &_phSceneLimits;
    _phSceneDesc.groundPlane = false;
    _phSceneDesc.boundsPlanes = false;
    _phSceneDesc.collisionDetection = true;
    _phSceneDesc.userData = this;

    // create scene
    _phScene = NxGetPhysicsSDK()->createScene( _phSceneDesc ); assert( _phScene );    

    // default material
    NxMaterial* defaultMaterial = _phScene->getMaterialFromIndex(0); 
	defaultMaterial->setRestitution( 0.5f );
	defaultMaterial->setStaticFriction( 0.25f );
	defaultMaterial->setDynamicFriction( 0.25f );

    // fixed flesh material
    NxMaterialDesc fixedFleshDesc;
    fixedFleshDesc.staticFriction  = 0.75f;
    fixedFleshDesc.dynamicFriction = 0.75f;
    fixedFleshDesc.restitution     = 0.25f;
    fixedFleshDesc.frictionCombineMode    = NX_CM_MAX;
    fixedFleshDesc.restitutionCombineMode = NX_CM_MULTIPLY;
    _phFleshMaterial = _phScene->createMaterial( fixedFleshDesc );
    assert( _phFleshMaterial );

    // moving flesh material
    NxMaterialDesc movingFleshDesc;
    movingFleshDesc.staticFriction  = 0.25f;
    movingFleshDesc.dynamicFriction = 0.25f;
    movingFleshDesc.restitution     = 0.125f;
    movingFleshDesc.frictionCombineMode    = NX_CM_MAX;
    movingFleshDesc.restitutionCombineMode = NX_CM_MULTIPLY;
    _phMovingFleshMaterial = _phScene->createMaterial( movingFleshDesc );
    assert( _phMovingFleshMaterial );

    // cloth material
    NxMaterialDesc clothDesc;
    clothDesc.staticFriction  = 0.995f;
    clothDesc.dynamicFriction = 0.995f;
    clothDesc.restitution     = 0.05f;
    clothDesc.frictionCombineMode    = NX_CM_MAX;
    clothDesc.restitutionCombineMode = NX_CM_MULTIPLY;
    _phClothMaterial = _phScene->createMaterial( clothDesc );

    // build terrain mesh data
    Matrix4f collisionGeometryLTM = _collisionGeometry->getFrame()->getLTM();
    Vector3f worldVertex;
    engine::Mesh* mesh = _collisionGeometry->getGeometry()->createMesh();
    _phTerrainVerts = new NxVec3[mesh->numVertices];
    _phTerrainTriangles = new NxU32[3*mesh->numTriangles];
    _phTerrainMaterials = new NxMaterialIndex[mesh->numTriangles];
    unsigned int i;
    for( i=0; i<mesh->numVertices; i++ )
    {
        worldVertex = Gameplay::iEngine->transformCoord( mesh->vertices[i], collisionGeometryLTM );
        _phTerrainVerts[i] = wrap( worldVertex );
    }
    for( i=0; i<mesh->numTriangles; i++ )
    {
        _phTerrainTriangles[i*3+0] = mesh->triangles[i].vertexId[0];
        _phTerrainTriangles[i*3+1] = mesh->triangles[i].vertexId[1];
        _phTerrainTriangles[i*3+2] = mesh->triangles[i].vertexId[2];
        _phTerrainMaterials[i] = 0;
    }    

    // initialize terrain descriptor
    _phTerrainDesc.numVertices = mesh->numVertices;
    _phTerrainDesc.numTriangles = mesh->numTriangles;
    _phTerrainDesc.pointStrideBytes = sizeof(NxVec3);
    _phTerrainDesc.triangleStrideBytes = 3*sizeof(NxU32);
    _phTerrainDesc.points = _phTerrainVerts;
    _phTerrainDesc.triangles = _phTerrainTriangles;
    _phTerrainDesc.flags = 0;
    _phTerrainDesc.materialIndexStride = sizeof( NxMaterialIndex );
    _phTerrainDesc.materialIndices = _phTerrainMaterials;
    _phTerrainDesc.heightFieldVerticalAxis = NX_NOT_HEIGHTFIELD;
    _phTerrainDesc.heightFieldVerticalExtent = -1000;
    //_phTerrainDesc.convexEdgeThreshold = 1.0f;

    // cook triangle mesh into shape
    MemoryWriteBuffer writeBuffer;    
    bool cookStatus = NxCookTriangleMesh( _phTerrainDesc, writeBuffer );
    MemoryReadBuffer readBuffer( writeBuffer.data );
    _phTerrainShapeDesc.meshData = NxGetPhysicsSDK()->createTriangleMesh( readBuffer );
    _phTerrainShapeDesc.meshFlags = _phTerrainShapeDesc.meshFlags | NX_MESH_SMOOTH_SPHERE_COLLISIONS;
    //_phTerrainShapeDesc.skinWidth = 0.25f;

    // create terrain actor 
    _phTerrainActorDesc.shapes.push_back( &_phTerrainShapeDesc );
    _phTerrain = _phScene->createActor( _phTerrainActorDesc );

    // retrieve terrain shape
    NxShape** terrainShapes = _phTerrain->getShapes();
    _phTerrainShape = terrainShapes[0]->isTriangleMesh();
    assert( _phTerrainShape );
    
    Gameplay::iEngine->releaseMesh( mesh );
}

/**
 * physics handlers
 */

void Scene::onContactNotify(NxContactPair &pair, NxU32 events)
{
    if( pair.actors[0]->userData )
    {
        reinterpret_cast<Actor*>( pair.actors[0]->userData )->onContact( pair, events );
    }
    if( pair.actors[1]->userData )
    {
        reinterpret_cast<Actor*>( pair.actors[1]->userData )->onContact( pair, events );
    }
}

void Scene::onTrigger(NxShape& triggerShape, NxShape& otherShape, NxTriggerFlag status)
{
}

/**
 * Activity
 */

void Scene::updateActivity(float dt)
{
    if( !_isLoaded ) 
    {
        load();
        _isLoaded = true;
        return;
    }

    // tune scene reverberation
    #ifdef GAMEPLAY_DEVELOPER_EDITION
        if( _reverberation )
        {
            bool _isChanged = false;
            // F4 = decrease inGain
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x3E] & 0x80 )
            {
                _reverberation->inGain -= 0.01f;
                _isChanged = true;
            }
            // F5 = increate inGain
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x3F] & 0x80 )
            {
                _reverberation->inGain += 0.01f;
                _isChanged = true;
            }
            // F6 = decrease reverbMixDB
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x40] & 0x80 )
            {
                _reverberation->reverbMixDB -= 0.001f;
                _isChanged = true;
            }
            // F7 = increate reverbMixDB
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x41] & 0x80 )
            {
                _reverberation->reverbMixDB += 0.001f;
                _isChanged = true;
            }
            // F8 = decrease reverbTime
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x42] & 0x80 )
            {
                _reverberation->reverbTime -= 0.01f;
                _isChanged = true;
            }
            // F9 = increase reverbTime
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x43] & 0x80 )
            {
                _reverberation->reverbTime += 0.01f;
                _isChanged = true;
            }
            // F10 = decrease hfTimeRatio
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x44] & 0x80 )
            {
                _reverberation->hfTimeRatio -= 0.001f;
                _isChanged = true;
            }
            // F11 = increase hfTimeRatio
            if( Gameplay::iGameplay->getKeyboardState()->keyState[0x57] & 0x80 )
            {
                _reverberation->hfTimeRatio += 0.001f;
                _isChanged = true;
            }
            if( _isChanged )
            {
                getScenery()->happen( getScenery(), EVENT_SCENE_REVERBERATION_IS_CHANGED, NULL );
                if( _modes.size() ) getTopMode()->happen( getTopMode(), EVENT_SCENE_REVERBERATION_IS_CHANGED, NULL );
                getCore()->logMessage( 
                    "Reverb = { %3.3f, %3.3f, %3.3f, %3.3f }", 
                    _reverberation->inGain,
                    _reverberation->reverbMixDB,
                    _reverberation->reverbTime,
                    _reverberation->hfTimeRatio
                );
            }
        }
    #endif

    // switch HUD
    _switchHUDTimeout -= dt;
    if( Gameplay::iGameplay->getActionChannel( ::iaSwitchHUDMode )->getTrigger() &&
        _switchHUDTimeout < 0 )
    {
        _isHUDEnabled = !_isHUDEnabled;
        _switchHUDTimeout = 0.25f;
    }

    // update playing time
    getCareer()->getVirtues()->statistics.playingTime += dt;

    // time speed effect
    dt *= _timeSpeed * _timeSpeedMultiplier;

    // wind time
    _windTime += dt * getCore()->getRandToolkit()->getUniform( 0.0f, 0.25f ) *
                      getCore()->getRandToolkit()->getUniform( 0.0f, 0.25f ); 

    // update scenery
    _scenery->updateActivity( dt );

    // remove complete modes
    if( _modes.size() )
    {
        if( _modes.top()->endOfMode() )
        {
            _modes.top()->onSuspend();
            delete _modes.top();
            _modes.pop();
            if( _modes.size() ) _modes.top()->onResume();
        }
    }
    // add mode queries
    if( _modeQuery )
    {
        if( _modes.size() ) _modes.top()->onSuspend();
        _modes.push( _modeQuery );
        _modes.top()->onResume();
        _modeQuery = NULL;
    }

    // update current mode
    if( _modes.size() )
    {
        _modes.top()->updateActivity( dt );
    }

    // update rain
    if( _rain )
    {
        Matrix4f cameraPose = _camera->getPose();
        Vector3f cameraPos( cameraPose[3][0], cameraPose[3][1], cameraPose[3][2] );
        _rain->setProperty( "Center", cameraPos );
        Vector3f vel = Vector3f( 0,-1000,0 ) - wrap( getWindAtPoint( NxVec3( 0,0,0 ) ) );
        _rain->setProperty( "Velocity", vel );
        _rain->setProperty( "NBias", 3.0f );
        _rain->setProperty( "TimeSpeed", _timeSpeed * _timeSpeedMultiplier );
    }

    // update camera
    if( _camera ) 
    {
        _camera->updateActivity( dt );

        Matrix4f cameraPose = _camera->getPose();
        Vector3f cameraOffset = Vector3f( cameraPose[3][0], cameraPose[3][1], cameraPose[3][2] ) - 
                                Vector3f( _lastCameraPose[3][0], _lastCameraPose[3][1], _lastCameraPose[3][2] );
        Vector3f cameraVel = cameraOffset;
        float velMagnitude = cameraVel.length();
        velMagnitude = velMagnitude / ( dt > 0 ? dt : 0.001f );
        if( velMagnitude > 5000.0f ) velMagnitude = 5000.0f;
        cameraVel.normalize();
        cameraVel *= velMagnitude;        
        Gameplay::iAudio->setListener( cameraPose, cameraVel );
        _lastCameraPose = cameraPose;
    }

    // check player health
    if( _career->getVirtues()->evolution.health < 0.75f && !Gameplay::iGameplay->_freeModeIsEnabled )
    {
        #ifdef GAMEPLAY_DEMOVERSION
            // check top mode is not mission and not interrupt mode
            if( dynamic_cast<Mission*>( _modes.top() ) == NULL &&
                dynamic_cast<Interrupt*>( _modes.top() ) == NULL )
            {
                if( _career->getVirtues()->evolution.health > 0 )
                {
                    _career->getVirtues()->evolution.health = 1.0f;
                }
                else
                {
                    _endOfActivity = true;
                }
            }
        #else
            // check top mode is not mission and not interrupt mode
            if( dynamic_cast<Mission*>( _modes.top() ) == NULL &&
                dynamic_cast<Interrupt*>( _modes.top() ) == NULL )
            {
                // force exit to career course
                _endOfActivity = true;
            }
        #endif
    }
}

bool Scene::endOfActivity(void)
{
    return _endOfActivity;
}

void Scene::onBecomeActive(void)
{
    // register progress callback
    Gameplay::iEngine->setProgressCallback( progressCallback, this );
}

void Scene::onBecomeInactive(void)
{
    // unregister progress callback
    Gameplay::iEngine->setProgressCallback( NULL, NULL );
}

/**
 * class complicated behaviour 
 */

void Scene::endOfScene(void)
{
    _endOfActivity = true;
}

float Scene::getHoldingTime(void)
{
    return _holdingTime;
}

float Scene::getPassedTime(void)
{
    return _passedTime;
}

void Scene::passHoldingTime(float time)
{
    assert( _holdingTime >= time );
    _holdingTime -= time;
    _passedTime  += time;
}

void Scene::queryMode(Mode* mode)
{
    assert( _modeQuery == NULL );
    if( _modeQuery ) delete _modeQuery;
    _modeQuery = mode;
}

void Scene::setCamera(Actor* camera)
{
    _camera = camera;
}

engine::IClump* Scene::getExtras(const char* extrasName)
{
    assert( _extrasAsset );

    callback::Locator locator;
    locator.target = NULL;
    locator.targetName = extrasName;
    _extrasAsset->forAllClumps( callback::locateClump, &locator );

    return reinterpret_cast<engine::IClump*>( locator.target );
}

unsigned int Scene::getNumExitPoints(void)
{
    return _enclosures.size();
}

Enclosure* Scene::getExitPointEnclosure(unsigned int exitId)
{
    assert( exitId>=0 && exitId<_enclosures.size() );

    unsigned int i = 0;
    for( EnclosureI enclosureI = _enclosures.begin();
                    enclosureI != _enclosures.end();
                    enclosureI++, i++ )
    {
        if( i == exitId ) return enclosureI->second;
    }

    assert( !"shouldn't be here!" );
    return NULL;
}

const wchar_t* Scene::getExitPointName(unsigned int exitId)
{
    assert( exitId>=0 && exitId<_enclosures.size() );

    unsigned int i = 0;
    for( EnclosureI enclosureI = _enclosures.begin();
                    enclosureI != _enclosures.end();
                    enclosureI++, i++ )
    {
        if( i == exitId ) 
        {
            return Gameplay::iLanguage->getUnicodeString(enclosureI->first->nameId);
        }
    }

    assert( !"shouldn't be here!" );
    return NULL;
}

engine::IClump* Scene::findClump(const char* clumpName)
{
    callback::Locator locator;
    locator.targetName = clumpName;
    locator.target = NULL;

    for( AssetI assetI=_localAssets.begin(); assetI!=_localAssets.end(); assetI++ )
    {        
        (*assetI)->forAllClumps( callback::locateClump, &locator );
        if( locator.target != NULL ) 
        {
            return reinterpret_cast<engine::IClump*>( locator.target );
        }
    }
    return NULL;
}

float Scene::getTimeSpeed(void)
{
    return _timeSpeed * _timeSpeedMultiplier;
}

void Scene::setTimeSpeed(float value)
{
    assert( value > 0 );
    _timeSpeed = value;
}

void Scene::setTimeSpeedMultiplier(float value)
{
    assert( value > 0 );
    _timeSpeedMultiplier = value;
}

audio::ISound* Scene::createWalkSound(void)
{
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );
    assert( locationInfo->footsteps.walkSound );
    
    audio::ISound* sound;
    sound = Gameplay::iAudio->createStaticSound( locationInfo->footsteps.walkSound );
    assert( sound );
    sound->setLoop( true );
    sound->setDistanceModel( 
        locationInfo->footsteps.refdist,
        locationInfo->footsteps.maxdist,
        locationInfo->footsteps.rolloff
    );
    return sound;
}

audio::ISound* Scene::createTurnSound(void)
{
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );
    assert( locationInfo->footsteps.turnSound );
    
    audio::ISound* sound;
    sound = Gameplay::iAudio->createStaticSound( locationInfo->footsteps.turnSound );
    assert( sound );
    sound->setLoop( true );
    sound->setDistanceModel( 
        locationInfo->footsteps.refdist,
        locationInfo->footsteps.maxdist,
        locationInfo->footsteps.rolloff
    );
    return sound;
}

float Scene::getWalkPitch(void)
{
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );
    return locationInfo->footsteps.walkPitch;
}

float Scene::getBackPitch(void)
{
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );
    return locationInfo->footsteps.backPitch;
}

float Scene::getTurnPitch(void)
{
    database::LocationInfo* locationInfo = database::LocationInfo::getRecord( _location->getDatabaseId() );
    return locationInfo->footsteps.turnPitch;
}

static float blast(float t)
{
    return sin(t) + 0.0625f * sin( 10 * t ) + 0.1f * sin( 150 * t );
}

NxVec3 Scene::getWindAtPoint(const NxVec3& point)
{
    if( !database::LocationInfo::getRecord( _location->getDatabaseId() )->wind ) 
    {
        return NxVec3( 0,0,0 );
    }

    float windAmbient   = _location->getWindAmbient();
    float windBlast     = _location->getWindBlast();
    float windAmplitude = ( windBlast - windAmbient ) / 2;

    float windMagnitude = windAmbient + windAmplitude + windAmplitude * blast( _windTime );

    NxVec3 windN = wrap( _location->getWindDirection() );
    windN.normalize();

    return windN * windMagnitude;
}

void Scene::addParticleSystem(engine::IParticleSystem* psys)
{
    _particleSystems.push_back( psys );
    _stage->add( psys );
}

void Scene::removeParticleSystem(engine::IParticleSystem* psys)
{
    _stage->remove( psys );
    for( ParticleSystemI psysI = _particleSystems.begin(); 
                         psysI != _particleSystems.end(); 
                         psysI++ )
    {
        if( (*psysI) == psys )
        {            
            _particleSystems.erase( psysI );
            return;
        }
    }
}

void Scene::addSmokeTrail(engine::IRendering* smokeTrail)
{
    _smokeTrails.push_back( smokeTrail );
    _stage->add( smokeTrail );
}

void Scene::removeSmokeTrail(engine::IRendering* smokeTrail)
{
    _stage->remove( smokeTrail );
    for( SmokeTrailI smokeTrailI = _smokeTrails.begin(); 
                     smokeTrailI != _smokeTrails.end(); 
                     smokeTrailI++ )
    {
        if( (*smokeTrailI) == smokeTrail )
        {
            _smokeTrails.erase( smokeTrailI );
            return;
        }
    }
}

/**
 * RenderSource
 */

Vector4f Scene::getClearColor(void)
{
    // return color from weather preset
    if( _locationWeather )
    {
        return _locationWeather->fogColor;
    }
    else
    {
        return Vector4f( 0,0,1,1 );
    }
}

float Scene::getBlur(void)
{
    return ( 1.0f - _timeSpeed );
}

float Scene::getBrightPass(void)
{
    return database::LocationInfo::getRecord( _location->getDatabaseId() )->afterFx.brightPass;
}

float Scene::getBloom(void)
{
    return database::LocationInfo::getRecord( _location->getDatabaseId() )->afterFx.bloom;
}

unsigned int Scene::getNumPasses(void)
{
    return 2;
}

float Scene::getPassNearClip(unsigned int passId)
{
    assert( passId>=0 && passId<getNumPasses() );
    switch( passId )
    {
    case 0: return _panoramaNearClip;
    case 1: return _stageNearClip;
    }
    return 0.0f;
}

float Scene::getPassFarClip(unsigned int passId)
{
    assert( passId>=0 && passId<getNumPasses() );
    switch( passId )
    {
    case 0: return _panoramaFarClip;        
    case 1: return _stageFarClip;
    }
    return 1000.0f;
}

unsigned int Scene::getPassClearFlag(unsigned int passId)
{
    assert( passId>=0 && passId<getNumPasses() );
    switch( passId )
    {
    case 0: 
        return engine::cmClearColor | engine::cmClearDepth | engine::cmClearStencil;
    case 1:
        return engine::cmClearDepth;
    }
    return 0;
}

void Scene::postRenderCallback(void* data)
{
    Scene* __this = reinterpret_cast<Scene*>( data );

    // DebugRenderer debugRenderer;
    // NxGetPhysicsSDK()->visualize( debugRenderer );
    if( __this->getTopMode() )
    {
        __this->getScenery()->happen( NULL, 0xFABCCBAF, NULL );
    }
}

void Scene::renderPass(unsigned int passId)
{
    assert( passId>=0 && passId<getNumPasses() );
    switch( passId )
    {
    case 0:
        if( _panorama ) _panorama->render();
        break;
    case 1:
        _stage->setPostRenderCallback( postRenderCallback, this );
        _stage->render();
        _stage->setPostRenderCallback( NULL, NULL );
        
        if( _timeSpeedMultiplier > 1 )
        {
            std::wstring text = wstrformat( Gameplay::iLanguage->getUnicodeString(376), int( _timeSpeedMultiplier ) );
            Vector3f screenSize = Gameplay::iEngine->getScreenSize();
            gui::Rect textRect( 0, 0, int( screenSize[0] ) ,32 );
            Gameplay::iGui->renderUnicodeText( textRect, "instruction", Vector4f( 0,0,0,0.75f ), gui::atCenter, gui::atCenter, true, text.c_str() );
            textRect.left -= 1, textRect.right -= 1, textRect.top -= 1, textRect.bottom -= 1;
            Gameplay::iGui->renderUnicodeText( textRect, "instruction", Vector4f( 1,1,0.25,1 ), gui::atCenter, gui::atCenter, true, text.c_str() );
        }
        break;
    }
}

void Scene::renderLensFlares(void)
{
    _stage->renderLensFlares();
}

/**
 * clipping methodz
 */

bool Scene::clipCameraRay(const Vector3f& targetPos, const Vector3f& cameraPos, float& clipDistance)
{
    bool result = false;

    // sense world triangles
    _clipRay->sense( targetPos, ( cameraPos - targetPos ), _stage );

    if( _clipRay->getNumIntersections() )
    {
        // detect nearest collision
        unsigned int nearestId = 0;
        float nearestDistance = _clipRay->getIntersection( 0 )->distance;
        for( unsigned int i=1; i<_clipRay->getNumIntersections(); i++ )
        {
            if( _clipRay->getIntersection( i )->distance < nearestDistance )
            {
                nearestId = i;
                nearestDistance = _clipRay->getIntersection( i )->distance;
            }
        }

        // store collision point
        Vector3f collisionPoint = _clipRay->getIntersection( nearestId )->collisionPoint;

        // sense world triangles with inversed ray
        // if such an intersection will be occured, the ray should pierce through 
        // collision geometry, so camera is not "under" the surface of world
        _clipRay->sense( cameraPos, ( collisionPoint - cameraPos ), _stage );
        if( !_clipRay->getNumIntersections() )
        {
            // calculate clipping distance
            clipDistance = ( collisionPoint - targetPos ).length();
            result = true;
        }
    }

    return result;
}