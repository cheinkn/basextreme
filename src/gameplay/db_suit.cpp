#include "headers.h"

#include "engine/configreader.h"
#include "shared/ccor.h"
#include "database.h"

using namespace ccor;
using namespace database;
using namespace std;

#define MODELID_SOLIFUGE_ALTITUDE    172
#define MODELID_SOLIFUGE_SUBTERMINAL 173
#define MODELID_FALCO_WINGSUIT       362
#define MODELID_FB_SUIT              747

#define DESCRIPTION_SOLIFUGE_ALTITUDE    339
#define DESCRIPTION_SOLIFUGE_SUBTERMINAL 340
#define DESCRIPTION_FALCO_WINGSUIT       363
#define DESCRIPTION_FB_SUIT              748

#define COST_SOLIFUGE_ALTITUDE    200.0f
#define COST_SOLIFUGE_SUBTERMINAL 300.0f
#define COST_FALCO_WINGSUIT       1500.0f
#define COST_XWING_WINGSUIT       3000.0f
#define COST_FB_SUIT              0.0f


    //float        kAlign;              horizontal velosity aligment coefficient
    //float        mHeadDown;           .        air resistance in headdown pose 
    //float        mFrog;               .        .     air resistance in frog pose 
    //float        mTracking;           .        .     .       air resistance in tracking pose 
    //float        mFrogGlideRatio;     .        .     .              glide ratio in frog pose
    //float        mTrackingGlideRatio; .        .     .              .       glide ratio in tracking pose
    //float        mFrogGlideForce;     .        .     .              .               glide force in frog pose
    //float        mTrackingGlideForce; .        .     .              .                      glide force in tracking pose
    //float        Kage;                .        .     .              .                             coefficient of ageing (damage - to - state reduction)


    float        mWingAreaBox;
    float        mWingAreaTrack;

    float        mWingLiftCoeffBox;
    float        mWingLiftCoeffTrack;
    float        mWingLiftBackTrackEfficiency;

    float        mDragCoeffBoxFront;
    float        mDragCoeffBoxSide;
    float        mDragCoeffBoxTop;

    float        mDragCoeffTrackFront;
    float        mDragCoeffTrackSide;
    float        mDragCoeffTrackTop; 

#define PROPERTIES_SOLIFUGE_ALTITUDE    0.0625f, 1.0f, 1.0f,    1.0f, 1.0f,   1.0f,   1.0f,  1.0f,  0.125f,  0.3f, 1.2f,  0.3f, 0.5f, 0.5f,  0.6f, 0.6f, 0.3f,  0.05f, 0.5f, 1.2f
#define PROPERTIES_SOLIFUGE_SUBTERMINAL 0.125f,  1.0f, 1.0f,    1.5f, 1.125f, 1.125f, 1.25f, 1.25f, 0.125f,  0.3f, 1.2f,  0.3f, 0.5f, 0.5f,  0.6f, 0.6f, 0.3f,  0.05f, 0.5f, 1.2f
#define PROPERTIES_FALCO_WINGSUIT       0.25f,   1.0f, 1.125f,  3.0f, 1.0f,   1.5f,   1.5f,  2.0f,  0.25f,   0.3f, 1.2f,  0.3f, 0.5f, 0.5f,  0.6f, 0.6f, 0.3f,  0.05f, 0.5f, 1.2f
#define PROPERTIES_XWING_WINGSUIT       0.15f,   1.0f, 1.4f,    7.0f, 1.1f,   2.2f,   1.6f,  1.8f,  0.20f,   0.3f, 1.2f,  0.3f, 0.5f, 0.5f,  0.6f, 0.6f, 0.3f,  0.05f, 0.5f, 1.2f
#define PROPERTIES_FB_SUIT              0.125f,  1.0f, 1.0f,    1.5f, 1.125f, 1.125f, 1.25f, 1.25f, 0.125f,  0.3f, 1.2f,  0.3f, 0.5f, 0.5f,  0.6f, 0.6f, 0.3f,  0.05f, 0.5f, 1.2f

