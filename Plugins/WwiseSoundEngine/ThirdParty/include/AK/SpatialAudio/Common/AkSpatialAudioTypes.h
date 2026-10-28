/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the
"Apache License"); you may not use this file except in compliance with the
Apache License. You may obtain a copy of the Apache License at
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Spatial audio data type definitions.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#ifdef __cplusplus
#include <AK/Tools/Common/AkString.h>
#endif

#define AK_MAX_NUM_TEXTURE 4
#define AK_MAX_REFLECT_ORDER 4
#define AK_MAX_REFLECTION_PATH_LENGTH (AK_MAX_REFLECT_ORDER + 4)
#define AK_STOCHASTIC_RESERVE_LENGTH AK_MAX_REFLECTION_PATH_LENGTH
#define AK_MAX_SOUND_PROPAGATION_DEPTH 8
#define AK_MAX_SOUND_PROPAGATION_WIDTH 32 // Can not exceed 32 total sound propagation paths.
#define AK_SA_EPSILON (0.001f)
#define AK_SA_DIFFRACTION_EPSILON (0.01f) // Radians (~0.57 degrees)
#define AK_SA_DIFFRACTION_DOT_EPSILON (0.00005f) // 1.f - cos(AK_SA_DIFFRACTION_EPSILON)
#define AK_SA_PLANE_THICKNESS (0.01f)
#define AK_SA_MIN_ENVIRONMENT_ABSORPTION (0.01f)
#define AK_SA_MIN_ENVIRONMENT_SURFACE_AREA (1.0f)

#define AK_INVALID_VERTEX ((AkVertIdx)(-1))
#define AK_INVALID_TRIANGLE ((AkTriIdx)(-1))
#define AK_INVALID_SURFACE ((AkSurfIdx)(-1))
#define AK_INVALID_SA_ID ((AkUInt64)-1)
#define AK_OUTDOORS_ROOM_ID AK_INVALID_SA_ID

/// Determines the operation used to calculate the transmission loss value for a transmission path.
/// A ray is cast from listener to emitter, and this operation is applied to combine the current surface with the previous.
/// A geometry instance can be marked as solid by using \c AkGeometryInstanceParams::IsSolid.
/// If a geometry instance is solid, transmission loss is applied for each pair of hit surfaces, representing an entrance and exit through the geometry.
/// The transmission loss from a pair of hits through a solid geometry instance will be the max of both hits.
enum AkTransmissionOperation
{
	AkTransmissionOperation_Add,				///< Transmission loss of each hit surface is summed until it reaches 100%.
	AkTransmissionOperation_Multiply,			///< The inverse of transmission loss (1 - TL) is multiplied in succession, and the result inverted. With each hit surface, the effect of additional transmission loss is reduced. The total loss will approach but never reach 100% unless a surface with 100% loss is found.
	AkTransmissionOperation_Max,				///< The highest transmission loss of all hit surfaces is used.
	AkTransmissionOperation_Default = AkTransmissionOperation_Max,
};
typedef AkUInt8 AkTransmissionOperation_t;

/// Determines how a room interacts with the distance calculation of other rooms that it overlaps or is nested within.
enum AkRoomDistanceBehavior
{
	/// This room is subtracted from the shape of parents and overlapping rooms of lower priority.
	/// Distance attenuation is applied to other rooms when the listener is in this room, because this room is a distinct space.
	AkRoomDistanceBehavior_Subtract,

	/// [\ref spatial_audio_experimental "Experimental"] This room is not included in the distance calculation of other rooms.
	/// Distance attenuation does not apply to this room's parents, or overlapping sections of lower priority rooms,
	/// because this room does not form a distinct space and is not subtracted from the shape of other rooms.
	/// Diffraction, transmission loss, and other attenuations are not affected.
	AkRoomDistanceBehavior_Exclude,

	AkRoomDistanceBehavior_Default = AkRoomDistanceBehavior_Subtract
};
typedef AkUInt8 AkRoomDistanceBehavior_t;

#ifdef __cplusplus

/// Base type for ID's used by Wwise spatial audio.  
struct AkSpatialAudioID
{
	/// Default constructor.  Creates an invalid ID.
	constexpr AkSpatialAudioID() : id(AK_INVALID_SA_ID) {}
	
	/// Construct from a 64-bit int.
	AkSpatialAudioID(AkUInt64 _id) : id(_id) {}

	/// Conversion from a pointer to a AkSpatialAudioID
	explicit AkSpatialAudioID(const void * ptr) : id(reinterpret_cast<AkUInt64>(ptr)) {}

	bool operator == (const AkSpatialAudioID& rhs) const { return id == rhs.id; }
	bool operator != (const AkSpatialAudioID& rhs) const { return id != rhs.id; }
	bool operator < (const AkSpatialAudioID& rhs) const { return id < rhs.id; }
	bool operator > (const AkSpatialAudioID& rhs) const { return id > rhs.id; }
	bool operator <= (const AkSpatialAudioID& rhs) const { return id <= rhs.id; }
	bool operator >= (const AkSpatialAudioID& rhs) const { return id >= rhs.id; }

	/// Determine if this ID is valid.
	bool IsValid() const { return id != (AkUInt64)-1; }

	/// Conversion function used internally to convert from a AkSpatialAudioID to a AkGameObjectID.
	AkGameObjectID AsGameObjectID() const { return (AkGameObjectID)id; }
	
	operator AkUInt64 () const { return id; }

	AkUInt64 id;
};

/// Spatial Audio Room ID type.  This ID type exists in the same ID-space as game object ID's.  The client is responsible for not choosing room ID's
/// that conflict with registered game objects' ID's.  Internally, the spatial audio rooms and portals API manages registration and un-registration of game objects that 
/// represent rooms using AkRoomID's provided by the client; AkRoomID's are converted to AkGameObjectID's by calling AsGameObjectID(). 
/// \sa 
///	- \ref AK::SpatialAudio::SetRoom
///	- \ref AK::SpatialAudio::RemoveRoom
struct AkRoomID : public AkSpatialAudioID
{
	/// Default constructor.  Creates an invalid ID.
	constexpr AkRoomID() : AkSpatialAudioID() {}

	/// Construct from a 64-bit int.
	AkRoomID(AkUInt64 _id) : AkSpatialAudioID(_id) {}

	/// Conversion from a pointer to a AkRoomID
	explicit AkRoomID(const void * ptr) : AkSpatialAudioID(ptr) {}

	/// Conversion function used to convert AkRoomID's to AkGameObjectIDs.
	AkGameObjectID AsGameObjectID() const { return IsValid() ? (AkGameObjectID)id : OutdoorsGameObjID; }
	
