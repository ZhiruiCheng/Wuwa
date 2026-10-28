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

#pragma once

#include <AK/SoundEngine/Common/AkTypedefs.h>
#include <AK/SoundEngine/Common/AkEnums.h>

/// 3D 64-bit vector. Intended as storage for world positions of sounds and objects, benefiting from 64-bit precision range
struct AkVector64
{
#ifdef __cplusplus
	inline AkVector64 operator+(const AkVector64& b) const
	{
		AkVector64 v;

		v.X = X + b.X;
		v.Y = Y + b.Y;
		v.Z = Z + b.Z;

		return v;
	}

	inline AkVector64 operator-(const AkVector64& b) const
	{
		AkVector64 v;

		v.X = X - b.X;
		v.Y = Y - b.Y;
		v.Z = Z - b.Z;

		return v;
	}

	inline void Zero()
	{
		X = 0; Y = 0; Z = 0;
	}
#endif

	AkReal64 X;	///< X Position
	AkReal64 Y;	///< Y Position
	AkReal64 Z;	///< Z Position
};

/// 3D vector for some operations in 3D space. Typically intended only for localized calculations due to 32-bit precision
struct AkVector
{
#ifdef __cplusplus

	inline AkVector operator+(const AkVector& b) const
	{
		AkVector v;

		v.X = X + b.X;
		v.Y = Y + b.Y;
		v.Z = Z + b.Z;

		return v;
	}

	inline AkVector operator-(const AkVector& b) const
	{
		AkVector v;

		v.X = X - b.X;
		v.Y = Y - b.Y;
		v.Z = Z - b.Z;

		return v;
	}

	inline AkVector operator*(const AkReal32 f) const
	{
		AkVector v;

		v.X = X * f;
		v.Y = Y * f;
		v.Z = Z * f;

		return v;
	}

	inline AkVector operator/(const AkReal32 f) const
	{
		AkVector v;

		v.X = X / f;
		v.Y = Y / f;
		v.Z = Z / f;

		return v;
	}

	inline void Zero()
	{
		X = 0; Y = 0; Z = 0;
	}

	// Helper for implicit conversion to AkVector64. ConvertAkVectorToAkVector64 is still preferable to make it more obvious where upconversion occurs.
	inline operator AkVector64() const { return AkVector64{ X, Y, Z }; }
#endif

	AkReal32 X;	///< X Position
	AkReal32 Y;	///< Y Position
	AkReal32 Z;	///< Z Position
};

/// Position and orientation of game objects in the world (i.e. supports 64-bit-precision position)
struct AkWorldTransform
{
#ifdef __cplusplus
public:
	//
	// Getters.
	//

	/// Get position vector.
	inline const AkVector64 & Position() const
	{
		return *reinterpret_cast<const AkVector64*>(&position);
	}

	/// Get orientation front vector.
	inline const AkVector & OrientationFront() const
	{
		return *reinterpret_cast<const AkVector*>(&orientationFront);
	}

	/// Get orientation top vector.
	inline const AkVector & OrientationTop() const
	{
		return *reinterpret_cast<const AkVector*>(&orientationTop);
	}

	//
	// Setters.
	//

	/// Set position and orientation. Orientation front and top should be orthogonal and normalized.
	inline void Set(
		const AkVector64 & in_position,			///< Position vector.
		const AkVector & in_orientationFront,	///< Orientation front
		const AkVector & in_orientationTop		///< Orientation top
		)
	{
		position = in_position;
		orientationFront = in_orientationFront;
		orientationTop = in_orientationTop;
	}

	/// Set position and orientation. Orientation front and top should be orthogonal and normalized.
	inline void Set(
		AkReal64 in_positionX,					///< Position x
		AkReal64 in_positionY,					///< Position y
		AkReal64 in_positionZ,					///< Position z
		AkReal32 in_orientFrontX,				///< Orientation front x
		AkReal32 in_orientFrontY,				///< Orientation front y
		AkReal32 in_orientFrontZ,				///< Orientation front z
		AkReal32 in_orientTopX,					///< Orientation top x
		AkReal32 in_orientTopY,					///< Orientation top y
		AkReal32 in_orientTopZ					///< Orientation top z
		)
	{
		position.X = in_positionX;
		position.Y = in_positionY;
		position.Z = in_positionZ;
		orientationFront.X = in_orientFrontX;
		orientationFront.Y = in_orientFrontY;
		orientationFront.Z = in_orientFrontZ;
		orientationTop.X = in_orientTopX;
		orientationTop.Y = in_orientTopY;
		orientationTop.Z = in_orientTopZ;
	}
	
