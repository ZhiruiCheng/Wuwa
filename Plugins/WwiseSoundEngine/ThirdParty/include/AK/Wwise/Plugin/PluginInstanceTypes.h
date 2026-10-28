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
 * \brief Wwise Authoring Plug-in Instance Types
 * \file AK/Wwise/Plugin/PluginInstanceTypes.h
 */

#pragma once

/** \addtogroup global
 * @{
 */

struct ak_wwise_plugin_base_interface;
typedef struct ak_wwise_plugin_base_interface* ak_wwise_plugin_interface_ptr;											///< Pointer to a generic base from a plug-in interface.

/**
 * \brief Generic base for all plug-in instances. In C++, this is derived. In C, they are equivalent.
 * 
 * \sa
 * - \ref ak_wwise_plugin_cpp_base_instance for discrepancies between the C and C++ models.
 */
struct ak_wwise_plugin_base_instance {};
typedef struct ak_wwise_plugin_base_instance* ak_wwise_plugin_instance_ptr;												///< Pointer to a generic base for a plug-in instances.

struct ak_wwise_plugin_interface_array_item;
struct ak_wwise_plugin_info;
struct ak_wwise_plugin_container;

typedef void ak_wwise_plugin_widget;

#ifdef __cplusplus
/**
 * \brief Generic base for all plug-in instances in C++
 * 
 * \warning This base differs from the ak_wwise_plugin_base_instance because C++ classes have a virtual table as their first member.
 * To distinguish between the C and C++ versions, you must use a static_cast operation instead of a reinterpret_cast or a C-style cast. 
 * Therefore, we recommend that you instantiate a plug-in in C++, and then return C pointers to the instance structures located at their base address.
 
 As such, it is expected to instantiate a plug-in in C++, and return
 * C pointers back to the instance structures with a different base address.
 */
struct ak_wwise_plugin_cpp_base_instance : public ak_wwise_plugin_base_instance
{
	virtual ~ak_wwise_plugin_cpp_base_instance() {}
};

/**
 * \brief Define a generic instance base, either in C or in C++.
 * 
 * \warning These macros are not equivalent. In C, you can directly cast from your instance to an
 * ak_wwise_plugin_base_instance. In C++, because the base classes have a virtual table, the pointer
 * to the C members is shifted in memory.
 */
#define AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE : public ak_wwise_plugin_cpp_base_instance
#else
#define AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE
#endif

#ifdef __cplusplus
/**
 * \brief Define a generic instance base, either in C or in C++.
 * 
 */

#define AK_WWISE_PLUGIN_INTERFACE_EXTEND_PREVIOUS(interface, interface_prev) \
	struct interface : public interface_prev {};
#else
#define AK_WWISE_PLUGIN_INTERFACE_EXTEND_PREVIOUS(interface, interface_prev) \
	struct interface { struct interface_prev; };
#endif

/**
 * \brief Plug-in backend instance.
 * 
 * A backend contains all the logic for the Authoring part of the plug-in. It is uniquely instantiated for each plug-in instance
 * in a project.
 */
struct ak_wwise_plugin_backend_instance AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Plug-in frontend instance.
 * 
 * A frontend contains the visual part of the Authoring plug-in. It is optional: for example, it is never instantiated when
 * connecting through WwiseConsole. Therefore, do not implement any processing or business work in a frontend part.
 * 
 * For example, avoid using the frontend for handling licensing, SoundBank generation, audio processing, properties (such as
 * validating ranges for property sets), media handling, media conversion, custom data loading, or other related work.
 * 
 * \aknote In legacy plug-ins from before Wwise 2021.1, there is only one instance of the Authoring plug-in that contains both
 * the backend and the frontend. This has changed for multiple reasons: to be able to instantiate a plug-in backend without any
 * frontend, or alternatively, to instantiate multiple copies of a frontend for a unique backend. \endaknote
 */
struct ak_wwise_plugin_frontend_instance AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