	/// Conversion function used to convert to AkGameObjectIDs to AkRoomID.
	static AkRoomID FromGameObjectID(AkGameObjectID in_fromGameObject)
	{
		AkRoomID id;
		if (in_fromGameObject != OutdoorsGameObjID)
			id.id = (AkUInt64)in_fromGameObject;
		return id;
	}

private:
	/// A game object ID that is in the reserved range, used for 'outdoor' rooms, i.e. when not in a room.
	/// \akwarning This AkGameObjectID is the underlying game object ID of the outdoor room, and should not be confused with the actual outdoor room's ID, AK::SpatialAudio::kOutdoorRoomID.\endakwarning
	static const AkGameObjectID OutdoorsGameObjID = (AkGameObjectID)-4;
};

namespace AK
{
	namespace SpatialAudio
	{
		/// The outdoor room ID. This room is created automatically and is typically used for outdoors, i.e. when not in a room. 
		constexpr AkRoomID kOutdoorRoomID;
	}
}

struct AkVertex : public AkVector
{
	AkVertex() : AkVector{ 0, 0, 0 } {}
	AkVertex(AkReal32 in_x, AkReal32 in_y, AkReal32 in_z)
		: AkVector{ in_x, in_y, in_z }
	{}
};

#else

typedef AkUInt64 AkSpatialAudioID;
typedef AkSpatialAudioID AkRoomID;

typedef struct AkVector AkVertex;

#endif


///< Unique ID for portals.  This ID type exists in the same ID-space as game object ID's.  The client is responsible for not choosing portal ID's
/// that conflict with registered game objects' ID's.  Internally, the spatial audio rooms and portals API manages registration and un-registration of game objects that 
/// represent portals using AkPortalID's provided by the client; AkPortalID's are convertied to AkGameObjectID's by calling AsGameObjectID(). 
/// \sa 
///	- \ref AK::SpatialAudio::SetPortal
///	- \ref AK::SpatialAudio::RemovePortal  
typedef AkSpatialAudioID AkPortalID;

///< Unique ID for identifying geometry sets.  Chosen by the client using any means desired.  
/// \sa 
///	- \ref AK::SpatialAudio::SetGeometry
///	- \ref AK::SpatialAudio::RemoveGeometry
typedef AkSpatialAudioID AkGeometrySetID;

///< Unique ID for identifying geometry set instances.  Chosen by the client using any means desired.  
/// \sa 
///	- \ref AK::SpatialAudio::SetGeometry
///	- \ref AK::SpatialAudio::RemoveGeometry
typedef AkSpatialAudioID AkGeometryInstanceID;

/// Data used to describe one image source in Reflect.
struct AkImageSourceName
{
#ifdef __cplusplus
	AkImageSourceName()
		: uNumChar(0)
		, pName(NULL)
	{
	}

	void SetName(const char* in_pName);
#endif

	AkUInt32 uNumChar;							///< Number of characters in image source name.
	const char* pName;							///< Optional image source name. Appears in Reflect's editor when profiling.
};


/// Initialization settings of the spatial audio module.
struct AkSpatialAudioInitSettings
{
#ifdef __cplusplus
	AkSpatialAudioInitSettings()
		: uMaxSoundPropagationDepth(AK_MAX_SOUND_PROPAGATION_DEPTH)
		, fMovementThreshold(0.25f)
		, uNumberOfPrimaryRays(35)
		, uMaxReflectionOrder(2)
		, uMaxDiffractionOrder(4)
		, uMaxDiffractionPaths(8)
		, uMaxGlobalReflectionPaths(0)
		, uMaxEmitterRoomAuxSends(3)
		, uDiffractionOnReflectionsOrder(2)
		, fMaxDiffractionAngleDegrees(180.f)
		, fMaxPathLength(1000.0f)
		, fCPULimitPercentage(0.0f)
		, uLoadBalancingSpread(1)
		, fSmoothingConstantMs(0.f)
		, fAdjacentRoomBleed(1.f)
		, bEnableGeometricDiffractionAndTransmission(true)
		, bCalcEmitterVirtualPosition(true)
		, eTransmissionOperation(AkTransmissionOperation_Default)
		, uClusteringMinPoints(0)
		, fClusteringMaxDistance(5.0)
		, fClusteringDeadZoneDistance(10.0f)
	{}
#endif