static std::vector<Suit> suits;
//{
//    /* 000 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_DARK_BLUE, MFRID_D3, 0, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 001 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_PURPLE, MFRID_D3, 1, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 002 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_KHAKI, MFRID_D3, 2, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 003 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_LILAC, MFRID_D3, 3, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 004 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_YELLOW, MFRID_D3, 4, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 005 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_MAGENTA, MFRID_D3, 5, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 006 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_LIGHT_BLUE, MFRID_D3, 6, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 007 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_LIGHT_GREEN, MFRID_D3, 7, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 008 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_ORANGE, MFRID_D3, 8, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 009 */ { true, COST_SOLIFUGE_ALTITUDE, false, 0, MODELID_SOLIFUGE_ALTITUDE, DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_WHITE, MFRID_D3, 9, PROPERTIES_SOLIFUGE_ALTITUDE },
//    /* 010 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_YELLOW, MFRID_D3, 10, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 011 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_CRIMSON, MFRID_D3, 11, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 012 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_LIGHT_VIOLET, MFRID_D3, 12, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 013 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_VIOLET, MFRID_D3, 13, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 014 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_DARK_BLUE, MFRID_D3, 14, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 015 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_DARK_GREEN, MFRID_D3, 15, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 016 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_WHITE, MFRID_D3, 16, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 017 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_CRIMSON, MFRID_D3, 17, PROPERTIES_FALCO_WINGSUIT },
//    /* 018 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_LIGHT_VIOLET, MFRID_D3, 36, PROPERTIES_FALCO_WINGSUIT },
//    /* 019 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_VIOLET, MFRID_D3, 37, PROPERTIES_FALCO_WINGSUIT },
//    /* 020 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_DARK_BLUE, MFRID_D3, 38, PROPERTIES_FALCO_WINGSUIT },
//    /* 021 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_LIGHT_BLUE, MFRID_D3, 39, PROPERTIES_FALCO_WINGSUIT },
//    /* 022 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_LIGHT_GREEN, MFRID_D3, 40, PROPERTIES_FALCO_WINGSUIT },
//    /* 023 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_DARK_GREEN, MFRID_D3, 41, PROPERTIES_FALCO_WINGSUIT },
//    /* 024 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_YELLOW, MFRID_D3, 42, PROPERTIES_FALCO_WINGSUIT },
//    /* 025 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_ORANGE, MFRID_D3, 43, PROPERTIES_FALCO_WINGSUIT },
//    /* 026 */ { true, COST_FALCO_WINGSUIT, true, 1, MODELID_FALCO_WINGSUIT, DESCRIPTION_FALCO_WINGSUIT, CLID_WHITE, MFRID_D3, 44, PROPERTIES_FALCO_WINGSUIT },
//    /* 027 */ { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, MODELID_SOLIFUGE_SUBTERMINAL, DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_BLACK, MFRID_D3, 64, PROPERTIES_SOLIFUGE_SUBTERMINAL },
//    /* 028 */ { false, 0.0f, false, 0, MODELID_FB_SUIT, DESCRIPTION_FB_SUIT, CLID_WHITE, MFRID_D3, 70, PROPERTIES_FB_SUIT },
//    { 0.0f, 0,0,0,0 }
//};

unsigned int Suit::getNumRecords(void)
{
        return suits.size();
}

Suit* Suit::getRecord(unsigned int id)
{
        assert(id < suits.size());
        return &suits[id];
}


bool getdir (string dir, vector<string>* files)
{
     WIN32_FIND_DATA find_data;
     HANDLE hnd;

     find_data.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
     if((hnd = FindFirstFile(dir.c_str(), &find_data)) == INVALID_HANDLE_VALUE) {
          return true; //no flies or error
     }
     
     do {
             files->push_back(find_data.cFileName);
     } while(FindNextFile(hnd, &find_data));
     
     FindClose(hnd);

     return true;
}


void Suit::initSuits()
{
        getCore()->logMessage("Info: Loading suits.");

        Suit prototypes[] = {
                { true, COST_SOLIFUGE_ALTITUDE, false, 0, L"Solifuge Altitude", DESCRIPTION_SOLIFUGE_ALTITUDE, CLID_DARK_BLUE, MFRID_D3, "", PROPERTIES_SOLIFUGE_ALTITUDE },
                { true, COST_SOLIFUGE_SUBTERMINAL, false, 0, L"Solifuge Subterminal", DESCRIPTION_SOLIFUGE_SUBTERMINAL, CLID_YELLOW, MFRID_D3, "", PROPERTIES_SOLIFUGE_SUBTERMINAL },
                { true, COST_FALCO_WINGSUIT, true, 1, L"Falco", DESCRIPTION_FALCO_WINGSUIT, CLID_CRIMSON, MFRID_D3, "", PROPERTIES_FALCO_WINGSUIT },
                { true, COST_XWING_WINGSUIT, true, 1, L"X Wing", DESCRIPTION_FALCO_WINGSUIT, CLID_CRIMSON, MFRID_D3, "", PROPERTIES_XWING_WINGSUIT }
        };

        loadSuits(prototypes[0], "./res/Gear/Suits/Altitude/", "./res/Gear/Suits/Altitude/*.dds");
        loadSuits(prototypes[1], "./res/Gear/Suits/Subterminal/", "./res/Gear/Suits/Subterminal/*.dds");
        loadSuits(prototypes[2], "./res/Gear/Suits/Falco/", "./res/Gear/Suits/Falco/*.dds");
        loadSuits(prototypes[3], "./res/Gear/Suits/X-Wing/", "./res/Gear/Suits/X-Wing/*.dds");
 
        getCore()->logMessage("Info: Suits loaded.");
}


void Suit::loadSuits(Suit& suitPrototype, string textureBase, const char* dir)
{
        std::vector<string> files;
        
        if (getdir(dir, &files)) {
                int i;
                for (i = 0; i < (int)files.size(); ++i) {
                        suitPrototype.texture = textureBase + files[i];

                        string s(files[i]);
                        s = s.substr(0, s.find_last_of('.'));
                        wstring name(L" ", s.length());
                        copy(s.begin(), s.end(), name.begin());

                        suitPrototype.name = name;
                        suits.push_back(suitPrototype);
                }
        } else {
                getCore()->logMessage("Can't open directory: %s", "");
        }
}