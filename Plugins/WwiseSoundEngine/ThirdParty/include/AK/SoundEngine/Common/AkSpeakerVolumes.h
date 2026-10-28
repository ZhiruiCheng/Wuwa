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

// AkSpeakerVolumes.h

/// \file 
/// Multi-channel volume definitions and services.
/// Always associated with an AkChannelConfig. In the case of standard configurations, the volume items ordering
/// match the bit ordering in the channel mask, except for the LFE which is skipped and placed at the end of the
/// volume array.
/// See \ref goingfurther_speakermatrixcallback for an example of how to manipulate speaker volume vectors/matrices.

#ifndef _AK_SPEAKER_VOLUMES_H_
#define _AK_SPEAKER_VOLUMES_H_

#include <string.h>
#include <AK/SoundEngine/Common/AkTypes.h>

#ifdef __cplusplus

#include <AK/SoundEngine/Platforms/Generic/AkSpeakerVolumes.h>

extern "C" {
#endif

/// Copy volumes.
/// 
/// \param[out] out_pVolumesDst Pointer to destination volume vector.
/// \param[in] in_pVolumesSrc Pointer to source volume vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Copy)(AkSpeakerVolumesVectorPtr out_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels);

/// Copy volumes with gain.
/// 
/// \param[out] out_pVolumesDst Pointer to destination volume vector.
/// \param[in] in_pVolumesSrc Pointer to source volume vector.
/// \param[in] in_uNumChannels Number of channels.
/// \param[in] in_fGain Gain to apply.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_CopyAndApplyGain)(AkSpeakerVolumesVectorPtr out_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels, AkReal32 in_fGain);

/// Clear volumes.
/// 
/// \param[out] out_pVolumesDst Pointer to volume vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Zero)(AkSpeakerVolumesVectorPtr out_pVolumesDst, AkUInt32 in_uNumChannels);

/// Accumulate two volume vectors.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination vector.
/// \param[in] in_pVolumesSrc Pointer to second vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Add)(AkSpeakerVolumesVectorPtr io_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels);

/// Compute the sum of all components of a volume vector.
/// 
/// \param[in] in_pVolumes Pointer to volume vector.
/// \param[in] in_uNumChannels Number of channels.
/// \return The sum of all volumes.
AK_EXTERNAPIFUNC(AkReal32, AK_SpeakerVolumes_Vector_L1Norm)(AkSpeakerVolumesConstVectorPtr in_pVolumes, AkUInt32 in_uNumChannels);

/// Multiply volume vector with a scalar.
/// 
/// \param[in,out] io_pVolumesDst Pointer to volume vector.
/// \param[in] in_fVol Volume multiplier.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_MulScalar)(AkSpeakerVolumesVectorPtr io_pVolumesDst, AkReal32 in_fVol, AkUInt32 in_uNumChannels);

/// Multiply two volume vectors.
///
/// \param[in,out] io_pVolumesDst Pointer to first and destination vector.
/// \param[in] in_pVolumesSrc Pointer to second vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Mul)(AkSpeakerVolumesVectorPtr io_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels);

/// Get max for all elements of two volume vectors, independently.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination vector.
/// \param[in] in_pVolumesSrc Pointer to second vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Max)(AkSpeakerVolumesVectorPtr io_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels);

/// Get min for all elements of two volume vectors, independently.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination vector.
/// \param[in] in_pVolumesSrc Pointer to second vector.
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Vector_Min)(AkSpeakerVolumesVectorPtr io_pVolumesDst, AkSpeakerVolumesConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels);

/// Compute size (in number of elements/floats) required for given number of channels in vector.
/// 
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(AkUInt32, AK_SpeakerVolumes_Vector_GetNumElements)(AkUInt32 in_uNumChannels);