	AkUInt32 uMaxSoundPropagationDepth;				///< Maximum number of rooms that sound can propagate through; must be less than or equal to AK_MAX_SOUND_PROPAGATION_DEPTH.  Values below 2 will not propagate sound past the listener's own room.
	AkReal32 fMovementThreshold;					///< Amount that an emitter or listener has to move to trigger a validation of reflections/diffraction. Larger values can reduce the CPU load at the cost of reduced accuracy. Note that the ray tracing itself is not affected by this value. Rays are cast each time a Spatial Audio update is executed.
	AkUInt32 uNumberOfPrimaryRays;					///< The number of primary rays used in the ray tracing engine. A larger number of rays will increase the chances of finding reflection and diffraction paths, but will result in higher CPU usage.
	AkUInt32 uMaxReflectionOrder;					///< Maximum reflection order [1, 4] - the number of 'bounces' in a reflection path. A high reflection order renders more details at the expense of higher CPU usage.
	AkUInt32 uMaxDiffractionOrder;					///< Maximum diffraction order [1, 8] - the number of 'bends' in a diffraction path. A high diffraction order accommodates more complex geometry at the expense of higher CPU usage.
	///< Diffraction must be enabled on the geometry to find diffraction paths (see \c AkGeometryParams). Set to 0 to disable diffraction on all geometry.
	///< This parameter limits the recursion depth of diffraction rays cast from the listener to scan the environment, and also the depth of the diffraction search to find paths between emitter and listener.
	///< To optimize CPU usage, set it to the maximum number of edges you expect the obstructing geometry to traverse. 
	///< For example, if box-shaped geometry is used exclusively, and only a single box is expected between an emitter and then listener, limiting \c uMaxDiffractionOrder to 2 may be sufficient.
	///< A diffraction path search starts from the listener, so when the maximum diffraction order is exceeded, the remaining geometry between the end of the path and the emitter is ignored. 
	///< In such case, where the search is terminated before reaching the emitter, the diffraction coefficient will be underestimated. It is calculated from a partial path, ignoring any remaining geometry.	
	AkUInt32 uMaxDiffractionPaths;					///< Limit the maximum number of diffraction paths computed per emitter, excluding the direct/transmission path. The acoustics engine searches for up to uMaxDiffractionPaths paths and stops searching when this limit is reached. 
	///< Setting a low number for uMaxDiffractionPaths (1-4) uses fewer CPU resources, but is more likely to cause discontinuities in the resulting audio. This can occur, for example, when a more prominent path is discovered, displacing a less prominent one. 
	///< Conversely, a larger number (8 or more) produces higher quality output but requires more CPU resources. The recommended range is 2-8.
	AkUInt32 uMaxGlobalReflectionPaths;					///< [\ref spatial_audio_experimental "Experimental"] Set a global reflection path limit among all sound emitters with early reflections enabled. Potential reflection paths, discovered by raycasting, are first sorted according to a heuristic to determine which paths are the most prominent. 
	///< Afterwards, the full reflection path calculation is performed on only the uMaxGlobalReflectionPaths, most prominent paths. Limiting the total number of reflection path calculations can significantly reduce CPU usage. Recommended range: 10-50. 
	///< Set to 0 to disable the limit. In this case, the number of paths computed is unbounded and depends on how many are discovered by raycasting.
	AkUInt32 uMaxEmitterRoomAuxSends;				///< The maximum number of game-defined auxiliary sends that can originate from a single emitter. An emitter can send to its own room, and to all adjacent rooms if the emitter and listener are in the same room. If a limit is set, the most prominent sends are kept, based on spread to the adjacent portal from the emitters perspective.
	///< Set to 1 to only allow emitters to send directly to their current room, and to the room a listener is transitioning to if inside a portal. Set to 0 to disable the limit.
	AkUInt32 uDiffractionOnReflectionsOrder;		///< The maximum possible number of diffraction points at each end of a reflection path. Diffraction on reflection allows reflections to fade in and out smoothly as the listener or emitter moves in and out of the reflection's shadow zone.
	///< When greater than zero, diffraction rays are sent from the listener to search for reflections around one or more corners from the listener.
	///< Diffraction must be enabled on the geometry to find diffracted reflections (see \c AkGeometryParams). Set to 0 to disable diffraction on reflections.
	///< To allow reflections to propagate through portals without being cut off, set \c uDiffractionOnReflectionsOrder to 2 or greater.
	AkReal32 fMaxDiffractionAngleDegrees;			///< The largest possible diffraction value, in degrees, beyond which paths are not computed and are inaudible. Must be greater than zero. Default value: 180 degrees. 
	///< A large value (for example, 360 degrees) allows paths to propagate further around corners and obstacles, but takes more CPU time to compute. 
	///< A gain is applied to each diffraction path to taper the volume of the path to zero as the diffraction angle approaches fMaxDiffractionAngleDegrees,
	///< and appears in the Voice Inspector as "Diffraction Tapering". This tapering gain is applied in addition to the diffraction curves, and prevents paths from popping in or out suddenly when the maximum diffraction angle is exceeded. 
	///< In Wwise Authoring, the horizontal axis of a diffraction curve in the attenuation editor is defined over the range 0-100%, corresponding to angles 0-180 degrees.
	///< If fMaxDiffractionAngleDegrees is greater than 180 degrees, diffraction coefficients over 100% are clamped and the curve is evaluated at the rightmost point. 
	AkReal32 fMaxPathLength;						///< The total length of a path composed of a sequence of segments (or Sound Paths) cannot exceed the defined maximum path length. High values compute longer paths but increase the CPU cost.
	///< Each individual sound is also affected by its maximum attenuation distance, specified in the Authoring tool. Reflection or diffraction paths, calculated inside Spatial Audio, will never exceed a sound's maximum attenuation distance.
	///< Note, however, that attenuation is considered infinite if the furthest point is above the audibility threshold.
	AkReal32 fCPULimitPercentage;					///< Defines the targeted max computation time allocated for spatial audio. Defined as a percentage [0, 100] of the current audio frame. When the value is different from 0, Spatial Audio adapts dynamically the load balancing spread value between 1 and the specified AkSpatialAudioInitSettings::uLoadBalancingSpread according to current CPU usage and the specified CPU limit. Set to 0 to disable the dynamic load balancing spread computation.
	AkUInt32 uLoadBalancingSpread;					///< Spread the computation of paths on uLoadBalancingSpread frames [1..[. When uLoadBalancingSpread is set to 1, no load balancing is done. Values greater than 1 indicate the computation of paths will be spread on this number of frames. When CPU limit is active (see AkSpatialAudioInitSettings::fCPULimitPercentage), this setting represents the upper bound spread used by the dynamic load balancing instead of a fixed value.
	AkReal32 fSmoothingConstantMs;					///< [\ref spatial_audio_experimental "Experimental"]  Enable parameter smoothing on the diffraction paths output from the Acoustics Engine. Set fSmoothingConstantMs to a value greater than 0 to define the time constant (in milliseconds) for parameter smoothing. 
	///< The time constant of an exponential moving average is the amount of time for the smoothed response of a unit step function to reach 1 - 1/e ~= 63.2% of the original signal. 
	///< A large value (eg. 500-1000 ms) results in less variance but introduces lag, which is a good choice when using conservative values for uNumberOfPrimaryRays (eg. 5-10), uMaxDiffractionPaths (eg. 1-3) or fMovementThreshold ( > 1m ), in order to reduce overall CPU cost. 
	///< A small value (eg. 10-100 ms) results in greater accuracy and faster convergence of rendering parameters. Set to 0 to disable path smoothing.
	AkReal32 fAdjacentRoomBleed;					///< Set a global scaling factor that manipulates reverb send values. AkSpatialAudioInitSettings::fAdjacentRoomBleed affects the proportion of audio sent to adjacent rooms 
	///< and to the emitter's current room.
	///< This value is multiplied by AkPortalParams::AdjacentRoomBleed, which is used to scale reverb bleed for individual portals.
	///< When calculating reverb send amounts, each portal's aperture is multiplied by fAdjacentRoomBleed, which alters its perceived size:
	///<	- 1.0 (default): Maintains portals at their true geometric sizes (no scaling).
	///<	- > 1.0: Increases the perceived size of all portals, which increases bleed into adjacent rooms.
	///<	- < 1.0: Decreases the perceived size of all portals, which reduces bleed into adjacent rooms.
	///< Valid range: (0.0 - infinity)
	///< Note: Values close to 0 might cause abrupt portal transitions.
	bool bEnableGeometricDiffractionAndTransmission;///< Enable computation of geometric diffraction and transmission paths for all sources that have the <b>Enable Diffraction and Transmission</b> box checked in the Positioning tab of the Wwise Property Editor.
	///< This flag enables sound paths around (diffraction) and through (transmission) geometry (see \c AK::SpatialAudio::SetGeometry).
	///< Setting \c bEnableGeometricDiffractionAndTransmission to false implies that geometry is only to be used for reflection calculation.
	///< Diffraction edges must be enabled on geometry for diffraction calculation (see \c AkGeometryParams).
	///< If \c bEnableGeometricDiffractionAndTransmission is false but a sound has <b>Enable Diffraction and Transmission</b> selected in the Positioning tab of the authoring tool, the sound will diffract through portals but will pass through geometry as if it is not there.
	///< One would typically disable this setting in the case that the game intends to perform its own obstruction calculation, but geometry is still passed to spatial audio for reflection calculation.
	bool bCalcEmitterVirtualPosition;				///< An emitter that is diffracted through a portal or around geometry will have its apparent or virtual position calculated by Wwise Spatial Audio and passed on to the sound engine.
	enum AkTransmissionOperation eTransmissionOperation; ///< The operation used to determine transmission loss on direct paths.

