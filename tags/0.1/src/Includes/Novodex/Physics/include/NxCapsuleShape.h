#ifndef NX_COLLISION_NXCAPSULESHAPE
#define NX_COLLISION_NXCAPSULESHAPE
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxShape.h"

class NxShape;
class NxCapsule;
class NxCapsuleShapeDesc;

/**
A capsule shaped collision detection primitive, also known as a line swept sphere.

'radius' is the radius of the capsule's hemispherical ends and its trunk.
'height' is the distance between the two hemispherical ends of the capsule.
The height is along the capsule's Y axis.

Each shape is owned by an actor that it is attached to.

An instance can be created by calling the createShape() method of the NxActor object
that should own it, with a NxCapsuleShapeDesc object as the parameter, or by adding the 
shape descriptor into the NxActorDesc class before creating the actor.

The shape is deleted by calling NxActor::releaseShape() on the owning actor.
*/

class NxCapsuleShape: public NxShape
	{
	public:
	/**
	Call this to initialize or alter the capsule. 
	*/
	virtual void setDimensions(NxReal radius, NxReal height) = 0;

	/**
	Alters the radius of the capsule.
	*/
	virtual void setRadius(NxReal radius) = 0;

	/**
	Retrieves the radius of the capsule.
	*/
	virtual NxReal getRadius() const = 0;

	/**
	Alters the height of the capsule. 
	*/
	virtual void setHeight(NxReal height) = 0;	

	/**
	Retrieves the height of the capsule.
	*/
	virtual NxReal getHeight() const = 0;

	/**
	Retrieves the capsule data in world space.
	*/
	virtual void getWorldCapsule(NxCapsule& worldCapsule)	const	= 0;

	/*
	Saves the state of the shape object to a descriptor.
	*/
	virtual	void	saveToDesc(NxCapsuleShapeDesc&)		const = 0;
	};
#endif
