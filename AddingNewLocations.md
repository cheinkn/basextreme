#How to add new locations into the game.

# Location resources #

Location resources consists of the following files:

  * `.\res\<name>\textures\*.dds` - textures.
  * `.\res\<name>\<name>.ba` - contains render geometry.
  * `.\res\<name>\gamedata.ba` - contains collision geometry and exit points.
  * `.\res\<name>\location.cfg` - location configuration, like position in map, etc.
  * `.\res\<name>\missions.cfg` - mission configuration.

# Setup location collision #

Collision data is exported to `.\res\<name>\gamedata.ba` file. Collision mesh must be named `"CollisionGeometry"`.

# Setup location exit points #

Exit point data is exported to `.\res\<name>\gamedata.ba` file, along with collision data.

  * To setup an exit point, create a mesh and name it `"Enclosure01"` for a first exit, "Enclosure02" for a second and so on.
    * Mesh must consist of a floor, where characters will walk;
    * and walls, that defines bounds where character can walk.
  * Assign new Lambert material to floor and name it `"EnclosureFloor"`. You don't need to assign texture to it.
  * Assign new Lambert material to walls where character can jump and name it `"EnclosureAbyss"`. You don't need to assign texture to it.
  * Add 6 dummy objects (like cubes) and position them in exit point's bounds. Parent them to Enclosure object. They will mark where characters will be spawned. Name them like this: `"M<enclosure id><spawn id>_R"`, for example: `M0101_R`, `M0102_R`, `M0103_R`, and so on.

# Add new location to the game #

  * Copy necessary resource files to **`.\res\<name>\`** folder.
  * Register a new location in **`.\res\locations.cfg`** file.
  * Create and setup **`.\res\<name>\location.cfg`** file.
  * Create and setup **`.\res\<name>\missions.cfg`** file.

Look at .\res\testlevel for an example on how to setup location.cfg and mission.cfg files.

And that's it. If everything was done correctly, a new level should appear in the game. :-)