	AkUInt32 uClusteringMinPoints;	///< [\ref spatial_audio_experimental "Experimental"] Minimum number of emitters in a cluster. Default value is 0. Values less than 2 disable the clustering.
	///< Note: Emitters with multi-positions are not clustered and are treated as independant emitters.
	///< Note: Changing an emitter from single to multi-positions with load balancing enabled (see \ref AkSpatialAudioInitSettings::uLoadBalancingSpread) might lead to unknown behaviors for a few frames.
	AkReal32 fClusteringMaxDistance; ///< Max distance between emitters to be considered as neighbors. This distance is specified for the reference distance defined by fClusteringDeadZoneDistance. Default value is 5.0.
	AkReal32 fClusteringDeadZoneDistance; ///< Defines a dead zone around the listener where no emitters are clusters. Default value is 10.0.
};

#pragma pack(push, 4)

struct AkImageSourceParams
{
#ifdef __cplusplus
	AkImageSourceParams()
		: fDistanceScalingFactor(1.f)
		, fLevel(1.f)
		, fDiffraction(0.f)
		, fOcclusion(0.f)
		, uDiffractionEmitterSide(0)
		, uDiffractionListenerSide(0)
	{
		sourcePosition.X = 0.f;
		sourcePosition.Y = 0.f;
		sourcePosition.Z = 0.f;
	}

	AkImageSourceParams(AkVector64 in_sourcePosition, AkReal32 in_fDistanceScalingFactor, AkReal32 in_fLevel)
		: sourcePosition(in_sourcePosition)
		, fDistanceScalingFactor(in_fDistanceScalingFactor)
		, fLevel(in_fLevel)
		, fDiffraction(0.f)
		, fOcclusion(0.f)
		, uDiffractionEmitterSide(0)
		, uDiffractionListenerSide(0)
	{
	}
#endif

	struct AkVector64 sourcePosition;           ///< Image source position, relative to the world.
	AkReal32 fDistanceScalingFactor;            ///< Image source distance scaling. This number effectively scales the sourcePosition vector with respect to the listener and, consequently, scales distance and preserves orientation.
	AkReal32 fLevel;                            ///< Game-controlled level for this source, linear.
	AkReal32 fDiffraction;                      ///< Diffraction amount, normalized to the range [0,1].
	AkReal32 fOcclusion;                        ///< Portal occlusion amount, in the range [0,1].
	AkUInt8 uDiffractionEmitterSide;            ///< If there is a shadow zone diffraction just after the emitter in the reflection path, indicates the number of diffraction edges, otherwise 0 if no diffraction.
	AkUInt8 uDiffractionListenerSide;           ///< If there is a shadow zone diffraction before reaching the listener in the reflection path, indicates the number of diffraction edges, otherwise 0 if no diffraction.
};

struct AkImageSourceTexture
{
#ifdef __cplusplus
	AkImageSourceTexture()
		: uNumTexture(1)
	{
		arTextureID[0] = AK_INVALID_UNIQUE_ID;
	}
#endif

	AkUInt32 uNumTexture;						///< Number of valid textures in the texture array.
	AkUniqueID arTextureID[AK_MAX_NUM_TEXTURE];	///< Unique IDs of the Acoustics Texture ShareSets used to filter this image source.
};

/// Settings for individual image sources.
struct AkImageSourceSettings
{
#ifdef __cplusplus
	/// Constructor
	AkImageSourceSettings() {}

	/// Constructor with parameters
	AkImageSourceSettings(AkVector64 in_sourcePosition, AkReal32 in_fDistanceScalingFactor, AkReal32 in_fLevel)
		: params(in_sourcePosition, in_fDistanceScalingFactor, in_fLevel)
		, texture()
	{
	}

	/// Helper function to set a single Acoustic Texture.
	void SetOneTexture(AkUniqueID in_texture)
	{
		texture.uNumTexture = 1;
		texture.arTextureID[0] = in_texture;
	}
#endif

	/// Image source parameters.
	struct AkImageSourceParams params;

	/// Acoustic Texture that goes with this image source.
	/// \akwarning Changing Acoustic Textures across frames for a given image source, identified by in_srcID, might cause discontinuity in the audio signal.
	/// \sa SetImageSource
	struct AkImageSourceTexture texture;
};

#pragma pack(pop)

///  AkExtent describes an extent with width, height and depth. halfWidth, halfHeight and halfDepth should form a vector from the centre of the volume to the positive corner.
///  Used in \c AkPortalParams, negative values in the extent will cause an error. For rooms, negative values can be used to opt out of room transmission.
struct AkExtent
{
#ifdef __cplusplus
	AkExtent(): halfWidth(0.0f), halfHeight(0.0f), halfDepth(0.0f) {}

	AkExtent(AkReal32 in_halfWidth, AkReal32 in_halfHeight, AkReal32 in_halfDepth)
		: halfWidth(in_halfWidth)
		, halfHeight(in_halfHeight)
		, halfDepth(in_halfDepth)
	{}
#endif

	AkReal32 halfWidth;
	AkReal32 halfHeight;
	AkReal32 halfDepth;
};

/// Triangle for a spatial audio mesh. 
struct AkTriangle
{
#ifdef __cplusplus
	/// Constructor
	AkTriangle() : point0(AK_INVALID_VERTEX)
		, point1(AK_INVALID_VERTEX)
		, point2(AK_INVALID_VERTEX)
		, surface(AK_INVALID_SURFACE)
	{}

	/// Constructor
	AkTriangle(AkVertIdx in_pt0, AkVertIdx in_pt1, AkVertIdx in_pt2, AkSurfIdx in_surfaceInfo)
		: point0(in_pt0)
		, point1(in_pt1)
		, point2(in_pt2)
		, surface(in_surfaceInfo)
	{}
#endif

	/// Index into the vertex table passed into \c AkGeometryParams that describes the first vertex of the triangle. Triangles are double-sided, so vertex order in not important.
	AkVertIdx point0;

	/// Index into the vertex table passed into \c AkGeometryParams that describes the second vertex of the triangle. Triangles are double-sided, so vertex order in not important.
	AkVertIdx point1;

	/// Index into the vertex table passed into \c AkGeometryParams that describes the third vertex of the triangle. Triangles are double-sided, so vertex order in not important.
	AkVertIdx point2;

	/// Index into the surface table passed into \c AkGeometryParams that describes the surface properties of the triangle.  
	/// If this field is left as \c AK_INVALID_SURFACE, then a default-constructed \c AkAcousticSurface is used.
	AkSurfIdx surface;
};

