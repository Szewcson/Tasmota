/*
  be_matter_module.c - implements the high level `matter` Berry module

  Copyright (C) 2023  Stephan Hadinger & Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/********************************************************************
 * Matter global module
 * 
 *******************************************************************/

#include "be_constobj.h"
#include "be_mapping.h"

// Matter logo
static const uint8_t MATTER_LOGO[] = 
  "<svg style='vertical-align:middle;' width='24' height='24' xmlns='http://www.w3.org/2000/svg' viewBox='100 100 240 240'>"
  "<defs><style>.cls-1{fill:none}.cls-2{fill:#FFFFFF;}</style></defs><rect class='cls-1' "
  "width='420' height='420'/><path class='cls-2' d='"
  "M167,156.88a71,71,0,0,0,32.1,14.73v-62.8l12.79-7.38,12.78,7.38v62.8a71.09,71.09,0,0,0,32.11-14.73"
  "L280,170.31a96.92,96.92,0,0,1-136.33,0Zm28.22,160.37A96.92,96.92,0,0,0,127,199.19v26.87a71.06,"
  "71.06,0,0,1,28.82,20.43l-54.39,31.4v14.77L114.22,300l54.38-31.4a71,71,0,0,1,3.29,35.17Zm101.5-"
  "118.06a96.93,96.93,0,0,0-68.16,118.06l23.27-13.44a71.1,71.1,0,0,1,3.29-35.17L309.46,300l12.78-"
  "7.38V277.89l-54.39-31.4a71.13,71.13,0,0,1,28.82-20.43Z'/></svg>";

extern const bclass be_class_Matter_Counter;
extern const bclass be_class_Matter_Verhoeff;
extern const bclass be_class_Matter_QRCode;

#include "solidify/solidified_Matter_Module.h"

#include "../generate/be_matter_clusters.h"
#include "../generate/be_matter_opcodes.h"

const char* matter_get_cluster_name(uint16_t cluster) {
  for (const matter_cluster_t * cl = matterAllClusters; cl->id != 0xFFFF; cl++) {
    if (cl->id == cluster) {
      return cl->name;
    }
  }
  return NULL;
}
BE_FUNC_CTYPE_DECLARE(matter_get_cluster_name, "s", "i")

const char* matter_get_opcode_name(uint16_t opcode) {
  for (const matter_opcode_t * op = matter_OpCodes; op->id != 0xFFFF; op++) {
    if (op->id == opcode) {
      return op->name;
    }
  }
  return NULL;
}
BE_FUNC_CTYPE_DECLARE(matter_get_opcode_name, "s", "i")

const char* matter_get_attribute_name(uint16_t cluster, uint16_t attribute) {
  for (const matter_cluster_t * cl = matterAllClusters; cl->id != 0xFFFF; cl++) {
    if (cl->id == cluster) {
      for (const matter_attribute_t * at = cl->attributes; at->id != 0xFFFF; at++) {
        if (at->id == attribute) {
          return at->name;
        }
      }
    }
  }
  return NULL;
}
BE_FUNC_CTYPE_DECLARE(matter_get_attribute_name, "s", "ii")

bbool matter_is_attribute_writable(uint16_t cluster, uint16_t attribute) {
  for (const matter_cluster_t * cl = matterAllClusters; cl->id != 0xFFFF; cl++) {
    if (cl->id == cluster) {
      for (const matter_attribute_t * at = cl->attributes; at->id != 0xFFFF; at++) {
        if (at->id == attribute) {
          return (at->flags & 0x01) ? btrue : bfalse;
        }
      }
    }
  }
  return bfalse;
}
BE_FUNC_CTYPE_DECLARE(matter_is_attribute_writable, "b", "ii")

bbool matter_is_attribute_reportable(uint16_t cluster, uint16_t attribute) {
  for (const matter_cluster_t * cl = matterAllClusters; cl->id != 0xFFFF; cl++) {
    if (cl->id == cluster) {
      for (const matter_attribute_t * at = cl->attributes; at->id != 0xFFFF; at++) {
        if (at->id == attribute) {
          return (at->flags & 0x02) ? btrue : bfalse;
        }
      }
    }
  }
  return bfalse;
}
BE_FUNC_CTYPE_DECLARE(matter_is_attribute_reportable, "b", "ii")

