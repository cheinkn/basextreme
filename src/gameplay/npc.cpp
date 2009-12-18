
#include "headers.h"
#include "npc.h"
#include "crypt.h"
#include "version.h"

/**
 * class implemetation
 */

NPC::NPC(Actor* parent, unsigned int databaseId, Airplane* airplane, Enclosure* enclosure, CatToy* catToy) : Actor( parent )
{
    assert( _enclosure );
    assert( _catToy );

    if( parent->getScene()->getCareer()->getLicensedFlag() )
    {
        assert( databaseId != database::NPCInfo::getLicensedCharacterId() );
    }

    _airplane   = airplane;
    _enclosure  = enclosure;
    _databaseId = databaseId;
    _catToy = catToy;

    // retrieve NPC Info
    database::NPCInfo* npcInfo = database::NPCInfo::getRecord( _databaseId );
    
    // fill virtues
    _virtues.appearance.face   = npcInfo->face;
    _virtues.appearance.height = npcInfo->height;
    _virtues.appearance.weight = npcInfo->weight;
    _virtues.evolution.health  = 1.0f;
    _virtues.setPerceptionSkill( npcInfo->perception );
    _virtues.setEnduranceSkill( npcInfo->endurance );
    _virtues.setTrackingSkill( npcInfo->tracking );
    _virtues.setRiggingSkill( npcInfo->rigging );
    
    // equip helmet
    _virtues.equipment.helmet.id    = catToy->getVirtues()->equipment.helmet.id;
    _virtues.equipment.helmet.age   = 0;
    _virtues.equipment.helmet.state = 1.0f;
    _virtues.equipment.helmet.type  = ::gtHelmet;

    // equip suit
    _virtues.equipment.suit.id    = catToy->getVirtues()->equipment.suit.id;
    _virtues.equipment.suit.age   = 0;
    _virtues.equipment.suit.state = 1.0f;
    _virtues.equipment.suit.type  = ::gtSuit;

    // equip rig
    _virtues.equipment.rig.id    = catToy->getVirtues()->equipment.rig.id;
    _virtues.equipment.rig.age   = 0;
    _virtues.equipment.rig.state = 1.0f;
    _virtues.equipment.rig.type  = ::gtRig;

    // equip canopy
    _virtues.equipment.canopy.id    = catToy->getVirtues()->equipment.canopy.id;
    _virtues.equipment.canopy.age   = 0;
    _virtues.equipment.canopy.state = 1.0f;
    _virtues.equipment.canopy.type  = ::gtCanopy;

    // (TEST ISSUE) disable malfunctions
    // because of NPC doesn't able to solve malfuctions
    _virtues.equipment.malfunctions = false;

    // choose PC for enclosure (temporary! - selects last pilotchute)
    _virtues.equipment.pilotchute = catToy->getVirtues()->equipment.pilotchute;

    // choose slider
    _virtues.equipment.sliderOption = catToy->getVirtues()->equipment.sliderOption;

    // dispatch case of LICENSED_CHAR NPC
    #ifdef GAMEPLAY_EDITION_ATARI
        if( database::NPCInfo::getLicensedCharacterId() == _databaseId )
        {
            forceFBEquipment();
        }
        else
        {
            forceNonFBEquipment();
        }
    #endif
   
    // create base jumper
    _jumper = new Jumper( this, _airplane, _enclosure, &_virtues, &_spinalCord, NULL );

    // dispatch case of LICENSED_CHAR NPC
    #ifdef GAMEPLAY_EDITION_ND
        if( database::NPCInfo::getLicensedCharacterId() == _databaseId )
        {
            _jumper->setLicensedCharacterAppearance();
        }
    #endif
    #ifdef GAMEPLAY_EDITION_ATARI
        if( database::NPCInfo::getLicensedCharacterId() == _databaseId )
        {
            _jumper->setLicensedCharacterAppearance();
            _jumper->setNoHelmet( _jumper->getClump() );
        }
    #endif

    // setup no npc program
    _npcProgram = NULL;
    _ownsCattoy = false;
}

NPC::~NPC(void)
{
    if( _ownsCattoy ) delete _catToy;
    if( _npcProgram ) delete _npcProgram;
}

/**
 * class behaviour
 */

void NPC::setProgram(NPCProgram* program)
{
    if( _npcProgram ) delete _npcProgram;
    _npcProgram = program;
}

const wchar_t* NPC::getNPCName(void)
{
    return Gameplay::iLanguage->getUnicodeString( database::NPCInfo::getRecord( _databaseId )->nameId );
}

/**
 * actor abstracts
 */

void NPC::onUpdateActivity(float dt)
{
    // update cat toy
    _catToy->update( dt );

    if( _npcProgram ) 
    {
        _npcProgram->update( dt );
        if( _npcProgram->isEndOfProgram() )
        {
            delete _npcProgram;
            _npcProgram = NULL;
        }
    }
}

void NPC::onEvent(Actor* initiator, unsigned int eventId, void* eventData)
{
}

/**
 * private behaviour
 */

void NPC::forceFBEquipment(void)
{
    // force licensed suit
    if( database::Suit::getRecord( _virtues.equipment.suit.id )->wingsuit )
    {
        // white "Falco" wingsuit
        _virtues.equipment.suit = Gear( ::gtSuit, 26 );
    }
    else
    {
        // FB jumpsuit
        _virtues.equipment.suit = Gear( ::gtSuit, 28 );
    }

    // force licensed rig
    if( database::Rig::getRecord( _virtues.equipment.rig.id )->skydiving )
    {
        // FB skydiving rig
        _virtues.equipment.rig = Gear( ::gtRig, 23 );
    }
    else
    {
        // FB BASE rig
        _virtues.equipment.rig = Gear( ::gtRig, 22 );
    }

    // force licensed canopy
    if( database::Canopy::getRecord( _virtues.equipment.canopy.id )->skydiving )
    {
        // GForce 250
        _virtues.equipment.canopy = Gear( ::gtCanopy, 121 );
        
    }
    else
    {
        // GForce 265
        _virtues.equipment.canopy = Gear( ::gtCanopy, 106 );
    }
}

void NPC::forceNonFBEquipment(void)
{
    // FB suit is equipped?
    if( _virtues.equipment.suit.id == 28 )
    {
        // force yellow "solifuge altitude"
        _virtues.equipment.suit.id = 10;
        // force yello helmet
        _virtues.equipment.helmet.id = 4;
    }

    // FB skyrig is equipped?
    if( _virtues.equipment.rig.id == 23 )
    {
        // force yellow harpy
        _virtues.equipment.rig.id = 19;
    }

    // FB baserig is equipped?
    if( _virtues.equipment.rig.id == 22 )
    {
        // force yellow vector pin
        _virtues.equipment.rig.id = 10;
    }

    // FB BASE canopy is equipped?
    if( _virtues.equipment.canopy.id == 106 ||
        _virtues.equipment.canopy.id == 105 )
    {
        // force purple psychonaut
        _virtues.equipment.canopy.id = 5;
    }

    // FB skydiving canopy is equipped?
    if( _virtues.equipment.canopy.id == 124 )
    {
        // force white haibane
        _virtues.equipment.canopy.id = 19;
    }
}