/// Describes the acoustic surface properties of one or more triangles.
/// An single acoustic surface may describe any number of triangles, depending on the granularity desired.  For example, if desired for debugging, one could create a unique 
/// \c AkAcousticSurface struct for each triangle, and define a unique name for each.  Alternatively, a single \c AkAcousticSurface could be used to describe all triangles.
/// In fact it is not necessary to define any acoustic surfaces at all.  If the \c AkTriangle::surface field is left as \c AK_INVALID_SURFACE, then a default-constructed \c AkAcousticSurface is used.
struct AkAcousticSurface
{
#ifdef __cplusplus
	/// Constructor
	AkAcousticSurface() : textureID(AK_INVALID_UNIQUE_ID)
		, transmissionLoss(1.0f)
		, strName(NULL)
	{}
#endif

	/// Acoustic Texture ShareSet ID for the surface.  The Acoustic Texture is authored in Wwise, and the ShareSet ID can be obtained by calling \c AK::SoundEngine::GetIDFromString
	/// \sa <tt>\ref AK::SoundEngine::GetIDFromString()</tt>
	AkUInt32 textureID;

	/// Transmission loss value to apply when simulating sound transmission through this geometric surface. 
	/// Transmission is only simulated on a sound when the sound has <b>Enable Diffraction and Transmission</b> box
	/// enabled in Wwise Authoring. 
	/// 
	/// If more than one surface is intersected between the emitter and the listener, the result depends on \c AkSpatialAudioInitSettings::eTransmissionOperation. The default behavior is to use the maximum of all surfaces' transmission loss values.
	/// 
	/// Transmission loss is applied on the mix connection between the emitter and the listener for the Direct Propagation Path, and between the emitter and the room for the Diffuse Propagation Path.
	/// The transmission loss value is converted to volume attenuation, low-pass and/or high-pass filtering, using the transmission loss curves defined on the sound in Wwise Authoring.
	/// 
	/// A transmission loss value of 0 has special meaning in some contexts: 
	/// - Setting a transmission loss value of 0 effectively disables a surface for ray intersection. It is ignored for all diffraction and reflection calculations.
	/// - When geometry is used to define the shape of a Reverb Zone, surfaces with a transmission loss of 0 define the center of the transition between the Reverb Zone and its parent Room. 
	/// Only surfaces with transmission loss of 0 define the transition region, allowing the user to have transitions around parts of the geometry (certain walls, for example), and not others.
	/// Valid range: (0.f-1.f)
	/// - \ref AkRoomParams
	/// - \ref AK::SpatialAudio::SetReverbZone
	AkReal32 transmissionLoss;

	/// Name to describe this surface
	const char* strName;
};

/// Structure for retrieving information about the indirect paths of a sound that have been calculated via the geometric reflections API. Useful for debug draw applications.
struct AkReflectionPathInfo
{
	/// Apparent source of the reflected sound that follows this path.
	struct AkVector64 imageSource;

	/// Vertices of the indirect path.
	/// pathPoint[0] is closest to the emitter, pathPoint[numPathPoints-1] is closest to the listener.
	struct AkVector64 pathPoint[AK_MAX_REFLECTION_PATH_LENGTH];

	/// The texture that were hit in the path.
	/// textureIDs[0] is closest to the emitter, textureIDs[numPathPoints-1] is closest to the listener.
	AkUInt32 textureIDs[AK_MAX_REFLECTION_PATH_LENGTH];

	/// Number of valid elements in the \c pathPoint[], \c surfaces[], and \c diffraction[] arrays.
	AkUInt32 numPathPoints;

	/// Number of reflections in the \c pathPoint[] array. Shadow zone diffraction does not count as a reflection. If there is no shadow zone diffraction, \c numReflections is equal to \c numPathPoints.
	AkUInt32 numReflections;

	/// Diffraction amount, normalized to the range [0,1]
	AkReal32 diffraction[AK_MAX_REFLECTION_PATH_LENGTH];

	/// Linear gain applied to image source.
	AkReal32 level;

	/// Deprecated - always false. Occluded paths are not generated.
	bool isOccluded;
};

/// Structure for retrieving information about paths for a given emitter. 
/// The diffraction paths represent indirect sound paths from the emitter to the listener, whether they go through portals 
/// (via the rooms and portals API) or are diffracted around edges (via the geometric diffraction API).
/// The direct path is included here and can be identified by checking \c nodeCount == 0. The direct path may have a non-zero transmission loss 
/// if it passes through geometry or between rooms.
struct AkDiffractionPathInfo
{
#ifdef __cplusplus
	/// Defines the maximum number of nodes that a user can retrieve information about.  Longer paths will be truncated. 
	static const AkUInt32 kMaxNodes = AK_MAX_SOUND_PROPAGATION_DEPTH;
#endif

	/// Diffraction points along the path. nodes[0] is the point closest to the listener; nodes[numNodes-1] is the point closest to the emitter. 
	/// Neither the emitter position nor the listener position are represented in this array.
	struct AkVector64 nodes[AK_MAX_SOUND_PROPAGATION_DEPTH];

	/// Emitter position. This is the source position for an emitter. In all cases, except for radial emitters, it is the same position as the game object position.
	/// For radial emitters, it is the calculated position at the edge of the volume.
	struct AkVector64 emitterPos;

	/// Raw diffraction angles at each point, in radians.
	AkReal32 angles[AK_MAX_SOUND_PROPAGATION_DEPTH];

	/// ID of the portals that the path passes through.  For a given node at position i (in the nodes array), if the path diffracts on a geometric edge, then portals[i] will be an invalid portal ID (ie. portals[i].IsValid() will return false). 
	/// Otherwise, if the path diffracts through a portal at position i, then portals[i] will be the ID of that portal.
	/// portal[0] represents the node closest to the listener; portal[numNodes-1] represents the node closest to the emitter.
	AkPortalID portals[AK_MAX_SOUND_PROPAGATION_DEPTH];

	/// ID's of the rooms that the path passes through. For a given node at position i, room[i] is the room on the listener's side of the node. If node i diffracts through a portal, 
	/// then rooms[i] is on the listener's side of the portal, and rooms[i+1] is on the emitters side of the portal.
	/// There is always one extra slot for a room so that the emitters room is always returned in slot room[numNodes] (assuming the path has not been truncated).
	AkRoomID rooms[AK_MAX_SOUND_PROPAGATION_DEPTH + 1];

	/// Virtual emitter position. This is the position that is passed to the sound engine to render the audio using multi-positioning, for this particular path.
	struct AkWorldTransform virtualPos;

	/// Total number of nodes in the path.  Defines the number of valid entries in the \c nodes, \c angles, and \c portals arrays. The \c rooms array has one extra slot to fit the emitter's room.
	AkUInt32 nodeCount;

	/// Calculated total diffraction from this path, normalized to the range [0,1]
	/// The diffraction amount is calculated from the sum of the deviation angles from a straight line, of all angles at each nodePoint. 
	//	Can be thought of as how far into the 'shadow region' the sound has to 'bend' to reach the listener.
	/// This value is applied internally, by spatial audio, as the Diffraction value and built-in parameter of the emitter game object.
	/// \sa
	/// - \ref AkSpatialAudioInitSettings
	AkReal32 diffraction;