	/// Set position.
	inline void SetPosition(
		const AkVector64 & in_position			///< Position vector.
		)
	{
		position = in_position;
	}

	/// Set position.
	inline void SetPosition(
		AkReal64 in_x,							///< x
		AkReal64 in_y,							///< y
		AkReal64 in_z							///< z
		)
	{
		position.X = in_x;
		position.Y = in_y;
		position.Z = in_z;
	}

	/// Set orientation. Orientation front and top should be orthogonal and normalized.
	inline void SetOrientation(
		const AkVector & in_orientationFront,	///< Orientation front
		const AkVector & in_orientationTop		///< Orientation top
		)
	{
		orientationFront = in_orientationFront;	
		orientationTop = in_orientationTop;
	}	

	/// Set orientation. Orientation front and top should be orthogonal and normalized.
	inline void SetOrientation(
		AkReal32 in_orientFrontX,				///< Orientation front x
		AkReal32 in_orientFrontY,				///< Orientation front y
		AkReal32 in_orientFrontZ,				///< Orientation front z
		AkReal32 in_orientTopX,					///< Orientation top x
		AkReal32 in_orientTopY,					///< Orientation top y
		AkReal32 in_orientTopZ					///< Orientation top z
		)
	{
		orientationFront.X = in_orientFrontX;
		orientationFront.Y = in_orientFrontY;
		orientationFront.Z = in_orientFrontZ;
		orientationTop.X = in_orientTopX;
		orientationTop.Y = in_orientTopY;
		orientationTop.Z = in_orientTopZ;
	}
#endif

	struct AkVector    orientationFront;  ///< Orientation of the listener
	struct AkVector    orientationTop;    ///< Top orientation of the listener
	struct AkVector64  position;          ///< Position of the listener
};

/// Position and orientation of objects in a "local" space
struct AkTransform
{
#ifdef __cplusplus
public:
	//
	// Getters.
	//

	/// Get position vector.
	inline const AkVector& Position() const
	{
		return position;
	}

	/// Get orientation front vector.
	inline const AkVector& OrientationFront() const
	{
		return orientationFront;
	}

	/// Get orientation top vector.
	inline const AkVector& OrientationTop() const
	{
		return orientationTop;
	}

	//
	// Setters.
	//

	/// Set position and orientation. Orientation front and top should be orthogonal and normalized.
	inline void Set(
		const AkVector& in_position,			///< Position vector.
		const AkVector& in_orientationFront,	///< Orientation front
		const AkVector& in_orientationTop		///< Orientation top
	)
	{
		position = in_position;
		orientationFront = in_orientationFront;
		orientationTop = in_orientationTop;
	}

	/// Set position and orientation. Orientation front and top should be orthogonal and normalized.
	inline void Set(
		AkReal32 in_positionX,					///< Position x
		AkReal32 in_positionY,					///< Position y
		AkReal32 in_positionZ,					///< Position z
		AkReal32 in_orientFrontX,				///< Orientation front x
		AkReal32 in_orientFrontY,				///< Orientation front y
		AkReal32 in_orientFrontZ,				///< Orientation front z
		AkReal32 in_orientTopX,					///< Orientation top x
		AkReal32 in_orientTopY,					///< Orientation top y
		AkReal32 in_orientTopZ					///< Orientation top z
	)
	{
		position.X = in_positionX;
		position.Y = in_positionY;
		position.Z = in_positionZ;
		orientationFront.X = in_orientFrontX;
		orientationFront.Y = in_orientFrontY;
		orientationFront.Z = in_orientFrontZ;
		orientationTop.X = in_orientTopX;
		orientationTop.Y = in_orientTopY;
		orientationTop.Z = in_orientTopZ;
	}

	/// Set position.
	inline void SetPosition(
		const AkVector& in_position			///< Position vector.
	)
	{
		position = in_position;
	}

	/// Set position.
	inline void SetPosition(
		AkReal32 in_x,							///< x
		AkReal32 in_y,							///< y
		AkReal32 in_z							///< z
	)
	{
		position.X = in_x;
		position.Y = in_y;
		position.Z = in_z;
	}

	/// Set orientation. Orientation front and top should be orthogonal and normalized.
	inline void SetOrientation(
		const AkVector& in_orientationFront,	///< Orientation front
		const AkVector& in_orientationTop		///< Orientation top
	)
	{
		orientationFront = in_orientationFront;
		orientationTop = in_orientationTop;
	}

