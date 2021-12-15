/*
Copyright (c) 2020-2021 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License 2.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   https://www.eclipse.org/legal/epl-2.0/
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Contributors:
   Roger Light - initial implementation and documentation.
*/

#include "config.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "json_help.h"
#include "mosquitto.h"
#include "mosquitto_broker.h"
#include "mosquitto_plugin.h"
#include "mqtt_protocol.h"

#include "dynamic_security.h"

struct dynsec__acl_default_access default_access = {false, false, false, false};

int dynsec__process_set_default_acl_access(cJSON *j_responses, struct mosquitto *context, cJSON *command, char *correlation_data)
{
	cJSON *j_actions, *j_action, *j_acltype, *j_allow;
	bool allow;
	const char *admin_clientid, *admin_username;

	j_actions = cJSON_GetObjectItem(command, "acls");
	if(j_actions == NULL || !cJSON_IsArray(j_actions)){
		dynsec__command_reply(j_responses, context, "setDefaultACLAccess", "Missing/invalid actions array", correlation_data);
		return MOSQ_ERR_INVAL;
	}

	admin_clientid = mosquitto_client_id(context);
	admin_username = mosquitto_client_username(context);

	cJSON_ArrayForEach(j_action, j_actions){
		j_acltype = cJSON_GetObjectItem(j_action, "acltype");
		j_allow = cJSON_GetObjectItem(j_action, "allow");
		if(j_acltype && cJSON_IsString(j_acltype)
					&& j_allow && cJSON_IsBool(j_allow)){

			allow = cJSON_IsTrue(j_allow);

			if(!strcasecmp(j_acltype->valuestring, ACL_TYPE_PUB_C_SEND)){
				default_access.publish_c_send = allow;
			}else if(!strcasecmp(j_acltype->valuestring, ACL_TYPE_PUB_C_RECV)){
				default_access.publish_c_recv = allow;
			}else if(!strcasecmp(j_acltype->valuestring, ACL_TYPE_SUB_GENERIC)){
				default_access.subscribe = allow;
			}else if(!strcasecmp(j_acltype->valuestring, ACL_TYPE_UNSUB_GENERIC)){
				default_access.unsubscribe = allow;
			}
			mosquitto_log_printf(MOSQ_LOG_INFO, "dynsec: %s/%s | setDefaultACLAccess | acltype=%s | allow=%s",
					admin_clientid, admin_username, j_acltype->valuestring, allow?"true":"false");
		}
	}

	dynsec__config_save();
	dynsec__command_reply(j_responses, context, "setDefaultACLAccess", NULL, correlation_data);
	return MOSQ_ERR_SUCCESS;
}


int dynsec__process_get_default_acl_access(cJSON *j_responses, struct mosquitto *context, cJSON *command, char *correlation_data)
{
	cJSON *tree, *jtmp, *j_data, *j_acls, *j_acl;
	const char *admin_clientid, *admin_username;

	UNUSED(command);

	tree = cJSON_CreateObject();
	if(tree == NULL){
		dynsec__command_reply(j_responses, context, "getDefaultACLAccess", "Internal error", correlation_data);
		return MOSQ_ERR_NOMEM;
	}

	admin_clientid = mosquitto_client_id(context);
	admin_username = mosquitto_client_username(context);
	mosquitto_log_printf(MOSQ_LOG_INFO, "dynsec: %s/%s | getDefaultACLAccess",
			admin_clientid, admin_username);

	if(cJSON_AddStringToObject(tree, "command", "getDefaultACLAccess") == NULL
		|| ((j_data = cJSON_AddObjectToObject(tree, "data")) == NULL)

			){
		goto internal_error;
	}

	j_acls = cJSON_AddArrayToObject(j_data, "acls");
	if(j_acls == NULL){
		goto internal_error;
	}

	/* publishClientSend */
	j_acl = cJSON_CreateObject();
	if(j_acl == NULL){
		goto internal_error;
	}
	cJSON_AddItemToArray(j_acls, j_acl);
	if(cJSON_AddStringToObject(j_acl, "acltype", ACL_TYPE_PUB_C_SEND) == NULL
			|| cJSON_AddBoolToObject(j_acl, "allow", default_access.publish_c_send) == NULL
			){

		goto internal_error;
	}

	/* publishClientReceive */
	j_acl = cJSON_CreateObject();
	if(j_acl == NULL){
		goto internal_error;
	}
	cJSON_AddItemToArray(j_acls, j_acl);
	if(cJSON_AddStringToObject(j_acl, "acltype", ACL_TYPE_PUB_C_RECV) == NULL
			|| cJSON_AddBoolToObject(j_acl, "allow", default_access.publish_c_recv) == NULL
			){

		goto internal_error;
	}

	/* subscribe */
	j_acl = cJSON_CreateObject();
	if(j_acl == NULL){
		goto internal_error;
	}
	cJSON_AddItemToArray(j_acls, j_acl);
	if(cJSON_AddStringToObject(j_acl, "acltype", ACL_TYPE_SUB_GENERIC) == NULL
			|| cJSON_AddBoolToObject(j_acl, "allow", default_access.subscribe) == NULL
			){

		goto internal_error;
	}

	/* unsubscribe */
	j_acl = cJSON_CreateObject();
	if(j_acl == NULL){
		goto internal_error;
	}
	cJSON_AddItemToArray(j_acls, j_acl);
	if(cJSON_AddStringToObject(j_acl, "acltype", ACL_TYPE_UNSUB_GENERIC) == NULL
			|| cJSON_AddBoolToObject(j_acl, "allow", default_access.unsubscribe) == NULL
			){

		goto internal_error;
	}

	cJSON_AddItemToArray(j_responses, tree);

	if(correlation_data){
		jtmp = cJSON_AddStringToObject(tree, "correlationData", correlation_data);
		if(jtmp == NULL){
			goto internal_error;
		}
	}

	return MOSQ_ERR_SUCCESS;

internal_error:
	cJSON_Delete(tree);
	dynsec__command_reply(j_responses, context, "getDefaultACLAccess", "Internal error", correlation_data);
	return MOSQ_ERR_NOMEM;
}
