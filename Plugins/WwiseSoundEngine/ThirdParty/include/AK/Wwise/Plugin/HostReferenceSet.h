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
 * \brief Wwise Authoring Plug-ins - Plug-in API for reference sets.
 * \file AK/Wwise/Plugin/HostReferenceSet.h
 */

#pragma once

#include "PluginInfoGenerator.h"

/**
 * \brief Interface used to interact with reference sets.
 * 
 * A reference set is a dictionary of references to other Objects that exist in a user's Authoring
 * project. Whenever a reference name is specified, it corresponds to the reference name set in
 * the plug-in's XML. These Object references can be resolved to GUIDs for use in the 
 * Authoring project, or to Short IDs for use in the Wwise Runtime.
 * 
 * The methods in this interface which use in_guidPlatform as an input parameter assume that you have
 * access to a Platform defined as a GUID, either provided by the caller function or retrieved through
 * the Host interface.
 * 
 * You can retrieve GUIDs in the following ways:
 * 
 * - Use the in_guidPlatform provided as an input parameter in \ref ak_wwise_plugin_audio_plugin_v1::GetBankParameters
 *   or \ref ak_wwise_plugin_custom_data_v1::GetPluginData
 * - Poll the currently-active platform from \ref ak_wwise_plugin_host_v1::GetCurrentPlatform or
 *   \ref ak_wwise_plugin_host_v1::GetAuthoringPlaybackPlatform
 * 
 * You can also provide GUID_NULL as a parameter, which accesses data for all platforms at once 
 * as a linked value. However, GUID_NULL only works when no platform-specific data is possible
 * for a value. Using the current platform is always the preferred access method.
 * 
 * \sa
 * - \ref wwiseplugin_xml_properties_tag
 * - \ref wwiseplugin_referenceset 
 * - \ref ak_wwise_plugin_notifications_reference_set_v1
 * - \ref ak_wwise_plugin_host_object_store_v1
 */
struct ak_wwise_plugin_host_reference_set_v1
#ifdef __cplusplus
	: public ak_wwise_plugin_base_interface
#endif
{
#ifndef __cplusplus
	ak_wwise_plugin_base_interface m_baseInterface;
#endif

#ifdef __cplusplus
	/// Base host-provided instance type for ak_wwise_plugin_host_reference_set_v1.
	using Instance = ak_wwise_plugin_host_reference_set_instance_v1;
	ak_wwise_plugin_host_reference_set_v1(int version = 1) :
		ak_wwise_plugin_base_interface(AK_WWISE_PLUGIN_INTERFACE_TYPE_HOST_REFERENCE_SET, version)
	{}
#endif

	/**
	 * \brief Retrieves the value of a specific reference as a GUID.
	 * \sa
	 * - \ref wwiseplugin_bank
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the queried platform.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return The returned GUID for the referenced object. If no reference is set, GUID_NULL is returned.
	 */
	GUID(*GetReferenceGuid)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char * in_pszReferenceName
	);

	/**
	 * \brief Returns the Unique ID (or "ShortID") of the corresponding object.
	 *
	 * For the plug-in reference set, this is equivalent to the ID field in the project files.
	 * This Unique ID can be used in the runtime to reference other objects through sound engine APIs,
	 * such as RTPCs, Sidechain Mix sharesets, or Events.
	 *
	 * \aknote
	 *		The ID might change on Undo or Redo operations and should not be cached.
	 *		Typically, this should only be used when serializing bank data for a plug-in.
	 * \endaknote
	 *
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the queried platform.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return Unique ID of the corresponding object. Returns AK_INVALID_UNIQUE_ID if no reference set.
	 */
	AkUniqueID (*GetReferenceShortId)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char* in_pszReferenceName
	);

	/**
	 * \brief Updates the value of a specific reference to the provided GUID.
	 * 
	 * Updates the reference at \c in_pszReferenceName to the referenced object by the GUID
	 * under the \c in_guidPlatform platform. The referenced object must be a compatible type
	 * as defined in the plug-in XML definition file.
	 * 
	 * If the GUID is not a valid reference or the referring object is not a valid type,
	 * the reference is set to None.
	 * 
	 * \sa
	 * - \ref wwiseplugin_bank
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the platform whose reference you want to modify.
	 * \param[in] in_pszReferenceName The name of the reference to update, based on the XML definition file.
	 * \param[out] in_guidReference The GUID of the new referenced object.
	 * \return true if the reference was set to a valid object. False if the object is not found or is an incompatible type.
	 */
	bool(*SetReferenceByGuid)(
		struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char * in_pszReferenceName,
		const GUID* in_guidReference
	);

	/**
	 * \brief Returns whether the specified reference has at least some linked platforms.
	 * 
	 * By default, all platforms are linked.
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return true if that particular reference has some linked platforms.
	 */
	bool(*ReferenceHasLinked)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const char * in_pszReferenceName
	);

	/**
	 * \brief Returns whether the specified reference has one or more platforms that are not linked.
	 * 
	 * By default, all platforms are linked.
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return true if that particular reference has one or more unlinked platforms.
	 */
	bool(*ReferenceHasUnlinked)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const char * in_pszReferenceName
	);

	/**
	 * \brief Returns whether the specified reference's platform is linked.
	 * 
	 * By default, all platforms are linked.
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the queried platform.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return true if the specified reference specifically has the specified platform linked.
	 */
	bool(*ReferencePlatformIsLinked)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char * in_pszReferenceName
		);

	/**
	 * \brief Retrieves the name of a referenced object.
	 *
	 * \aknote
	 *      The returned pointer is provided by Wwise Authoring, and contains a TLS copy of the name as a string.
	 *      It is guaranteed to be valid until the next Host interface function call. If you
	 *      wish to retain this string, you should make a copy at the earliest convenience.
	 * \endaknote
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the queried platform.
	 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
	 * \return Null-terminated string of the name of the referenced object. Returns nullptr if no object is referenced.
	 */
	const char* (*GetReferenceName)(
		const struct ak_wwise_plugin_host_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char * in_pszReferenceName
	);
};