	/// Set orientation. Orientation front and top should be orthogonal and normalized.
	inline void SetOrientation(
		AkReal32 in_orientFrontX,				///< Orientation front x
		AkReal32 in_orientFrontY,				///< Orientation front y
		AkReal32 in_orientFrontZ,				///< Orientation front z
		AkReal32 in_orientTopX,					///< Orientation top x
		AkReal32 in_orientTopY,					///< Orientation top y
		AkReal32 in_orientTopZ					///< Orientation top z
	)
	{
		orientationFront.X = in_orientFrontX;
		orientationFront.Y = in_orientFrontY;
		orientationFront.Z = in_orientFrontZ;
		orientationTop.X = in_orientTopX;
		orientationTop.Y = in_orientTopY;
		orientationTop.Z = in_orientTopZ;
	}

	// Helper for implicit conversion to AkWorldTransform. ConvertAkTransformToAkWorldTransform is still preferable to make it more obvious where upconversion occurs.
	inline operator AkWorldTransform() const {
		AkWorldTransform ret;
		ret.Set(position, orientationFront, orientationTop);
		return ret;
	}

#endif

	struct AkVector orientationFront;   ///< Orientation of the listener
	struct AkVector orientationTop;     ///< Top orientation of the listener
	struct AkVector position;           ///< Position of the listener
};

/// Positioning information for a sound.
typedef struct AkWorldTransform AkSoundPosition;

/// Positioning information for a listener.
typedef struct AkWorldTransform AkListenerPosition;

/// Positioning information for a sound, with specified subset of its channels.
struct AkChannelEmitter
{
	struct AkWorldTransform position;         ///< Emitter position.
	AkChannelMask           uInputChannels;   ///< Channels to which the above position applies.
};

/// Polar coordinates.
struct AkPolarCoord
{
	AkReal32 r;     ///< Norm/distance
	AkReal32 theta; ///< Azimuth
};

#ifdef __cplusplus
namespace AK
{
	// Helper functions to make transitions between 64b and 32b conversion easier

	// Warning: this conversion incurs a loss of precision and range
	inline AkVector ConvertAkVector64ToAkVector(AkVector64 in)
	{
		AkVector out;
		out.X = (AkReal32)in.X;
		out.Y = (AkReal32)in.Y;
		out.Z = (AkReal32)in.Z;
		return out;
	}

	// Warning: this conversion incurs a loss of precision and range in position data
	inline AkTransform ConvertAkWorldTransformToAkTransform(AkWorldTransform in)
	{
		AkTransform out;
		AkVector pos = ConvertAkVector64ToAkVector(in.Position());
		out.Set(pos, in.OrientationFront(), in.OrientationTop());
		return out;
	}

	inline AkVector64 ConvertAkVectorToAkVector64(AkVector in)
	{
		AkVector64 out;
		out.X = (AkReal64)in.X;
		out.Y = (AkReal64)in.Y;
		out.Z = (AkReal64)in.Z;
		return out;
	}

	inline AkWorldTransform ConvertAkTransformToAkWorldTransform(AkTransform in)
	{
		AkWorldTransform out;
		AkVector64 pos = ConvertAkVectorToAkVector64(in.Position());
		out.Set(pos, in.OrientationFront(), in.OrientationTop());
		return out;
	}

	// \deprecated Use AkMultiPositionType.
	using MultiPositionType = AkMultiPositionType;
	const AkMultiPositionType MultiPositionType_SingleSource = AkMultiPositionType_SingleSource;
	const AkMultiPositionType MultiPositionType_MultiSources = AkMultiPositionType_MultiSources;
	const AkMultiPositionType MultiPositionType_MultiDirections = AkMultiPositionType_MultiDirections;
	const AkMultiPositionType MultiPositionType_Last = AkMultiPositionType_Last;
}

/// Spherical coordinates.
struct AkSphericalCoord : public AkPolarCoord
{
	AkReal32		phi;			///< Elevation
};

/// Emitter-listener pair: Positioning data pertaining to a single pair of emitter and listener.
class AkEmitterListenerPair
{
public:
	/// Constructor.
	AkEmitterListenerPair()
		: fDistance(0.f)
		, fEmitterAngle(0.f)
		, fListenerAngle(0.f)	
		, fDryMixGain(1.f)
		, fGameDefAuxMixGain(1.f)
		, fUserDefAuxMixGain(1.f)
		, fOcclusion(0.f)
		, fObstruction(0.f)
		, fDiffraction(0.f)
		, fTransmissionLoss(0.f)
		, fSpread(0.f)
		, fAperture(100.f)
		, fScalingFactor(1.f)
		, fPathGain(1.f)
		, uEmitterChannelMask(0xFFFFFFFF)
		, id(0)
		, m_uListenerID(0)
	{
	}
	/// Destructor.
	~AkEmitterListenerPair() {}