/// Compute size (in bytes) required for given number of channels in vector.
/// 
/// \param[in] in_uNumChannels Number of channels.
AK_EXTERNAPIFUNC(AkUInt32, AK_SpeakerVolumes_Vector_GetRequiredSize)(AkUInt32 in_uNumChannels);

/// Compute matrix size required for given channel configurations.
/// 
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \return Matrix size in byte.
AK_EXTERNAPIFUNC(AkUInt32, AK_SpeakerVolumes_Matrix_GetRequiredSize)(AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Compute number of elements in matrix required for given channel configurations.
/// 
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \return Number of matrix elements.
AK_EXTERNAPIFUNC(AkUInt32, AK_SpeakerVolumes_Matrix_GetNumElements)(AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Get pointer to volume distribution for input channel in_uIdxChannelIn.
/// 
/// \param[in] in_pVolumeMx Pointer to volume matrix.
/// \param[in] in_uIdxChannelIn Index of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \return Pointer to volume distribution vector.
AK_EXTERNAPIFUNC(AkSpeakerVolumesVectorPtr, AK_SpeakerVolumes_Matrix_GetChannel)(AkSpeakerVolumesMatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut);

/// Get pointer to volume distribution for input channel in_uIdxChannelIn.
/// 
/// \param[in] in_pVolumeMx Pointer to volume matrix.
/// \param[in] in_uIdxChannelIn Index of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \return Pointer to volume distribution vector.
AK_EXTERNAPIFUNC(AkSpeakerVolumesConstVectorPtr, AK_SpeakerVolumes_Matrix_GetChannel_Const)(AkSpeakerVolumesConstMatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut);

/// Copy matrix.
/// 
/// \param[out] out_pVolumesDst Pointer to destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to source volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_Copy)(AkSpeakerVolumesMatrixPtr out_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Copy matrix with gain.
/// 
/// \param[out] out_pVolumesDst Pointer to destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to source volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \param[in] in_fGain Gain to apply.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_CopyAndApplyGain)(AkSpeakerVolumesMatrixPtr out_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkReal32 in_fGain);

/// Clear matrix.
/// 
/// \param[out] out_pVolumesDst Pointer to destination volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_Zero)(AkSpeakerVolumesMatrixPtr out_pVolumesDst, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Multiply a matrix with a scalar.
/// 
/// \param[in,out] io_pVolumesDst Pointer to volume matrix.
/// \param[in] in_fVol Volume multiplier.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_Mul)(AkSpeakerVolumesMatrixPtr io_pVolumesDst, AkReal32 in_fVol, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Add all elements of two volume matrices, independently.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to second volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_Add)(AkSpeakerVolumesMatrixPtr io_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Pointwise Multiply-Add of all elements of two volume matrices.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to second volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
/// \param[in] in_fGain Volume multiplier.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_MAdd)(AkSpeakerVolumesMatrixPtr io_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkReal32 in_fGain);

/// Get absolute max for all elements of two volume matrices, independently.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to second volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_AbsMax)(AkSpeakerVolumesMatrixPtr io_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

/// Get max for all elements of two volume matrices, independently.
/// 
/// \param[in,out] io_pVolumesDst Pointer to first and destination volume matrix.
/// \param[in] in_pVolumesSrc Pointer to second volume matrix.
/// \param[in] in_uNumChannelsIn Number of input channels.
/// \param[in] in_uNumChannelsOut Number of output channels.
AK_EXTERNAPIFUNC(void, AK_SpeakerVolumes_Matrix_Max)(AkSpeakerVolumesMatrixPtr io_pVolumesDst, AkSpeakerVolumesConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut);

#ifdef __cplusplus
} // extern "C"

namespace AK
{
/// Multi-channel volume definitions and services.
namespace SpeakerVolumes
{
	/// Volume vector services.
	namespace Vector
	{
		AkForceInline void Copy(VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels)
		{
			if (in_uNumChannels)
				memcpy(in_pVolumesDst, in_pVolumesSrc, in_uNumChannels * sizeof(AkReal32));
		}

		AkForceInline void Copy(VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels, AkReal32 in_fGain)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] = in_pVolumesSrc[uChan] * in_fGain;
			}
		}

		AkForceInline void Zero(VectorPtr in_pVolumes, AkUInt32 in_uNumChannels)
		{
			if (in_uNumChannels)
				memset(in_pVolumes, 0, in_uNumChannels * sizeof(AkReal32));
		}

		AkForceInline void Add(VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan];
			}
		}

		AkForceInline AkReal32 L1Norm(ConstVectorPtr io_pVolumes, AkUInt32 in_uNumChannels)
		{
			AkReal32 total = 0;
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				total += io_pVolumes[uChan];
			}

			return total;
		}

		AkForceInline void Mul(VectorPtr in_pVolumesDst, const AkReal32 in_fVol, AkUInt32 in_uNumChannels)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] *= in_fVol;
			}
		}

		AkForceInline void Mul(VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] *= in_pVolumesSrc[uChan];
			}
		}

		AkForceInline void Max(AkReal32* in_pVolumesDst, const AkReal32* in_pVolumesSrc, AkUInt32 in_uNumChannels)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] = AkMax(in_pVolumesDst[uChan], in_pVolumesSrc[uChan]);
			}
		}

		AkForceInline void Min(AkReal32* in_pVolumesDst, const AkReal32* in_pVolumesSrc, AkUInt32 in_uNumChannels)
		{
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				in_pVolumesDst[uChan] = AkMin(in_pVolumesDst[uChan], in_pVolumesSrc[uChan]);
			}
		}
	}

	/// Volume matrix (multi-in/multi-out channel configurations) services.
	namespace Matrix
	{
		AkForceInline AkUInt32 GetRequiredSize(AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			return in_uNumChannelsIn * Vector::GetRequiredSize(in_uNumChannelsOut);
		}

		AkForceInline AkUInt32 GetNumElements(AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			return in_uNumChannelsIn * Vector::GetNumElements(in_uNumChannelsOut);
		}

		AkForceInline VectorPtr GetChannel(MatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut)
		{
			return in_pVolumeMx + in_uIdxChannelIn * Vector::GetNumElements(in_uNumChannelsOut);
		}

		AkForceInline ConstVectorPtr GetChannel(ConstMatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut)
		{
			return in_pVolumeMx + in_uIdxChannelIn * Vector::GetNumElements(in_uNumChannelsOut);
		}

		AkForceInline void Copy(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			if (uNumElements)
				memcpy(in_pVolumesDst, in_pVolumesSrc, uNumElements * sizeof(AkReal32));
		}

		AkForceInline void Copy(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkReal32 in_fGain)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] = in_pVolumesSrc[uChan] * in_fGain;
			}
		}

		AkForceInline void Zero(MatrixPtr in_pVolumes, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			if (uNumElements)
				memset(in_pVolumes, 0, uNumElements * sizeof(AkReal32));
		}

		AkForceInline void Mul(MatrixPtr in_pVolumesDst, const AkReal32 in_fVol, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] *= in_fVol;
			}
		}

		AkForceInline void Add(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan];
			}
		}

		AkForceInline void MAdd(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkReal32 in_fGain)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan] * in_fGain;
			}
		}

		AkForceInline void AbsMax(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] = ((in_pVolumesDst[uChan] * in_pVolumesDst[uChan]) > (in_pVolumesSrc[uChan] * in_pVolumesSrc[uChan])) ? in_pVolumesDst[uChan] : in_pVolumesSrc[uChan];
			}
		}

		AkForceInline void Max(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] = (in_pVolumesDst[uChan] > in_pVolumesSrc[uChan]) ? in_pVolumesDst[uChan] : in_pVolumesSrc[uChan];
			}
		}
	}
}
}
#endif // __cplusplus

#endif  //_AK_SPEAKER_VOLUMES_H_