struct ak_wwise_plugin_notifications_reference_set_v1
#ifdef __cplusplus
	: public ak_wwise_plugin_base_interface
#endif
{
#ifndef __cplusplus
	ak_wwise_plugin_base_interface m_baseInterface;
#endif

#ifdef __cplusplus
	/// Base instance type for receiving notifications on reference set's changes.
	using Instance = ak_wwise_plugin_notifications_reference_set_instance_v1;

	ak_wwise_plugin_notifications_reference_set_v1() :
		ak_wwise_plugin_base_interface(AK_WWISE_PLUGIN_INTERFACE_TYPE_NOTIFICATIONS_REFERENCE_SET, 1)
	{}
#endif

	/**
	 * \brief This function is called by Wwise when a plug-in reference changes.
	 * 
	 * For example, data can change through interaction with a UI control bound to a reference,
	 * or through undo/redo operations.
	 * 
	 * \sa
	 * - \ref wwiseplugin_referenceset_notification
	 * 
	 * \param[in] in_this Current instance of this interface.
	 * \param[in] in_guidPlatform The unique ID of the queried platform.
	 * \param[in] in_pszReferenceName The name of the affected reference.
	 */
	void(*NotifyReferenceChanged)(
		struct ak_wwise_plugin_notifications_reference_set_instance_v1* in_this,
		const GUID* in_guidPlatform,
		const char * in_pszReferenceName
	);
};


#define AK_WWISE_PLUGIN_HOST_REFERENCE_SET_V1_ID() \
	AK_WWISE_PLUGIN_BASE_INTERFACE_FROM_ID(AK_WWISE_PLUGIN_INTERFACE_TYPE_HOST_REFERENCE_SET, 1)
#define AK_WWISE_PLUGIN_HOST_REFERENCE_SET_V1_CTOR() \
{ \
	.m_baseInterface = AK_WWISE_PLUGIN_HOST_REFERENCE_SET_V1_ID() \
}

#define AK_WWISE_PLUGIN_NOTIFICATIONS_REFERENCE_SET_V1_ID() \
	AK_WWISE_PLUGIN_BASE_INTERFACE_FROM_ID(AK_WWISE_PLUGIN_INTERFACE_TYPE_NOTIFICATIONS_REFERENCE_SET, 1)
#define AK_WWISE_PLUGIN_NOTIFICATIONS_REFERENCE_SET_V1_CTOR(/* ak_wwise_plugin_info* */ in_pluginInfo, /* void* */ in_data) \
{ \
	.m_baseInterface = AK_WWISE_PLUGIN_BASE_INTERFACE_CTOR(AK_WWISE_PLUGIN_NOTIFICATIONS_REFERENCE_SET_V1_ID(), in_pluginInfo, in_data) \
}


#ifdef __cplusplus
namespace AK::Wwise::Plugin
{
	using CHostReferenceSet = ak_wwise_plugin_host_reference_set_v1;

	/// \copydoc ak_wwise_plugin_host_reference_set_v1
	template<typename CHostReferenceSetT = CHostReferenceSet, int interface_version = 1>
	class ReferenceSetBase : public CBaseInstanceGlue<CHostReferenceSetT>
	{
	public:
		using Interface = CHostReferenceSet;