	/// Get distance.
	inline AkReal32 Distance() const { return fDistance; }

	/// Get the absolute angle, in radians between 0 and pi, of the emitter's orientation relative to 
	/// the line that joins the emitter and the listener.
	inline AkReal32 EmitterAngle() const { return fEmitterAngle; }

	/// Get the absolute angle, in radians between 0 and pi, of the listener's orientation relative to
	/// the line that joins the emitter and the listener
	inline AkReal32 ListenerAngle() const { return fListenerAngle; }

	/// Get the occlusion factor for this emitter-listener pair
	inline AkReal32 Occlusion() const { return fOcclusion; }

	/// Get the obstruction factor for this emitter-listener pair
	inline AkReal32 Obstruction() const { return fObstruction; }	

	/// Get the diffraction factor for this emitter-listener pair
	inline AkReal32 Diffraction() const { return fDiffraction; }

	/// Get the transmission loss factor for this emitter-listener pair
	inline AkReal32 TransmissionLoss() const { return fTransmissionLoss; }

	/// Get the overall path-contribution gain, used to scale the dry + gamedef + userdef gains
	inline AkReal32 PathGain() const { return fPathGain; }

	/// Get the emitter-listener-pair-specific gain (due to distance and cone attenuation), linear [0,1], for a given connection type.
	inline AkReal32 GetGainForConnectionType(AkConnectionType in_eType) const
	{
		if (in_eType == ConnectionType_Direct)
			return fDryMixGain;
		else if (in_eType == ConnectionType_GameDefSend)
			return fGameDefAuxMixGain;
		else if (in_eType == ConnectionType_UserDefSend)
			return fUserDefAuxMixGain;
		else
			return 1.0f;
	}

	/// Get the emitter-listener pair's ID.
	inline AkRayID ID() const { return id; }

	/// Get listener ID associated with the emitter-listener pair.
	inline AkGameObjectID ListenerID() const { return m_uListenerID; }
	
	AkWorldTransform emitter;			///< Emitter position.
	AkReal32 fDistance;					///< Distance between emitter and listener.
	AkReal32 fEmitterAngle;				///< Angle between position vector and emitter orientation.
	AkReal32 fListenerAngle;			///< Angle between position vector and listener orientation.	
	AkReal32 fDryMixGain;				///< Emitter-listener-pair-specific gain (due to distance and cone attenuation) for direct connections.
	AkReal32 fGameDefAuxMixGain;		///< Emitter-listener-pair-specific gain (due to distance and cone attenuation) for Automatic Room Send connections.
	AkReal32 fUserDefAuxMixGain;		///< Emitter-listener-pair-specific gain (due to distance and cone attenuation) for Automatic Room Send connections.
	AkReal32 fOcclusion;				///< Emitter-listener-pair-specific occlusion factor
	AkReal32 fObstruction;				///< Emitter-listener-pair-specific obstruction factor
	AkReal32 fDiffraction;				///< Emitter-listener-pair-specific diffraction coefficient
	AkReal32 fTransmissionLoss;			///< Emitter-listener-pair-specific transmission occlusion.
	AkReal32 fSpread;					///< Emitter-listener-pair-specific spread
	AkReal32 fAperture;					///< Emitter-listener-pair-specific aperture
	AkReal32 fScalingFactor;			///< Combined scaling factor due to both emitter and listener.
	AkReal32 fPathGain;					///< Emitter-listener-pair-specific overall gain that scales fDryMixGain, fGameDefAuxMixGain and fUserDefAuxMixGain
	AkChannelMask uEmitterChannelMask;	///< Channels of the emitter that apply to this ray.

protected:
	AkRayID id;							///< ID of this emitter-listener pair, unique for a given emitter.
	AkGameObjectID m_uListenerID;		///< Listener game object ID.
};
#endif

/// Listener information.
struct AkListener
{
#ifdef __cplusplus
	AkListener()
		: fScalingFactor( 1.0f )
		, bSpatialized( true )
	{}
#endif
	AkListenerPosition	position;		/// Listener position (see AK::SoundEngine::SetPosition()).
	AkReal32			fScalingFactor;	/// Listener scaling factor (see AK::SoundEngine::SetListenerScalingFactor()).
	bool				bSpatialized;	/// Whether listener is spatialized or not (see AK::SoundEngine::SetListenerSpatialization()).
};