	/// Calculated total transmission loss from this path, normalized to the range [0,1]
	/// This field will be 0 for diffraction paths where \c nodeCount > 0. It may be non-zero for the direct path where \c nodeCount == 0.
	/// The path's transmission loss value might be geometric transmission loss, if geometry was intersected in the path, 
	/// or room transmission loss, if no geometry was available.
	/// The geometric transmission loss is calculated from the transmission loss values assigned to the geometry that this path transmits through.
	/// If a path transmits through multiple geometries with different transmission loss values, the largest value is taken.
	/// The room transmission loss is taken from the emitter and listener rooms' transmission loss values, and likewise, 
	/// if the listener's room and the emitter's room have different transmission loss values, the greater of the two is used.
	/// This value is applied internally, by spatial audio, as the Transmission Loss value and built-in parameter of the emitter game object.
	/// \sa
	/// - \ref AkSpatialAudioInitSettings
	/// - \ref AkRoomParams
	/// - \ref AkAcousticSurface
	AkReal32 transmissionLoss;

	/// Total path length
	/// Represents the sum of the length of the individual segments between nodes, with a correction factor applied for diffraction. 
	/// The correction factor simulates the phenomenon where by diffracted sound waves decay faster than incident sound waves and can be customized in the spatial audio init settings.
	/// \sa
	/// - \ref AkSpatialAudioInitSettings
	AkReal32 totLength;

	/// Obstruction value for this path 
	/// This value includes the accumulated portal obstruction for all portals along the path.
	AkReal32 obstructionValue;

	/// Occlusion value for this path 
	/// This value includes the accumulated portal occlusion for all portals along the path.
	AkReal32 occlusionValue;

	/// Propagation path gain.
	/// Includes volume tapering gain to ensure that diffraction paths do not cut in or out when the maximum diffraction angle is exceeded.
	AkReal32 gain;
};

#pragma pack(push, 4)

/// Parameters passed to \c SetPortal
struct AkPortalParams
{
#ifdef __cplusplus
	/// Constructor
	AkPortalParams()
		: AdjacentRoomBleed(1.f)
		, bEnabled(false)
	{}
#endif

	/// Portal's position and orientation in the 3D world. 
	/// Position vector is the center of the opening.
	/// OrientationFront vector must be unit-length and point along the normal of the portal, and must be orthogonal to Up. It defines the local positive-Z dimension (depth/transition axis) of the portal, used by Extent. 
	/// OrientationTop vector must be unit-length and point along the top of the portal (tangent to the wall), must be orthogonal to Front. It defines the local positive-Y direction (height) of the portal, used by Extent.
	struct AkWorldTransform Transform;

	/// Portal extent. Defines the dimensions of the portal relative to its center; all components must be positive numbers.
	/// The shape described by these extents is used to "cut out" the opening shape of room geometry. Geometry that overlaps the portal extent is effectively ignored.
	/// The depth dimension is used to perform transitions between connected rooms by manipulating game-defined auxiliary sends.
	/// The depth dimension is also used to place game objects into rooms while they are inside the portals.
	/// \sa
	/// - \ref AkExtent
	struct AkExtent Extent;

	/// ID of the room to which the portal connects, in the direction of the Front vector.  If a room with this ID has not been added via AK::SpatialAudio::SetRoom,
	/// a room will be created with this ID and with default AkRoomParams.  If you would later like to update the AkRoomParams, simply call AK::SpatialAudio::SetRoom again with this same ID.
	///	- \ref AK::SpatialAudio::SetRoom
	///	- \ref AK::SpatialAudio::RemoveRoom
	/// - \ref AkRoomParams
	AkRoomID FrontRoom;

	/// ID of the room to which the portal connects, in the direction opposite to the Front vector. If a room with this ID has not been added via AK::SpatialAudio::SetRoom,
	/// a room will be created with this ID and with default AkRoomParams.  If you would later like to update the AkRoomParams, simply call AK::SpatialAudio::SetRoom again with this same ID.
	///	- \ref AK::SpatialAudio::SetRoom
	///	- \ref AK::SpatialAudio::RemoveRoom
	/// - \ref AkRoomParams
	AkRoomID BackRoom;

	/// A scaling factor that manipulates reverb send values. AkPortalParams::AdjacentRoomBleed affects the proportion of audio sent to the adjacent room versus the proportion sent to the emitter's current room, for this particular portal.
	/// This value is multiplied by AkSpatialAudioInitSettings::fAdjacentRoomBleed, which is used to scale reverb bleed for all portals.
	/// When calculating reverb send amounts, the portal's aperture is multiplied by AdjacentRoomBleed, altering its perceived size:
	///	- 1.0 (default): Maintain the portal at its true geometric size (no scaling).
	///	- > 1.0: Increases the perceived portal size, allowing more bleed into adjacent rooms.
	///	- < 1.0: Decreases the perceived portal size, reducing bleed into adjacent rooms.
	/// Valid range: (0.0 - infinity)
	/// Note: Values approaching 0 may result in abrupt portal transitions.
	AkReal32 AdjacentRoomBleed;

	/// Whether or not the portal is active/enabled. For example, this parameter may be used to simulate open/closed doors.
	/// Portal diffraction is simulated when at least one portal exists and is active between an emitter and the listener.
	/// To simulate a door that opens or closes gradually, use \c AK::SpatialAudio::SetPortalObstructionAndOcclusion to apply occlusion to a portal, according to the door's opening amount.
	/// \sa
	/// - \ref AK::SpatialAudio::SetPortalObstructionAndOcclusion
	bool bEnabled;
};

/// Parameters passed to \c SetRoom
struct AkRoomParams
{
#ifdef __cplusplus
	/// Constructor
	AkRoomParams() : ReverbAuxBus(AK_INVALID_AUX_ID)
		, ReverbLevel(1.f)
		, TransmissionLoss(1.f)
		, RoomGameObj_AuxSendLevelToSelf(0.f)
		, RoomPriority(100.f)
		, DistanceBehavior(AkRoomDistanceBehavior_Default)
		, RoomGameObj_KeepRegistered(false)
	{
		// default invalid values
		Up.X = 0.f;
		Up.Y = 1.f;
		Up.Z = 0.f;
		Front.X = 0.f;
		Front.Y = 0.f;
		Front.Z = 1.f;
	}
#endif

	/// Room Orientation. Up and Front must be orthonormal.
	/// Room orientation has an effect when the associated aux bus (see ReverbAuxBus) is set with 3D Spatialization in Wwise, as 3D Spatialization implements relative rotation of the emitter (room) and listener.
	struct AkVector Front;

	/// Room Orientation. Up and Front must be orthonormal.
	/// Room orientation has an effect when the associated aux bus (see ReverbAuxBus) is set with 3D Spatialization in Wwise, as 3D Spatialization implements relative rotation of the emitter (room) and listener.
	struct AkVector Up;