		/**
			* \brief The interface type, as requested by this plug-in.
			*/
		enum : InterfaceTypeValue
		{
			/**
				* \brief The interface type, as requested by this plug-in.
				*/
			k_interfaceType = AK_WWISE_PLUGIN_INTERFACE_TYPE_HOST_REFERENCE_SET
		};
		/**
			* \brief The interface version, as requested by this plug-in.
			*/
		enum : InterfaceVersion
		{
			/**
				* \brief The interface version, as requested by this plug-in.
				*/
			k_interfaceVersion = interface_version
		};

		/**
			* \brief Retrieves the value of a specific reference as a GUID.
			* \sa
			* - \ref wwiseplugin_bank
			*
			* \param[in] in_guidPlatform The unique ID of the queried platform.
			* \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
			* \return The returned GUID for referenced object. If no reference is set, GUID_NULL is returned.
			*/
		inline GUID GetReferenceGuid(
			const GUID& in_guidPlatform,
			const char* in_pszReferenceName
		) const
		{
			return this->g_cinterface->GetReferenceGuid(this, &in_guidPlatform, in_pszReferenceName);
		}

		/**
			* \brief Returns the Unique ID (or "ShortID") of the corresponding object.
			*
			* For the plug-in reference set, this is equivalent to the ID field in the project files.
			* This Unique ID can be used in the runtime to reference other objects through sound engine APIs,
			* such as RTPCs, Sidechain Mix sharesets, or Events.
			*
			* \aknote
			* 		The ID might change on Undo or Redo operations, and should not be cached.
			*		Typically, this should only be used when serializing bank data for a plug-in.
			* \endaknote
			*
			* \param[in] in_guidPlatform The unique ID of the queried platform.
			* \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
			* \return Unique ID of the corresponding object. Returns AK_INVALID_UNIQUE_ID if no reference set.
			*/
		inline AkUniqueID GetReferenceShortId(
			const GUID& in_guidPlatform,
			const char* in_pszReferenceName
		) const
		{
			return this->g_cinterface->GetReferenceShortId(this, &in_guidPlatform, in_pszReferenceName);
		}

		/**
			* \brief Updates the value of a specific reference to the provided GUID.
			*
			* Updates the reference at \c in_pszReferenceName to the referenced object by the GUID
			* under the \c in_guidPlatform platform. The referenced object must be a compatible type
			* as defined in the plug-in XML definition file.
			*
			* If the GUID is not a valid reference, or the referring object is not a valid type,
			* the reference is set to None.
			*
			* \sa
			* - \ref wwiseplugin_bank
			*
			* \param[in] in_guidPlatform The unique ID of the platform whose reference you want to modify.
			* \param[in] in_pszReferenceName The name of the reference to update, based on the XML definition file.
			* \param[out] in_guidReference The GUID of the new referenced object.
			* \return true if the reference was set to a valid object. False if the object is not found, or type is incompatible.
			*/
		inline bool SetReferenceByGuid(
			const GUID& in_guidPlatform,
			const char* in_pszReferenceName,
			const GUID& in_guidReference
		)
		{
			return MKBOOL(this->g_cinterface->SetReferenceByGuid(this, &in_guidPlatform, in_pszReferenceName, &in_guidReference));
		}

		/**
			* \brief Returns whether the specified reference has one or more platforms that are not linked.
			* 
			* By default, all platforms are linked.
			* 
			* \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
			* \return true if that particular reference has some unlinked platforms.
			*/
		inline bool ReferenceHasUnlinked(
			const char* in_pszReferenceName
		) const
		{
			return MKBOOL(this->g_cinterface->ReferenceHasUnlinked(this, in_pszReferenceName));
		}

		/**
			* \brief Returns whether the specified reference's platform is linked.
			* 
			* By default, all platforms are linked.
			* 
			* \param[in] in_guidPlatform The unique ID of the queried platform.
			* \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
			* \return true if the specified reference specifically has the specified platform linked.
			*/
		inline bool ReferencePlatformIsLinked(
			const GUID& in_guidPlatform,
			const char* in_pszReferenceName
		) const
		{
			return MKBOOL(this->g_cinterface->ReferencePlatformIsLinked(this, &in_guidPlatform, in_pszReferenceName));
		}