const char* matter_get_command_name(uint16_t cluster, uint16_t command) {
  for (const matter_cluster_t * cl = matterAllClusters; cl->id != 0xFFFF; cl++) {
    if (cl->id == cluster) {
      for (const matter_command_t * cmd = cl->commands; cmd->id != 0xFFFF; cmd++) {
        if (cmd->id == command) {
          return cmd->name;
        }
      }
    }
  }
  return NULL;
}
BE_FUNC_CTYPE_DECLARE(matter_get_command_name, "s", "ii")

// Convert an IP address from string to raw bytes
extern const void* matter_get_ip_bytes(const char* ip_str, size_t* ret_len);
BE_FUNC_CTYPE_DECLARE(matter_get_ip_bytes, "&", "s")


#include "solidify/solidified_Matter_inspect.h"

extern const bclass be_class_Matter_TLV;   // need to declare it upfront because of circular reference
#include "solidify/solidified_Matter_Path.h"
#include "solidify/solidified_Matter_TLV.h"
#include "solidify/solidified_Matter_IM_Data.h"
#include "solidify/solidified_Matter_UDPServer.h"
#include "solidify/solidified_Matter_Expirable.h"
#include "solidify/solidified_Matter_Fabric.h"
#include "solidify/solidified_Matter_Session.h"
#include "solidify/solidified_Matter_Session_Store.h"
#include "solidify/solidified_Matter_Commissioning_Data.h"
#include "solidify/solidified_Matter_Commissioning.h"
#include "solidify/solidified_Matter_Message.h"
#include "solidify/solidified_Matter_MessageHandler.h"
#include "solidify/solidified_Matter_IM_Message.h"
#include "solidify/solidified_Matter_IM_Subscription.h"
#include "solidify/solidified_Matter_IM.h"
#include "solidify/solidified_Matter_Control_Message.h"
#include "solidify/solidified_Matter_Plugin.h"
#include "solidify/solidified_Matter_Base38.h"
#include "solidify/solidified_Matter_UI.h"
#include "solidify/solidified_Matter_Device.h"

#include "../generate/be_matter_certs.h"

#include "solidify/solidified_Matter_Plugin_Root.h"
#include "solidify/solidified_Matter_Plugin_Device.h"
#include "solidify/solidified_Matter_Plugin_OnOff.h"
#include "solidify/solidified_Matter_Plugin_Light0.h"
#include "solidify/solidified_Matter_Plugin_Light1.h"
#include "solidify/solidified_Matter_Plugin_Light2.h"
#include "solidify/solidified_Matter_Plugin_Light3.h"
#include "solidify/solidified_Matter_Plugin_Sensor.h"
#include "solidify/solidified_Matter_Plugin_Sensor_Pressure.h"
#include "solidify/solidified_Matter_Plugin_Sensor_Temp.h"
#include "solidify/solidified_Matter_Plugin_Sensor_Light.h"
#include "solidify/solidified_Matter_Plugin_Sensor_Humidity.h"

/*********************************************************************************************\
 * Get a bytes() object of the certificate DAC/PAI_Cert
\*********************************************************************************************/
static int matter_return_static_bytes(bvm *vm, const uint8* addr, size_t len) {
  be_getbuiltin(vm, "bytes");
  be_pushcomptr(vm, addr);
  be_pushint(vm, - len);
  be_call(vm, 2);
  be_pop(vm, 2);
  be_return(vm);
}
static int matter_PAI_Cert_FFF1(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentPAI_Cert_FFF1, sizeof(kDevelopmentPAI_Cert_FFF1)); }
static int matter_PAI_Pub_FFF1(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentPAI_PublicKey_FFF1, sizeof(kDevelopmentPAI_PublicKey_FFF1)); }
static int matter_PAI_Priv_FFF1(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentPAI_PrivateKey_FFF1, sizeof(kDevelopmentPAI_PrivateKey_FFF1)); }
static int matter_DAC_Cert_FFF1_8000(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentDAC_Cert_FFF1_8000, sizeof(kDevelopmentDAC_Cert_FFF1_8000)); }
static int matter_DAC_Pub_FFF1_8000(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentDAC_PublicKey_FFF1_8000, sizeof(kDevelopmentDAC_PublicKey_FFF1_8000)); }
static int matter_DAC_Priv_FFF1_8000(bvm *vm) { return matter_return_static_bytes(vm, kDevelopmentDAC_PrivateKey_FFF1_8000, sizeof(kDevelopmentDAC_PrivateKey_FFF1_8000)); }
static int matter_CD_FFF1_8000(bvm *vm) { return matter_return_static_bytes(vm, kCdForAllExamples, sizeof(kCdForAllExamples)); }


#include "be_fixed_matter.h"