	/// The reverb aux bus that is associated with this room.  
	/// When Spatial Audio is told that a game object is in a particular room via SetGameObjectInRoom, a send to this aux bus will be created to model the reverb of the room.
	/// Using a combination of Rooms and Portals, Spatial Audio manages which game object the aux bus is spawned on, and what control gain is sent to the bus.  
	/// When a game object is inside a connected portal, as defined by the portal's orientation and extent vectors, both this aux send and the aux send of the adjacent room are active.
	/// Spatial audio modulates the control value for each send based on the game object's position, in relation to the portal's z-azis and extent, to crossfade the reverb between the two rooms.
	/// If more advanced control of reverb is desired, SetGameObjectAuxSendValues can be used to add additional sends on to a game object.
	/// - \ref AK::SpatialAudio::SetGameObjectInRoom
	/// - \ref AK::SoundEngine::SetGameObjectAuxSendValues
	AkAuxBusID						ReverbAuxBus;

	/// The reverb control value for the send to ReverbAuxBus. Valid range: (0.f-1.f)
	/// Can be used to implement multiple rooms that share the same aux bus, but have different reverb levels.
	AkReal32						ReverbLevel;

	/// Characteristic transmission loss value for the Room, describing the average amount of sound energy dissipated by the walls of the room.
	/// 
	/// The Room transmission loss value is used in the following situations: 
	/// 1) If a room tone is playing on the Room Game Object, in a different room than that of the listener. 
	///    Transmission loss is only applied when the listener and the emitter are in different rooms.
	///    It is taken as the maximum between the emitter's room's transmission loss value and the listener's room's transmission loss value.
	/// 2) If, for both the emitter and listener's Rooms, geometry is not assigned with \c AkRoomParams::GeometryInstanceID, or the Geometry Instance that is assigned 
	///    has \c AkGeometryInstanceParams::UseForReflectionAndDiffraction set to false. In this case, room transmission loss is applied 
	///	   to the direct path when the emitter and listener are in different rooms.
	///	   When geometry is available, the transmission loss value is calculated instead by intersecting a ray with the geometry between the emitter and listener.
	///    Transmission loss on geometry overrides room transmission loss, allowing a room to have varying degrees of transmission loss (for example, some transparent walls and some opaque walls).
	/// 3) To spatialize the output of the Room Game Object. A low transmission loss value emphasizes panning the room 
	///    towards the room's center (the center of the room bounding box) and a higher value emphasizes panning towards the room's portals.
	/// 
	/// When room transmission is applied directly to either a room tone (situation 1) or a point source emitter (situation 2), it is applied on the mix connection between the sound and the room.
	/// The transmission loss value is converted to volume attenuation, low-pass and/or high-pass filtering, using the transmission loss curves defined on the sound in Wwise Authoring.
	/// 
	/// \aknote Even when relying on geometry for detailed transmission loss values (per triangle), it is still necessary to set \c AkSoundParams::TransmissionLoss to a representitive value so that
	/// the output of the Room Game Object is properly spatialized (situation 3).
	/// 
	/// Valid range: (0.f-1.f)
	/// - \ref AkAcousticSurface
	AkReal32						TransmissionLoss;

	/// Send level for sounds that are posted on the room game object; adds reverb to ambience and room tones. Valid range: (0.f-1.f).  Set to a value greater than 0 to have spatial audio create a send on the room game object, 
	/// where the room game object itself is specified as the listener and ReverbAuxBus is specified as the aux bus. A value of 0 disables the aux send. This should not be confused with ReverbLevel, which is the send level 
	/// for spatial audio emitters sending to the room game object.
	/// \aknote The room game object can be accessed though the ID that is passed to \c SetRoom() and the \c AkRoomID::AsGameObjectID() method.  Posting an event on the room game object leverages automatic room game object placement 
	///	by spatial audio so that when the listener is inside the room, the sound comes from all around the listener, and when the listener is outside the room, the sound comes from the portal(s). Typically, this would be used for
	/// surround ambiance beds or room tones. Point source sounds should use separate game objects that are registered as spatial audio emitters.
	/// \sa
	/// - \ref AkRoomParams::RoomGameObj_KeepRegistered
	/// - \ref AkRoomID
	AkReal32						RoomGameObj_AuxSendLevelToSelf;

	/// Assign a Geometry Instance \c GeometryInstanceID, which describes the size and shape of the Room. 
	/// Assigning a Geometry Instance to a Room serves several purposes: the shape and surface properties of the geometry are used to calculate the transmission of reverb and room tones through walls,
	/// it allows Spatial Audio to automatically determine which Game Objects are in which Room, and it allows for visualization of the room inside the Game Object 3D Viewer.
	/// To create Room geometry, a Geometry Set must be created using AK::SpatialAudio::SetGeometry, and then a Geometry Instance must be created using AK::SpatialAudio::SetGeometryInstance. 
	/// The Room itself, however, can be created either before or after the geometry is created.
	/// \aknote If the geometry instance is only used for room containment and not for reflection, diffraction, or transmission, then set \c AkGeometryInstanceParams::UseForReflectionAndDiffraction to false. 
	/// \sa
	/// - \ref spatial_audio_roomsportals_apiconfigroomgeometry
	/// - \ref AkGeometryInstanceParams
	/// - \ref AK::SpatialAudio::SetGameObjectInRoom
	AkGeometrySetID					GeometryInstanceID;

	/// Associate a priority with this room. Room priority is used by the room containment system to disambiguate cases where an object is inside several rooms at the same time. In this case, the room with the higher priority is selected.
	/// Default priority is 100. The outdoor Room ignores \c AkRoomParams::RoomPriority and always has the lowest possible priority of -FLT_MAX.
	/// \aknote If several rooms have the same highest room priority, the inner one is selected.
	AkReal32						RoomPriority;

	/// Determines how a room interacts with the distance calculation of other rooms that it overlaps or is nested within.
	/// Default is AkRoomDistanceBehavior_Subtract.
	///  - \ref spatial_audio_roomsportals_distance
	enum AkRoomDistanceBehavior		DistanceBehavior;

	/// If set to true, the room game object is registered on calling \c SetRoom(), and not released until the room is deleted or removed with \c RemoveRoom(). If set to false, Spatial Audio registers
	/// the room object only when it is needed by the sound propagation system for the purposes of reverb, and unregisters the game object when all reverb tails are finished.
	/// We recommend that you set RoomGameObj_KeepRegistered to true if you use RTPCs on the room game object, if you call \c SetScalingFactor(), or call \c PostEvent() for the purpose of ambience or room tones.
	/// \aknote The room game object can be accessed through the ID that is passed to \c SetRoom() and the \c AkRoomID::AsGameObjectID() method. Posting an event on the room game object uses automatic room game object placement 
	///	by Spatial Audio so that when the listener is inside the room, the sound comes from all around the listener, and when the listener is outside the room, the sound comes from the portal(s). Typically, this would be used for
	/// surround ambience beds or room tones. For point source sounds, use separate game objects that are registered as Spatial Audio emitters.
	/// \sa
	/// - \ref AkRoomParams::RoomGameObj_AuxSendLevelToSelf
	/// - \ref AkRoomID
	bool							RoomGameObj_KeepRegistered;


};