		/**
		 * \brief Retrieves the name of an object being referenced.
		 *
		 * \aknote
		 *      The returned pointer is provided by Authoring, and contains a TLS copy of the property
		 *      value string. It is guaranteed to be valid until the next Host function call. If you
		 *      wish to retain this string, you should make a copy at the earliest convenience.
		 * \endaknote
		 * 
		 * \param[in] in_guidPlatform The unique ID of the queried platform.
		 * \param[in] in_pszReferenceName The name of the queried reference, based on the XML definition file.
		 * \return Null-terminated UTF-8 string of the name of the referenced object. Returns empty string if no object is referenced.
		 */
		inline const char* GetReferenceName(
			const GUID& in_guidPlatform,
			const char* in_pszReferenceName
		) const
		{
			return this->g_cinterface->GetReferenceName(this, &in_guidPlatform, in_pszReferenceName);
		}

	};

	using ReferenceSet = ReferenceSetBase<>;

	namespace Notifications
	{
		using CReferenceSet_ = ak_wwise_plugin_notifications_reference_set_v1;

		/// \copydoc ak_wwise_plugin_notifications_reference_set_v1
		class ReferenceSet_ : public CReferenceSet_::Instance
		{
		public:
			/**
				* \copydoc ak_wwise_plugin_notifications_reference_set_v1
				*/
			using Instance = CReferenceSet_::Instance;

			/**
				* \brief The interface type, as provided by this plug-in.
				*/
			enum : InterfaceTypeValue
			{
				/**
					* \brief The interface type, as provided by this plug-in.
					*/
				k_interfaceType = AK_WWISE_PLUGIN_INTERFACE_TYPE_NOTIFICATIONS_REFERENCE_SET
			};
			/**
				* \brief The interface version, as provided by this plug-in.
				*/
			enum : InterfaceVersion
			{
				/**
					* \brief The interface version, as provided by this plug-in.
					*/
				k_interfaceVersion = 1
			};

			/**
				* \brief The C interface, fulfilled by your plug-in.
				*/
			struct Interface : public CReferenceSet_
			{
				using Instance = ReferenceSet_;
				Interface()
				{
					CReferenceSet_::NotifyReferenceChanged = [](
						struct ak_wwise_plugin_notifications_reference_set_instance_v1* in_this,
						const GUID* in_guidPlatform,
						const char* in_pszReferenceName)
					{
						static_cast<Instance*>(in_this)->NotifyReferenceChanged(
							*in_guidPlatform,
							in_pszReferenceName);
					};
				}
			};

			InterfacePtr GetInterfacePointer() {
				static Interface g_interface;
				return &g_interface;
			}
			CReferenceSet_::Instance* GetInstancePointer() {
				return this;
			}
			const CReferenceSet_::Instance* GetInstancePointer() const {
				return this;
			}

			ReferenceSet_() :
				CReferenceSet_::Instance()
			{
			}

			virtual ~ReferenceSet_() {}

			/**
				* \brief This function is called by Wwise when a plug-in reference changes.
				*
				* For example, data can change through interaction with a UI control bound to a reference,
				* or through undo/redo operations.
				*
				* This function is not called on the Object Store or any inner reference sets.
				*
				* \sa
				* - \ref wwiseplugin_referenceset_notification
				*
				* \param[in] in_guidPlatform The unique ID of the queried platform.
				* \param[in] in_pszReferenceName The name of the affected reference.
				*/
			virtual void NotifyReferenceChanged(
				const GUID& in_guidPlatform,		///< The unique ID of the queried platform
				const char* in_pszReferenceName		///< The name of the reference
			) {}
		};
	} // of namespace Notifications

	/**
		* \brief Requests a ReferenceSet interface, provided as m_referenceSet variable.
		* 
		* If you derive your plug-in class from RequestReferenceSet, both the ReferenceSet and Notifications::ReferenceSet_ interfaces are requested.
		* You can then derive from the virtual functions as defined in Notifications::ReferenceSet_, and
		* access the host-provided functions in the `m_referenceSet` member variable.
		* 
		*/
	using RequestReferenceSet = RequestedHostInterface<ReferenceSet>;

	namespace Notifications
	{
		/// Latest version of the C ReferenceSet notification interface.
		using CReferenceSet = Notifications::CReferenceSet_;
		/// Latest version of the C++ ReferenceSet notification interface.
		using ReferenceSet = Notifications::ReferenceSet_;
	}

	AK_WWISE_PLUGIN_SPECIALIZE_HOST_INTERFACE(ReferenceSet, referenceSet,, public Notifications::ReferenceSet);
	AK_WWISE_PLUGIN_SPECIALIZE_INTERFACE_CLASS(ReferenceSet);
	AK_WWISE_PLUGIN_SPECIALIZE_INTERFACE_CLASS(Notifications::ReferenceSet);
	AK_WWISE_PLUGIN_SPECIALIZE_INTERFACE_VERSION(Notifications::ReferenceSet);
} // of namespace AK::Wwise::Plugin

#endif