#ifdef __cplusplus
/**
 * \brief Define an instance type as a backend.
 * 
 * In C++, this derives the backend instance from the ak_wwise_plugin_backend_instance in a separate action so that RTTI can help
 * identify the types and the compiler ensures the type correctness.
 * 
 * In C, this is a typedef, because it is not possible to derive structures. The compiler also doesn't need to look up addresses
 * for complex structures, so this is just a no-op.
 * 
 * \sa
 * - \ref ak_wwise_plugin_backend_instance for a backend discussion.
 * - \ref ak_wwise_plugin_frontend_instance for a frontend discussion.
 * - \ref ak_wwise_plugin_cpp_base_instance for discrepancies between the C and C++ models.
 */
#define AK_WWISE_PLUGIN_DERIVE_FROM_BACKEND_INSTANCE(x) \
	struct x : public ak_wwise_plugin_backend_instance {}

/**
 * \brief Define an instance type as a frontend.
 * 
 * In C++, this derives the backend instance from the ak_wwise_plugin_backend_instance in a separate action so that RTTI can help
 * identify the types and the compiler ensures the type correctness.
 *
 * In C, this is a typedef, because it is not possible to derive structures. The compiler also doesn't need to look up addresses
 * for complex structures, so this is just a no-op.
 * 
 * \sa
 * - \ref ak_wwise_plugin_backend_instance for a backend discussion.
 * - \ref ak_wwise_plugin_frontend_instance for a frontend discussion.
 * - \ref ak_wwise_plugin_cpp_base_instance for discrepancies between the C and C++ models.
 */
#define AK_WWISE_PLUGIN_DERIVE_FROM_FRONTEND_INSTANCE(x) \
	struct x : public ak_wwise_plugin_frontend_instance {}
#else
#define AK_WWISE_PLUGIN_DERIVE_FROM_BACKEND_INSTANCE(x) \
	typedef x ak_wwise_plugin_backend_instance
#define AK_WWISE_PLUGIN_DERIVE_FROM_FRONTEND_INSTANCE(x) \
	typedef x ak_wwise_plugin_frontend_instance
#endif

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_v1 C interface.
 * - AK::Wwise::Plugin::V1::Host C++ class.
 */
struct ak_wwise_plugin_host_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_v2.
 * 
 * \sa
 * - ak_wwise_plugin_host_v2 C interface.
 * - AK::Wwise::Plugin::V2::Host C++ class.
 */
AK_WWISE_PLUGIN_INTERFACE_EXTEND_PREVIOUS(
	ak_wwise_plugin_host_instance_v2,
	ak_wwise_plugin_host_instance_v1
);

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_conversion_helpers_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_conversion_helpers_v1 C interface.
 * - AK::Wwise::Plugin::V1::ConversionHelpers C++ class.
 */
struct ak_wwise_plugin_host_conversion_helpers_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_data_writer_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_data_writer_v1 C interface.
 * - AK::Wwise::Plugin::V1::DataWriter C++ class.
 */
struct ak_wwise_plugin_host_data_writer_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_object_media_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_object_media_v1 C interface.
 * - AK::Wwise::Plugin::V1::ObjectMedia C++ class.
 */
struct ak_wwise_plugin_host_object_media_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_object_store_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_object_store_v1 C interface.
 * - AK::Wwise::Plugin::V1::ObjectStore C++ class.
 */
struct ak_wwise_plugin_host_object_store_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_property_set_v1.
 * 
 * Is also compatible with ak_wwise_plugin_host_property_set_v2.
 * 
 * \sa
 * - ak_wwise_plugin_host_property_set_v1 C interface.
 * - AK::Wwise::Plugin::V1::PropertySet C++ class.
 * - ak_wwise_plugin_host_property_set_v2 C interface.
 * - AK::Wwise::Plugin::V2::PropertySet C++ class.
 */
struct ak_wwise_plugin_host_property_set_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_reference_set_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_reference_set_v1 C interface.
 * - AK::Wwise::Plugin::ReferenceSet C++ class.
 */
struct ak_wwise_plugin_host_reference_set_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for ak_wwise_plugin_host_undo_manager_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_undo_manager_v1 C interface.
 * - AK::Wwise::Plugin::V1::UndoManager C++ class.
 */