/// Parameters passed to \c SetGeometry
struct AkGeometryParams
{
#ifdef __cplusplus
	/// Constructor
	AkGeometryParams() : NumTriangles(0), NumVertices(0), NumSurfaces(0), EnableDiffraction(false), EnableDiffractionOnBoundaryEdges(false), Triangles(NULL), Vertices(NULL), Surfaces(NULL) {}
#endif

	/// Number of triangles in Triangles.
	AkTriIdx NumTriangles;

	/// Number of vertices in Vertices.
	AkVertIdx NumVertices;

	/// Number of of AkTriangleInfo structures in in_pTriangleInfo and number of AkTriIdx's in in_infoMap.
	AkSurfIdx NumSurfaces;

	/// Switch to enable or disable geometric diffraction for this Geometry.
	bool EnableDiffraction;

	/// Switch to enable or disable geometric diffraction on boundary edges for this Geometry.  Boundary edges are edges that are connected to only one triangle.
	bool EnableDiffractionOnBoundaryEdges;

	/// Pointer to an array of AkTriangle structures. 
	/// This array will be copied into spatial audio memory and will not be accessed after \c SetGeometry returns.
	///	- \ref AkTriangle
	///	- \ref AK::SpatialAudio::SetGeometry
	///	- \ref AK::SpatialAudio::RemoveGeometry
	AK_WPTR(struct AkTriangle*, Triangles);

	/// Pointer to an array of AkVertex structures. 
	/// This array will be copied into spatial audio memory and will not be accessed after \c SetGeometry returns.
	///	- \ref AkVertex
	///	- \ref AK::SpatialAudio::SetGeometry
	///	- \ref AK::SpatialAudio::RemoveGeometry
	AK_WPTR(AkVertex*, Vertices);

	/// Pointer to an array of AkAcousticSurface structures.
	/// This array will be copied into spatial audio memory and will not be accessed after \c SetGeometry returns.
	///	- \ref AkVertex
	///	- \ref AK::SpatialAudio::SetGeometry
	///	- \ref AK::SpatialAudio::RemoveGeometry
	AK_WPTR(struct AkAcousticSurface*, Surfaces);
};

#define AK_DEFAULT_GEOMETRY_POSITION_X (0.0)
#define AK_DEFAULT_GEOMETRY_POSITION_Y (0.0)
#define AK_DEFAULT_GEOMETRY_POSITION_Z (0.0)
#define AK_DEFAULT_GEOMETRY_FRONT_X (0.0)
#define AK_DEFAULT_GEOMETRY_FRONT_Y (0.0)
#define AK_DEFAULT_GEOMETRY_FRONT_Z (1.0)
#define AK_DEFAULT_GEOMETRY_TOP_X (0.0)
#define AK_DEFAULT_GEOMETRY_TOP_Y (1.0)
#define AK_DEFAULT_GEOMETRY_TOP_Z (0.0)

/// Parameters passed to \c SetGeometryInstance
struct AkGeometryInstanceParams
{
#ifdef __cplusplus
	/// Constructor
	/// Creates an instance with an identity transform.
	/// \akwarning A default-constructed AkGeometryInstanceParams assumes the default floor plane is passed to AkInitSettings::eFloorPlane.
	AkGeometryInstanceParams()
		: Scale({ 1, 1, 1 })
		, UseForReflectionAndDiffraction(true)
		, BypassPortalSubtraction(false)
		, IsSolid(false)
	{
		PositionAndOrientation.Set(
			AK_DEFAULT_GEOMETRY_POSITION_X, AK_DEFAULT_GEOMETRY_POSITION_Y, AK_DEFAULT_GEOMETRY_POSITION_Z,
			AK_DEFAULT_GEOMETRY_FRONT_X, AK_DEFAULT_GEOMETRY_FRONT_Y, AK_DEFAULT_GEOMETRY_FRONT_Z,
			AK_DEFAULT_GEOMETRY_TOP_X, AK_DEFAULT_GEOMETRY_TOP_Y, AK_DEFAULT_GEOMETRY_TOP_Z);
	}
#endif

	/// Set the position and orientation of the geometry instance.
	/// AkWorldTransform uses one vector to define the position of the geometry instance, and two more to define the orientation; a forward vector and an up vector. 
	/// To ensure that a geometry instance has the correct rotation with respect to the game, AkInitSettings::eFloorPlane must be initialized with the correct value.
	///	\sa
	/// - \ref AkInitSettings::eFloorPlane
	/// - \ref AK::SpatialAudio::SetGeometryInstance
	///	- \ref AK::SpatialAudio::RemoveGeometryInstance
	struct AkWorldTransform PositionAndOrientation;

	/// Set the 3-dimensional scaling of the geometry instance.
	/// \sa
	/// - \ref AK::SpatialAudio::SetGeometryInstance
	///	- \ref AK::SpatialAudio::RemoveGeometryInstance
	struct AkVector Scale;

	/// Geometry set referenced by the instance
	/// \sa
	///	- \ref AK::SpatialAudio::SetGeometry
	///	- \ref AK::SpatialAudio::RemoveGeometry
	/// - \ref AK::SpatialAudio::SetGeometryInstance
	///	- \ref AK::SpatialAudio::RemoveGeometryInstance
	AkGeometrySetID GeometrySetID;

	/// When enabled, the geometry instance is indexed for ray computation and used to compute reflection, diffraction, and transmission.
	/// If the geometry instance is used only for room containment, this flag must be set to false.
	///	- \ref AK::SpatialAudio::SetRoom
	///	- \ref AkRoomParams
	///
	bool UseForReflectionAndDiffraction;

	/// [\ref spatial_audio_experimental "Experimental"]  When set to false (default), the intersection of the geometry instance with any portal bounding box is subtracted from the geometry. In effect, an opening is created at the portal location through which sound can pass.
	/// When set to true, portals cannot create openings in the geometry instance. Enable this to allow the geometry instance to be an obstacle to paths going into or through portal bounds.
	///	- \ref AK::SpatialAudio::SetPortal
	///
	bool BypassPortalSubtraction;

	/// A solid geometry instance applies transmission loss once for each time a transmission path enters and exits its volume, using the max transmission loss between each hit surface. A non-solid geometry instance is one where each surface is infinitely thin, applying transmission loss at each surface. This option has no effect if the Transmission Operation is set to Max.
	///
	bool IsSolid;
};

#pragma pack(pop)