/* @const_object_info_begin

module matter (scope: global, strings: weak) {
  _LOGO, comptr(MATTER_LOGO)
  MATTER_OPTION, int(151)       // SetOption151 enables Matter

  Verhoeff, class(be_class_Matter_Verhoeff)
  Counter, class(be_class_Matter_Counter)
  setmember, closure(matter_setmember_closure)
  member, closure(matter_member_closure)
  get_ip_bytes, ctype_func(matter_get_ip_bytes)

  get_cluster_name, ctype_func(matter_get_cluster_name)
  get_attribute_name, ctype_func(matter_get_attribute_name)
  is_attribute_writable, ctype_func(matter_is_attribute_writable)
  is_attribute_reportable, ctype_func(matter_is_attribute_reportable)
  get_command_name, ctype_func(matter_get_command_name)
  get_opcode_name, ctype_func(matter_get_opcode_name)
  TLV, class(be_class_Matter_TLV)
  sort, closure(matter_sort_closure)
  inspect, closure(matter_inspect_closure)

  // Status codes
  SUCCESS, int(0x00)
  FAILURE, int(0x01)
  INVALID_SUBSCRIPTION, int(0x7D)
  UNSUPPORTED_ACCESS, int(0x7E)
  UNSUPPORTED_ENDPOINT, int(0x7F)
  INVALID_ACTION, int(0x80)
  UNSUPPORTED_COMMAND, int(0x81)
  INVALID_COMMAND, int(0x85)
  UNSUPPORTED_ATTRIBUTE, int(0x86)
  CONSTRAINT_ERROR, int(0x87)
  UNSUPPORTED_WRITE, int(0x88)
  RESOURCE_EXHAUSTED, int(0x89)
  NOT_FOUND, int(0x8B)
  UNREPORTABLE_ATTRIBUTE, int(0x8C)
  INVALID_DATA_TYPE, int(0x8D)
  UNSUPPORTED_READ, int(0x8F)
  DATA_VERSION_MISMATCH, int(0x92)
  TIMEOUT, int(0x94)
  UNSUPPORTED_NODE, int(0x9B)
  BUSY, int(0x9C)
  UNSUPPORTED_CLUSTER, int(0xC3)
  NO_UPSTREAM_SUBSCRIP­TION, int(0xC5)
  NEEDS_TIMED_INTERACTION, int(0xC6)
  UNSUPPORTED_EVENT, int(0xC7)
  PATHS_EXHAUSTED, int(0xC8)
  TIMED_REQUEST_MISMATCH, int(0xC9)
  FAILSAFE_REQUIRED, int(0xCA)

  // Matter_Data_IM classes
  AttributePathIB, class(be_class_Matter_AttributePathIB)
  ClusterPathIB, class(be_class_Matter_ClusterPathIB)
  DataVersionFilterIB, class(be_class_Matter_DataVersionFilterIB)
  AttributeDataIB, class(be_class_Matter_AttributeDataIB)
  AttributeReportIB, class(be_class_Matter_AttributeReportIB)
  EventFilterIB, class(be_class_Matter_EventFilterIB)
  EventPathIB, class(be_class_Matter_EventPathIB)
  EventDataIB, class(be_class_Matter_EventDataIB)
  EventReportIB, class(be_class_Matter_EventReportIB)
  CommandPathIB, class(be_class_Matter_CommandPathIB)
  CommandDataIB, class(be_class_Matter_CommandDataIB)
  InvokeResponseIB, class(be_class_Matter_InvokeResponseIB)
  CommandStatusIB, class(be_class_Matter_CommandStatusIB)
  EventStatusIB, class(be_class_Matter_EventStatusIB)
  AttributeStatusIB, class(be_class_Matter_AttributeStatusIB)
  StatusIB, class(be_class_Matter_StatusIB)
  StatusResponseMessage, class(be_class_Matter_StatusResponseMessage)
  ReadRequestMessage, class(be_class_Matter_ReadRequestMessage)
  ReportDataMessage, class(be_class_Matter_ReportDataMessage)
  SubscribeRequestMessage, class(be_class_Matter_SubscribeRequestMessage)
  SubscribeResponseMessage, class(be_class_Matter_SubscribeResponseMessage)
  WriteRequestMessage, class(be_class_Matter_WriteRequestMessage)
  WriteResponseMessage, class(be_class_Matter_WriteResponseMessage)
  TimedRequestMessage, class(be_class_Matter_TimedRequestMessage)
  InvokeRequestMessage, class(be_class_Matter_InvokeRequestMessage)
  InvokeResponseMessage, class(be_class_Matter_InvokeResponseMessage)

  // Matter Commisioning messages
  PBKDFParamRequest, class(be_class_Matter_PBKDFParamRequest)
  PBKDFParamResponse, class(be_class_Matter_PBKDFParamResponse)
  Pake1, class(be_class_Matter_Pake1)
  Pake2, class(be_class_Matter_Pake2)
  Pake3, class(be_class_Matter_Pake3)
  Sigma1, class(be_class_Matter_Sigma1)
  Sigma2, class(be_class_Matter_Sigma2)
  Sigma2Resume, class(be_class_Matter_Sigma2Resume)
  Sigma3, class(be_class_Matter_Sigma3)

  Commisioning_Context, class(be_class_Matter_Commisioning_Context)

  // UDP Server
  UDPPacket_sent, class(be_class_Matter_UDPPacket_sent)
  UDPServer, class(be_class_Matter_UDPServer)

  // Expirable
  Expirable, class(be_class_Matter_Expirable)
  Expirable_list, class(be_class_Matter_Expirable_list)

  // Sessions
  Fabric, class(be_class_Matter_Fabric)
  Session, class(be_class_Matter_Session)
  Session_Store, class(be_class_Matter_Session_Store)

  // Message Handler
  Frame, class(be_class_Matter_Frame)
  MessageHandler, class(be_class_Matter_MessageHandler)

  // Interation Model
  Path, class(be_class_Matter_Path)
  IM_Status, class(be_class_Matter_IM_Status)
  IM_InvokeResponse, class(be_class_Matter_IM_InvokeResponse)
  IM_WriteResponse, class(be_class_Matter_IM_WriteResponse)
  IM_ReportData, class(be_class_Matter_IM_ReportData)
  IM_ReportDataSubscribed, class(be_class_Matter_IM_ReportDataSubscribed)
  IM_SubscribeResponse, class(be_class_Matter_IM_SubscribeResponse)
  IM_SubscribedHeartbeat, class(be_class_Matter_IM_SubscribedHeartbeat)
  IM_Subscription, class(be_class_Matter_IM_Subscription)
  IM_Subscription_Shop, class(be_class_Matter_IM_Subscription_Shop)
  IM, class(be_class_Matter_IM)
  Control_Message, class(be_class_Matter_Control_Message)
  UI, class(be_class_Matter_UI)

  // QR Code
  QRCode, class(be_class_Matter_QRCode)

  // Base38 for QR Code
  Base38, class(be_class_Matter_Base38)

  // Matter Device core class
  Device, class(be_class_Matter_Device)

  // credentials from example
  PAI_Cert_FFF1, func(matter_PAI_Cert_FFF1)
  PAI_Pub_FFF1, func(matter_PAI_Pub_FFF1)
  PAI_Priv_FFF1, func(matter_PAI_Priv_FFF1)
  DAC_Cert_FFF1_8000, func(matter_DAC_Cert_FFF1_8000)
  DAC_Pub_FFF1_8000, func(matter_DAC_Pub_FFF1_8000)
  DAC_Priv_FFF1_8000, func(matter_DAC_Priv_FFF1_8000)
  CD_FFF1_8000, func(matter_CD_FFF1_8000)               // Certification Declaration

  // Plugins
  Plugin_Root, class(be_class_Matter_Plugin_Root)       // Generic behavior common to all devices
  Plugin_Device, class(be_class_Matter_Plugin_Device)   // Generic device (abstract)
  Plugin_OnOff, class(be_class_Matter_Plugin_OnOff)     // Relay/Light behavior (OnOff)
  Plugin_Light0, class(be_class_Matter_Plugin_Light0)     // OnOff Light
  Plugin_Light1, class(be_class_Matter_Plugin_Light1)     // Dimmable Light
  Plugin_Light2, class(be_class_Matter_Plugin_Light2)     // Color Temperature Light
  Plugin_Light3, class(be_class_Matter_Plugin_Light3)     // Extended Color Light
  Plugin_Sensor, class(be_class_Matter_Plugin_Sensor)     // Generic Sensor
  Plugin_Sensor_Pressure, class(be_class_Matter_Plugin_Sensor_Pressure)   // Pressure Sensor
  Plugin_Sensor_Temp, class(be_class_Matter_Plugin_Sensor_Temp)           // Temperature Sensor
  Plugin_Sensor_Light, class(be_class_Matter_Plugin_Sensor_Light)         // Light Sensor
  Plugin_Sensor_Humidity, class(be_class_Matter_Plugin_Sensor_Humidity)   // Humidity Sensor
}

@const_object_info_end */