struct ak_wwise_plugin_host_undo_manager_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for reading XML files through ak_wwise_plugin_host_xml_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_xml_v1 C interface.
 * - AK::Wwise::Plugin::V1::XmlReader C++ class.
 */
struct ak_wwise_plugin_host_xml_reader_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base host-provided instance type for writing XML files through ak_wwise_plugin_host_xml_v1.
 * 
 * \sa
 * - ak_wwise_plugin_host_xml_v1 C interface.
 * - AK::Wwise::Plugin::V1::XmlWriter C++ class.
 */
struct ak_wwise_plugin_host_xml_writer_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};


struct ak_wwise_plugin_analysis_task_v1;
/**
 * \brief Base instance type for providing analysis task services through ak_wwise_plugin_analysis_task_v1.
 * 
 * \sa
 * - ak_wwise_plugin_analysis_task_v1 C interface.
 * - AK::Wwise::Plugin::V1::AnalysisTask C++ class.
 */
struct ak_wwise_plugin_analysis_task_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_audio_plugin_v1;
/**
 * \brief Base instance type for providing audio plug-in backend services through ak_wwise_plugin_audio_plugin_v1.
 * 
 * \sa
 * - ak_wwise_plugin_audio_plugin_v1 C interface.
 * - AK::Wwise::Plugin::V1::AudioPlugin C++ class.
 */
AK_WWISE_PLUGIN_DERIVE_FROM_BACKEND_INSTANCE(ak_wwise_plugin_audio_plugin_instance_v1);

struct ak_wwise_plugin_conversion_v1;
/**
 * \brief Base instance type for providing a conversion plug-in through ak_wwise_plugin_conversion_v1.
 * 
 * \sa
 * - ak_wwise_plugin_conversion_v1 C interface.
 * - AK::Wwise::Plugin::V1::Conversion C++ class.
 */
AK_WWISE_PLUGIN_DERIVE_FROM_BACKEND_INSTANCE(ak_wwise_plugin_conversion_instance_v1);

struct ak_wwise_plugin_custom_data_v1;
/**
 * \brief Base instance type for providing custom data loading and saving through ak_wwise_plugin_custom_data_v1.
 * 
 * \sa
 * - ak_wwise_plugin_custom_data_v1 C interface.
 * - AK::Wwise::Plugin::V1::CustomData C++ class.
 */
struct ak_wwise_plugin_custom_data_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_property_display_name_v1;
/**
 * \brief Base instance type for providing display names to properties through ak_wwise_plugin_property_display_name_v1.
 * 
 * \sa
 * - ak_wwise_plugin_property_display_name_v1 C interface.
 * - AK::Wwise::Plugin::V1::PropertyDisplayName C++ class.
 */
struct ak_wwise_plugin_property_display_name_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_feedback_aware_v1;
/**
 * \brief Base instance type for providing property-based feedback through ak_wwise_plugin_feedback_aware_v1.
 * 
 * \sa
 * - ak_wwise_plugin_feedback_aware_v1 C interface.
 * - AK::Wwise::Plugin::V1::FeedbackAware C++ class.
 */
struct ak_wwise_plugin_feedback_aware_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_gui_conversion_windows_v1;
/**
 * \brief Base instance type for providing a Windows frontend for a conversion plug-in through ak_wwise_plugin_gui_conversion_windows_v1.
 * 
 * \sa
 * - ak_wwise_plugin_gui_conversion_windows_v1 C interface.
 * - AK::Wwise::Plugin::V1::GUIConversionWindows C++ class.
 * - ak_wwise_plugin_conversion_v1 Conversion backend's C interface.
 * - AK::Wwise::Plugin::V1::Conversion Conversion frontend's C++ class.
 */
AK_WWISE_PLUGIN_DERIVE_FROM_FRONTEND_INSTANCE(ak_wwise_plugin_gui_conversion_windows_instance_v1);

