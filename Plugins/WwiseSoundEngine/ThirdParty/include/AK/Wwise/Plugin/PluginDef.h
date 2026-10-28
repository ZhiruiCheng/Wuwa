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

/**
 * \brief Wwise Authoring Plug-ins - Base plug-in definitions
 * \file AK/Wwise/Plugin/PluginDef.h
 */

#pragma once

#include "PluginHelpers.h"
#include "PluginInstanceTypes.h"

// Everthing in this section needs:
// - to be converted back to plain vanilla C
// - create optional interfaces for classes
// - create compulsory convertion functions for enums
#ifdef __cplusplus
#include "../../SoundEngine/Common/IAkPlugin.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class AkVariantBase;
	}
}

namespace AK
{
	namespace Wwise
	{
		namespace Plugin
		{
			/// License type.
			enum LicenseType
			{
				LicenseType_Trial = 1,		///< Used for both Trial and Evaluation License handling
				LicenseType_Purchased,		///< The license was purchased
				LicenseType_Academic		///< The license is for academic
			};

			/// License status.
			enum LicenseStatus
			{
				LicenseStatus_Unlicensed,	///< No license found
				LicenseStatus_Expired,		///< A license is found, but is expired
				LicenseStatus_Valid,		///< A license is found and is valid

				LicenseStatus_Incompatible	///< The plugin was made for an older version of Wwise
			};

			struct LicenseID
			{
				// nul-terminated 8 characters license
				char id[9];
			};

			/// Type of operation for the NotifyInnerObjectAddedRemoved function.
			enum NotifyInnerObjectOperation
			{
				InnerObjectAdded,
				InnerObjectRemoved
			};

			struct MonitorData
			{
				uint64_t uGameObjectID;
				void* pData;
				unsigned int uDataSize;
			};

			/// Import channel configuration options.
			enum AudioFileChannel
			{
				Channel_mono = 0,
				Channel_stereo = 1,
				Channel_mono_drop = 2,
				Channel_stereo_drop = 3,
				Channel_as_input = 4,
				Channel_mono_drop_right = 5,
				Channel_stereo_balance = 6,
			};

			/// Log message severity.
			enum Severity
			{
				Severity_Success = -1,	///< operation was executed without errors or will not produce errors
				Severity_Message,		///< not impacting the integrity of the current operation
				Severity_Warning,		///< potentially impacting the integrity of the current operation
				Severity_Error,			///< impacting the integrity of the current operation
				Severity_FatalError,	///< impacting the completion of the current operation
			};

			/// Represents the association between a dialog control (such as
			/// a checkbox or radio button) and a plug-in property.
			/// \aknote
			/// You should not need to use this structure directly. Instead, use the 
			/// AK_BEGIN_POPULATE_TABLE(), AK_POP_ITEM(), and AK_END_POPULATE_TABLE() macros.
			/// \endaknote
			/// \sa
			/// - \ref wwiseplugin_dialog_guide_poptable
			struct PopulateTableItem
			{
				uint32_t uiID;				///< The dialog control resource ID
				const char * pszProp;		///< The property name
			};

			/// Dialog type. Source plug-ins can be edited in the Property Editor or
			/// the Contents Editor, while effect plug-ins can only be edited in the
			/// Effect Editor.
			/// \sa
			/// - \ref wwiseplugin_dialogcode
			enum eDialog
			{
				SettingsDialog,			///< Main plug-in dialog. This is the dialog used in the Property
										///< Editor for source plug-ins, and in the Effect Editor for
										///< effect plug-ins.
				ContentsEditorDialog	///< Contents Editor dialog. This is the small dialog used in the
										///< Contents Editor for source plug-ins.
			};

			/// Conversion error code.
			enum ConversionResult
			{
				ConversionSuccess = 0,
				ConversionWarning = 1,
				ConversionFailed = 2,
			};

			class IProgress
			{
			public:
				/// Call this to set the name of the operation currently done.
				/// If not called the operation will have an empty name in the UI.
				/// The name should be on a single line.
				virtual void SetCurrentOperationName(const char * in_szOperationName) = 0;

				/// Should be called at the beginning of the operation to set the min and max value
				virtual void SetRange(uint32_t in_dwMinValue, uint32_t in_dwMaxValue) = 0;