struct ak_wwise_plugin_gui_windows_v1;
/**
 * \brief Base instance type for providing a Windows frontend for an audio plug-in through ak_wwise_plugin_gui_windows_v1.
 * 
 * \sa
 * - ak_wwise_plugin_gui_windows_v1 C interface.
 * - AK::Wwise::Plugin::V1::GUIWindows C++ class.
 * - ak_wwise_plugin_audio_plugin_v1 Audio plug-in backend's C interface.
 * - AK::Wwise::Plugin::V1::AudioPlugin Audio plug-in frontend's C++ class.
 */
AK_WWISE_PLUGIN_DERIVE_FROM_FRONTEND_INSTANCE(ak_wwise_plugin_gui_windows_instance_v1);

struct ak_wwise_plugin_link_backend_v1;
/**
 * \brief Base host-provided instance to retrieve the related backend instance, as shown in the frontend.
 * 
 * \sa
 * - ak_wwise_plugin_link_backend_v1 C interface.
 * - AK::Wwise::Plugin::V1::LinkBackend C++ class.
 */
struct ak_wwise_plugin_link_backend_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_link_frontend_v1;
/**
 * \brief Base host-provided instance to retrieve the related frontend instances related to the current backend.
 * 
 * \sa
 * - ak_wwise_plugin_link_frontend_v1 C interface.
 * - AK::Wwise::Plugin::V1::LinkFrontend C++ class.
 */
struct ak_wwise_plugin_link_frontend_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_media_converter_v1;
/**
 * \brief Base instance type for providing custom media conversion through ak_wwise_plugin_media_converter_v1.
 * 
 * \sa
 * - ak_wwise_plugin_media_converter_v1 C interface.
 * - AK::Wwise::Plugin::V1::MediaConverter C++ class.
 */
struct ak_wwise_plugin_media_converter_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_host_v1;
/**
 * \brief Base instance type for receiving notifications on host changes events.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_host_v1 C interface.
 * - AK::Wwise::Plugin::V1::Notifications::Host_ C++ class.
 * - ak_wwise_plugin_host_v1 Related C host interface.
 * - AK::Wwise::Plugin::V1::Host Related C++ host class.
 */
struct ak_wwise_plugin_notifications_host_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_monitor_v1;
/**
 * \brief Base instance type for receiving Sound Engine's monitoring data.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_monitor_v1 C interface.
 * - AK::Wwise::Plugin::V1::Notifications::Monitor C++ class.
 * - AK::Wwise::Plugin::MonitorData
 */
struct ak_wwise_plugin_notifications_monitor_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_object_media_v1;
/**
 * \brief Base instance type for receiving notifications on related object media's changes.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_object_media_v1 C interface.
 * - AK::Wwise::Plugin::V1::Notifications::ObjectMedia_ C++ class.
 * - ak_wwise_plugin_host_object_media_v1 Related C host interface.
 * - AK::Wwise::Plugin::V1::ObjectMedia Related C++ host class.
 */
struct ak_wwise_plugin_notifications_object_media_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_object_store_v1;
/**
 * \brief Base instance type for receiving notifications on related Object Store's changes.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_object_store_v1 C interface.
 * - AK::Wwise::Plugin::V1::Notifications::ObjectStore_ C++ class.
 * - ak_wwise_plugin_host_object_store_v1 Related C host interface.
 * - AK::Wwise::Plugin::V1::ObjectStore Related C++ host class.
 */
struct ak_wwise_plugin_notifications_object_store_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_property_set_v1;
/**
 * \brief Base instance type for receiving notifications on property set's changes.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_property_set_v1 C interface.
 * - AK::Wwise::Plugin::V1::Notifications::PropertySet_ C++ class.
 * - ak_wwise_plugin_host_property_set_v1 Related C host interface.
 * - AK::Wwise::Plugin::V1::PropertySet Related C++ host class.
 */
struct ak_wwise_plugin_notifications_property_set_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_notifications_reference_set_v1;
/**
 * \brief Base instance type for receiving notifications on reference set's changes.
 * 
 * \sa
 * - ak_wwise_plugin_notifications_reference_set_v1 C interface.
 * - AK::Wwise::Plugin::Notifications::ReferenceSet_ C++ class.
 * - ak_wwise_plugin_host_reference_set_v1 Related C host interface.
 * - AK::Wwise::Plugin::ReferenceSet Related C++ host class.
 */