				/// Notify of the advancement of the task.
				virtual void NotifyProgress(uint32_t in_dwProgress) = 0;

				/// Check if the user has cancelled the task
				virtual bool IsCancelled() const = 0;

				/// Display an error message to the user.
				/// The message should be on a single line.
				virtual void ErrorMessage(const char * in_rErrorText, Severity in_eSeverity = Severity_Warning) = 0;
			};

			/// Interface to let the plug in give us a string of any size.
			/// The pointer to the interface should not be kept.
			class IWriteString
			{
			public:
				virtual void WriteString(const char * in_szString,
					int in_iStringLength) = 0;
			};

			/// Interfaces used to set and get the properties from a plug in.
			class IReadOnlyProperties
			{
			public:
				virtual bool GetValue(const char * in_szPropertyName,
					AK::WwiseAuthoringAPI::AkVariantBase& out_rValue) const = 0;

				virtual int GetType(
					const char* in_pszPropertyName		///< The name of the property
				) const = 0;

				virtual bool GetValueString(
					const char * in_pszPropertyName,		///< The name of the property
					const char*& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueInt64(
					const char * in_pszPropertyName,		///< The name of the property
					int64_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueInt32(
					const char * in_pszPropertyName,		///< The name of the property
					int32_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueInt16(
					const char * in_pszPropertyName,		///< The name of the property
					int16_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueInt8(
					const char * in_pszPropertyName,		///< The name of the property
					int8_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueUInt64(
					const char * in_pszPropertyName,		///< The name of the property
					uint64_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueUInt32(
					const char * in_pszPropertyName,		///< The name of the property
					uint32_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueUInt16(
					const char * in_pszPropertyName,		///< The name of the property
					uint16_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueUInt8(
					const char * in_pszPropertyName,		///< The name of the property
					uint8_t& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueReal64(
					const char * in_pszPropertyName,		///< The name of the property
					double& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueReal32(
					const char * in_pszPropertyName,		///< The name of the property
					float& out_varProperty		///< The returned value of the property
				) const = 0;

				virtual bool GetValueBool(
					const char * in_pszPropertyName,		///< The name of the property
					bool& out_varProperty		///< The returned value of the property
				) const = 0;

				inline const char* GetString(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					const char* result;
					AKVERIFY(GetValueString(in_pszPropertyName, result));
					return result;
				}

				inline int64_t GetInt64(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					int64_t result;
					AKVERIFY(GetValueInt64(in_pszPropertyName, result));
					return result;
				}

				inline int32_t GetInt32(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					int32_t result;
					AKVERIFY(GetValueInt32(in_pszPropertyName, result));
					return result;
				}

				inline int16_t GetInt16(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					int16_t result;
					AKVERIFY(GetValueInt16(in_pszPropertyName, result));
					return result;
				}

				inline int8_t GetInt8(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					int8_t result;
					AKVERIFY(GetValueInt8(in_pszPropertyName, result));
					return result;
				}

				inline uint64_t GetUInt64(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					uint64_t result;
					AKVERIFY(GetValueUInt64(in_pszPropertyName, result));
					return result;
				}

				inline uint32_t GetUInt32(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					uint32_t result;
					AKVERIFY(GetValueUInt32(in_pszPropertyName, result));
					return result;
				}

				inline uint16_t GetUInt16(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					uint16_t result;
					AKVERIFY(GetValueUInt16(in_pszPropertyName, result));
					return result;
				}

				inline uint8_t GetUInt8(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					uint8_t result;
					AKVERIFY(GetValueUInt8(in_pszPropertyName, result));
					return result;
				}

				inline double GetReal64(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					double result;
					AKVERIFY(GetValueReal64(in_pszPropertyName, result));
					return result;
				}

				inline float GetReal32(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					float result;
					AKVERIFY(GetValueReal32(in_pszPropertyName, result));
					return result;
				}

				inline bool GetBool(
					const char * in_pszPropertyName		///< The name of the property
				) const
				{
					bool result;
					AKVERIFY(GetValueBool(in_pszPropertyName, result));
					return result;
				}
			};

			class IReadWriteProperties : public IReadOnlyProperties
			{
			public:
				virtual bool SetValue(const char * in_szPropertyName,
					const AK::WwiseAuthoringAPI::AkVariantBase& in_rValue) = 0;

				virtual bool ClearValue(
					const char* in_pszPropertyName		///< The name of the property
				) = 0;

				virtual bool SetValueString(
					const char * in_pszPropertyName,		///< The name of the property
					const char * in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueInt64(
					const char * in_pszPropertyName,		///< The name of the property
					int64_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueInt32(
					const char * in_pszPropertyName,		///< The name of the property
					int32_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueInt16(
					const char * in_pszPropertyName,		///< The name of the property
					int16_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueInt8(
					const char * in_pszPropertyName,		///< The name of the property
					int8_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueUInt64(
					const char * in_pszPropertyName,		///< The name of the property
					uint64_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueUInt32(
					const char * in_pszPropertyName,		///< The name of the property
					uint32_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueUInt16(
					const char * in_pszPropertyName,		///< The name of the property
					uint16_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueUInt8(
					const char * in_pszPropertyName,		///< The name of the property
					uint8_t in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueReal64(
					const char * in_pszPropertyName,		///< The name of the property
					double in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueReal32(
					const char * in_pszPropertyName,		///< The name of the property
					float in_varProperty	///< The value to set
				) = 0;

				virtual bool SetValueBool(
					const char * in_pszPropertyName,		///< The name of the property
					bool in_varProperty	///< The value to set
				) = 0;
			};

			// Note: This should be kept in line with AkChunkHeader
			struct RiffHeader
			{
				uint32_t fccChunkId;
				uint32_t dwDataSize;
			};
			
			struct ConversionContext
			{
				//
				// Inputs
				//
				const GUID* platformID;
				const GUID* basePlatformID;
				const char* sourceFileName;
				const char* destinationFileName;
				AK::Wwise::Plugin::IProgress* progress;
				AK::Wwise::Plugin::IWriteString* error;

				bool noDecode;

				const IReadOnlyProperties* sourceProperties;
				IReadWriteProperties* pluginProperties;

				// Analysis data types available for this source.
				uint32_t analysisDataTypes;

				// False if source context contains at least one value that is not the default.
				bool isDefault;

				// Required downmix normalization gain (linear), 1 if not available / desired.
				float downmixNormalizationGain;
			};

			struct ConvertedFileInfo
			{
				uint32_t dataSize = 0;			///< Actual size of data, taking duration into account (for prefetch)
				uint32_t duration = -1;			///< Actual duration of data, or -1 for entire file
				uint32_t sampleRate = 0;		///< Number of samples per second
				AkChannelConfig channelConfig;	///< Channel configuration
				uint32_t decodedFileSize = -1;	///< File size of file when decoded to PCM format, *If* offline decoding is supported by the codec. Otherwise has value NO_OFFLINE_DECODING (-1)
				uint8_t abyHash[16] = { 0 };	///< Converted file hash (as present in the HASH chunk).
			};

			struct OpenedConvertedFile
				: public ConvertedFileInfo
			{
				const void* data = nullptr;		///< Pointer to start of file data
			};
		}
	}
}
#endif

/** \addtogroup global
 * @{
 */

/**
 * \brief A definition of an undo event, with a specific interface and instance.
 * 
 * Allows to bind an event from any source: Authoring or any plug-in. Allows to have multiple separate interfaces.
 * 
 * - ak_wwise_plugin_undo_event_v1 C interface for an undo event.
 * - ak_wwise_plugin_undo_event_instance_v1 C instance for an undo event.
 * - AK::Wwise::Plugin::V1::UndoEvent C++ class for an undo event.
 * - ak_wwise_plugin_host_undo_manager_v1 C interface for the undo manager.
 * - AK::Wwise::Plugin::V1::UndoManager C++ class for the undo manager.
 */
struct ak_wwise_plugin_undo_event_pair_v1
{
	struct ak_wwise_plugin_undo_event_v1* m_interface;																	///< The interface to execute that undo event's commands.
	struct ak_wwise_plugin_undo_event_instance_v1* m_instance;															///< The specific instance usued for that particular undo event.
};

typedef int ak_wwise_plugin_undo_group_id;																				///< Unique identifier for a particular undo group. Useful to reopen an unapplied closed group session.

/**
 * \brief Action to apply once this undo group is closed.
 * 
 * The goal of an undo group is to keep a list of actions being executed that can be undone (for example, a
 * property set modification). Once the operations are all done, this indicates what should be done with the internal
 * operations.
 * 
 * - \ref AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY : The usual operation is to open an undo group, and then close
 *   and apply the group to the undo stack. This provides a valid Undo/Redo label, as well as a description of all
 *   the operations that need to be done.
 * - \ref AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY_FIRST_EVENT_NAME
 *   and AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY_LAST_EVENT_NAME are convenience operations, where the Undo
 *   operation title is retrieved from either the first or the last event's name, instead of hard-coding it ourselves.
 * - \ref AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_CANCEL cancels the undo operations. It happens when an abnormal
 *   termination occurs, such as an error, or the user cancelling a lengthy operation. Since the operation states
 *   aren't known, you are responsible to handle the stored undo yourself, and manually revert all operations first
 *   before closing the action. An example of this operation is doing a slider drag event, where the user presses
 *   the Escape key instead of doing a Mouse Up. Effectively, all the inner operations will simply be cancelled by storing
 *   the initial value before doing the cancel operation on close.
 * - \ref AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_CLOSE temporarily closes the undo state, leaving it possible to reopen
 *   it later. This is useful during lengthy processes, such as a user dragging a slider. You are expected to
 *   temporarily close the undo group when going out of scope, and reopening it when receiving a new system mouse drag
 *   event in this particular example.
 */
typedef enum {
	AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_CLOSE,																		///< Close this group session (going out of scope), but do not apply it yet.
	AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY,																		///< Close this group session permanently, and apply its operations to the englobing group.
	AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY_FIRST_EVENT_NAME,														///< Same than AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY, but copies the name of the first inner undo event as the name.
	AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY_LAST_EVENT_NAME,														///< Same than AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_APPLY, but copies the name of the last inner undo event as the name.
	AK_WWISE_PLUGIN_UNDO_GROUP_CLOSE_ACTION_CANCEL																		///< Close this group session permanently, and cancel all its internal operations. Undo operations are not applied.
} ak_wwise_plugin_undo_group_close_action;

/**
 * \brief Bitfield values of brace types, used to delay or avoid certain actions normally triggered as a result
 * of a property set mutation.
 *
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_NOTIFY : Do not notify of changes
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_UNDO_EVENTS : Do not create undo events
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_SET_VALUE : Collapse set values notifications once the brace is closed
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_REORDER_CHILDREN : Children are being reordered
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_DISABLE_PROPERTY_CONSTRAINTS : Disable property constraints
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_DIRTY : Inhibit the project's dirty state
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_CREATE_PROPERTY_ON_SET_VALUE : Create a new property when an unknown property is set
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_CHANGE_TO_EXISTING_VALUE_TYPE : Allow a property type change if the provided type differs from its current type
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_LOADING : Indicates the object is being loaded
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_UNLOADING : Indicates the object is being unloaded
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_DELETING : Indicates the object is being deleted
 * - \ref AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_SET_OBJECT_LIST : Collapse the final set object list once the last brace is closed
 *
 * \sa
 * - AK::Wwise::Plugin::PropertySet::OpenBraces
 * - AK::Wwise::Plugin::PropertySet::CloseBraces
*/
typedef enum
{
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_NOTIFY                     = 1 << 0,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_UNDO_EVENTS                = 1 << 1,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_SET_VALUE                     = 1 << 2,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_REORDER_CHILDREN              = 1 << 3,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_DISABLE_PROPERTY_CONSTRAINTS  = 1 << 4,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_NO_DIRTY                      = 1 << 5,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_CREATE_PROPERTY_ON_SET_VALUE  = 1 << 6,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_CHANGE_TO_EXISTING_VALUE_TYPE = 1 << 7,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_LOADING                       = 1 << 8,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_UNLOADING                     = 1 << 9,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_DELETING                      = 1 << 10,
	AK_WWISE_PLUGIN_PROPERTY_SET_BRACES_SET_OBJECT_LIST               = 1 << 11,
} ak_wwise_plugin_property_set_braces_values;

/// \brief Bitfield composed of values defined in \c ak_wwise_plugin_property_set_braces_values
typedef uint32_t ak_wwise_plugin_property_set_braces;

/**
 * @}
 */