struct ak_wwise_plugin_notifications_reference_set_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_source_v1;
/**
 * \brief Base instance type for providing source-specific information, through ak_wwise_plugin_source_v1.
 * 
 * \sa
 * - ak_wwise_plugin_source_v1 C interface.
 * - AK::Wwise::Plugin::V1::Source C++ class.
 */
struct ak_wwise_plugin_source_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_undo_event_v1;
/**
 * \brief Base instance type for providing custom undo operations through ak_wwise_plugin_undo_event_v1.
 * 
 * \sa
 * - ak_wwise_plugin_undo_event_v1 C interface.
 * - AK::Wwise::Plugin::V1::UndoEvent C++ class.
 * - ak_wwise_plugin_host_undo_manager_v1 Host's C manager interface.
 * - AK::Wwise::Plugin::V1::UndoManager Host's C++ manager class.
 */
struct ak_wwise_plugin_undo_event_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_license_v1;
/**
 * \brief Base instance type for providing licensing information, through ak_wwise_plugin_license_v1.
 * 
 * \sa
 * - ak_wwise_plugin_license_v1 C interface.
 * - AK::Wwise::Plugin::V1::License C++ class.
 */
struct ak_wwise_plugin_license_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};


/**
 * \brief Base instance type for providing a message shown the first time an instance is created through ak_wwise_plugin_first_time_creation_message_v1.
 * 
 * \sa
 * - ak_wwise_plugin_first_time_creation_message_v1 C interface.
 * - AK::Wwise::Plugin::V1::FirstTimeCreationMessage C++ class.
 */
struct ak_wwise_plugin_first_time_creation_message_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

/**
 * \brief Base instance type for providing a device list for your custom sink through ak_wwise_plugin_sink_devices_v1.
 * 
 * \sa
 * - ak_wwise_plugin_sink_devices_v1 C interface.
 * - AK::Wwise::Plugin::V1::SinkDevices C++ class.
 */
struct ak_wwise_plugin_sink_devices_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_test_service_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};
struct ak_wwise_plugin_test_service_instance_v2 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_frontend_v1;
AK_WWISE_PLUGIN_DERIVE_FROM_FRONTEND_INSTANCE(ak_wwise_plugin_frontend_instance_v1);

struct ak_wwise_plugin_host_frontend_model_instance_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE {};

struct ak_wwise_plugin_host_frontend_model_args_v1 AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE
{
	const char* templateName;
};

#undef AK_WWISE_PLUGIN_DERIVE_FROM_INSTANCE_BASE


/**
 * @}
 */

#ifdef __cplusplus
namespace AK::Wwise::Plugin
{
	using CBaseInterface = ak_wwise_plugin_base_interface;																///< \copydoc ak_wwise_plugin_base_interface
	using CInterfacePtr = ak_wwise_plugin_interface_ptr;
	using CInterfaceArrayItem = ak_wwise_plugin_interface_array_item;													///< \copydoc ak_wwise_plugin_interface_array_item
	using CPluginInfo = ak_wwise_plugin_info;																			///< \copydoc ak_wwise_plugin_info
	using CPluginContainer = ak_wwise_plugin_container;																	///< \copydoc ak_wwise_plugin_container
	using CWidget = ak_wwise_plugin_widget;

	using BaseInterface = CBaseInterface;																				///< \copydoc ak_wwise_plugin_base_interface
	using InterfacePtr = CInterfacePtr;
	using InterfaceArrayItem = CInterfaceArrayItem;																		///< \copydoc ak_wwise_plugin_interface_array_item
	using PluginInfo = CPluginInfo;																						///< \copydoc ak_wwise_plugin_info
	using PluginContainer = CPluginContainer;																			///< \copydoc ak_wwise_plugin_container
	using Widget = CWidget;
}
#endif
