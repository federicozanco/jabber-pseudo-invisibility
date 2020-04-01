/*
 * Jabber Pseudo Invisibility Plugin
 *  Copyright (C) 2010 - 2012, Federico Zanco <federico.zanco ( at ) gmail.com>
 *
 *
 * License:
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */
 
#define PLUGIN_ID			"jabber-pseudo-invisibility"
#define PLUGIN_NAME			"Jabber Pseudo Invisibility"
#define PLUGIN_VERSION		"0.4.3"
#define PLUGIN_STATIC_NAME	"jabber-pseudo-invisibility"
#define PLUGIN_SUMMARY		"Provide a pseudo invisibility for jabber accounts."
#define PLUGIN_DESCRIPTION	"Provide a pseudo invisibility for jabber accounts."
#define PLUGIN_AUTHOR		"Federico Zanco <federico.zanco@gmail.com>"
#define PLUGIN_WEBSITE		"http://www.siorarina.net/jabber-pseudo-invisibility/"

#define PREF_PREFIX					"/plugins/core/" PLUGIN_ID
#define PREF_DISABLE_INACTIVITY		PREF_PREFIX "/disable_inactivity"
#define PREF_IDLE_REPORTING			PREF_PREFIX "/idle_reporting"
#define PREF_AWAY_WHEN_IDLE			PREF_PREFIX "/away_when_idle"
#define PREF_AUTO_AWAY				PREF_PREFIX "/auto_away"
#define PREF_UPDATE_TIMEOUT			PREF_PREFIX "/update_timeout"
#define PREF_UPDATE_TIMEOUT_DEFAULT	120
#define PREF_TIMEOUT_MIN			1
#define PREF_TIMEOUT_MAX			3600

#define STRING_INACTIVITY_WARNING	"You haven't disabled Idle Reporting and \
Auto Away. This means that your status will be changed automatically to Idle \
and you'll result online!.\n\nTo avoid this change Inactivity Preferences or \
set the 'Disable Idle Reporting and Auto Away' option in plugin conf."

#define STRING_PLUGIN_LOAD_WARNING		"Jabber Pseudo Invisibility plugin \
requires restart! Please exit and restart to get it working"

#define STRING_PLUGIN_UNLOAD_WARNING	"Jabber Pseudo Invisibility plugin \
requires restart! Please exit and restart to complete unload!"

#define PSEUDO_INVISIBLE_STATUS_ID		"pseudoinvisible"
#define PSEUDO_INVISIBLE_STATUS_NAME	"Pseudo Invisible"
#define	BEST_FRIENDS_GROUP				"Best Friends"
 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* config.h may define PURPLE_PLUGINS; protect the definition here so that we
 * don't get complaints about redefinition when it's not necessary. */
#ifndef PURPLE_PLUGINS
# define PURPLE_PLUGINS
#endif

#ifdef GLIB_H
# include <glib.h>
#endif

/* This will prevent compiler errors in some instances and is better explained in the
 * how-to documents on the wiki */
#ifndef G_GNUC_NULL_TERMINATED
# if __GNUC__ >= 4
#  define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define G_GNUC_NULL_TERMINATED
# endif
#endif

#include <blist.h>
#include <notify.h>
#include <debug.h>
#include <plugin.h>
#include <version.h>
#include <status.h>
#include <savedstatuses.h>
#include <prefs.h>
#include <string.h>


static PurplePlugin	*this_plugin = NULL;
static guint 		timeout = 0;


static gboolean
is_jabber_account(PurpleAccount *account)
{
	return g_strcmp0(purple_account_get_protocol_id(account), "prpl-jabber") == 0;
}

static gboolean
is_status_invisible()
{
	return purple_savedstatus_get_type(purple_savedstatus_get_current()) == PURPLE_STATUS_INVISIBLE;
}


static void 
add_status_invisible(PurpleAccount *account)
{
	PurpleStatusType *inv_type = NULL;
	PurpleStatus *inv_status = NULL;
	PurplePresence *pres = NULL;
	GList *types = NULL;
	GList *statuses = NULL;
	
	inv_type = purple_status_type_new_full(
		PURPLE_STATUS_INVISIBLE,		//primitive
		PSEUDO_INVISIBLE_STATUS_ID,		//id
		PSEUDO_INVISIBLE_STATUS_NAME,	//name 
		TRUE,							//saveable
		TRUE,							//user_settable
		FALSE);							//independent

	types = purple_account_get_status_types(account);

    // if status 'INVISIBLE' is already present there's no need to add it    
	for (; types; types = types->next)
	{
		if (purple_status_type_get_primitive(types->data) == PURPLE_STATUS_INVISIBLE)
			return;
	}
	
	types = purple_account_get_status_types(account);
	types = g_list_append(types, inv_type);
    account->status_types = types;
//	purple_account_set_status_types(account, types);

	pres = purple_account_get_presence(account);
	inv_status = purple_status_new(inv_type, pres);
	
	statuses = purple_presence_get_statuses(pres);
	statuses = g_list_append(statuses, inv_status);
}


static void
send_presence_unavailable(PurpleConnection *gc, const char *to)
{
	xmlnode *presence = NULL;

	purple_debug_info(PLUGIN_STATIC_NAME, "sending presence unavailable to %s\n", to);
		
	presence = xmlnode_new("presence");
	xmlnode_set_attrib(presence, "type", "unavailable");
	//xmlnode_set_attrib(presence, "id", "jpi");

	if (to)
		xmlnode_set_attrib(presence, "to", to);

	purple_signal_emit(
		purple_connection_get_prpl(gc),
		"jabber-sending-xmlnode",
		gc,
		&presence);
		                   
	if (presence)
		xmlnode_free(presence);
}


static void
send_presence_pseudoinvisible(PurpleConnection *gc, const char *to)
{
	xmlnode *presence = NULL;
	xmlnode *show = NULL;
	xmlnode *status = NULL;

	purple_debug_info(PLUGIN_STATIC_NAME, "sending presence pseudoinvisible to %s\n", to);
	
	presence = xmlnode_new("presence");
	xmlnode_set_attrib(presence, "to", to);
	//xmlnode_set_attrib(presence, "id", "jpi");
	show = xmlnode_new_child(presence, "show");
	xmlnode_insert_data(show, "dnd", -1);
	status = xmlnode_new_child(presence, "status");
	xmlnode_insert_data(status, "Pseudo Invisible", -1);

	purple_signal_emit(
		purple_connection_get_prpl(gc),
		"jabber-sending-xmlnode",
		gc,
		&presence);
		                   
	if (presence)
		xmlnode_free(presence);	
}


static void
update_account_blist(PurpleAccount* account)
{
	PurpleBuddy *buddy = NULL;
	PurpleConnection *gc = NULL;
	GSList *buddies = NULL;
	GSList *cur = NULL;
	
	purple_debug_info(PLUGIN_STATIC_NAME, "update_account_blist\n");
	
	gc = purple_account_get_connection(account);
	buddies = purple_find_buddies(account, NULL);
	
	// for every buddy in account blist
	for (cur = buddies; cur; cur = cur->next)
	{
		buddy = (PurpleBuddy *) cur->data;
		
		// if buddy is online
		if (PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			purple_debug_info(PLUGIN_STATIC_NAME, "updating %s	Account: %s\n",
				purple_buddy_get_name(buddy), purple_account_get_username(account));
			
			// if it is in Best Friends group ...
			if (purple_find_buddy_in_group(
						account,
						purple_buddy_get_name(buddy),
						purple_find_group(BEST_FRIENDS_GROUP)))
				// ...then send pseudoinvisible presence (to let it know you're online but 'pseudoinvisible')
				send_presence_pseudoinvisible(gc, purple_buddy_get_name(buddy));
			else
				// ...else send unavailable presence to pretend you're offline
				send_presence_unavailable(gc, purple_buddy_get_name(buddy));
		}
	}
	
	g_slist_free(buddies);
}


static gboolean
update_blist_cb(gpointer data)
{
	GList *accounts = NULL;

	purple_debug_info(PLUGIN_STATIC_NAME, "update_blist_cb\n");
	
	// resend presences to all online buddies
	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next)
	{
		if (is_jabber_account(accounts->data) && purple_account_is_connected(accounts->data))
			update_account_blist(accounts->data);
	}
	
	return TRUE;
}



static void
disable_inactivity()
{
	purple_prefs_set_string(PREF_IDLE_REPORTING,
		purple_prefs_get_string("/purple/away/idle_reporting"));
	purple_prefs_set_string("/purple/away/idle_reporting", "never");
	
	purple_prefs_set_bool(PREF_AWAY_WHEN_IDLE,
		purple_prefs_get_bool("/purple/away/away_when_idle"));
	purple_prefs_set_bool("/purple/away/away_when_idle", FALSE);

	purple_prefs_set_string(PREF_AUTO_AWAY,
		purple_prefs_get_string("/purple/away/auto_away"));
	purple_prefs_set_string("/purple/away/auto_away", "");
}


static void
reenable_inactivity()
{
	if (purple_prefs_exists(PREF_IDLE_REPORTING))
	{
		purple_prefs_set_string("/purple/away/idle_reporting",
			purple_prefs_get_string(PREF_IDLE_REPORTING));
		purple_prefs_remove(PREF_IDLE_REPORTING);
	}

	if (purple_prefs_exists(PREF_AWAY_WHEN_IDLE))
	{
		purple_prefs_set_bool("/purple/away/away_when_idle",
			purple_prefs_get_bool(PREF_AWAY_WHEN_IDLE));
		purple_prefs_remove(PREF_AWAY_WHEN_IDLE);
	}

	if (purple_prefs_exists(PREF_AUTO_AWAY))
	{
		purple_prefs_set_string("/purple/away/auto_away",
			purple_prefs_get_string(PREF_AUTO_AWAY));
		purple_prefs_remove(PREF_AUTO_AWAY);
	}
}


static gboolean
jabber_receiveing_presence_cb(PurpleConnection *gc, const char *type, const char *from, xmlnode *presence)
{
	PurpleAccount *account = NULL;
	gchar **bname = NULL;

	if (!is_status_invisible())
		return FALSE;
	
	// to send unavailable presence to an unavailable buddy is useless
	if (!g_strcmp0(type,"unavailable"))
		return FALSE;
	
	account = purple_connection_get_account(gc);

	// from is a full JID and we can't use it for searching so we have to extract
	// only buddy name (without resource)
	bname = g_strsplit(from, "/", 2);
	
	purple_debug_info(PLUGIN_STATIC_NAME, "jabber_receiveing_presence_cb from %s\n", bname[0]);
	
	// here is the point: when a buddy sends a presence (because a status change, signing on/off, ...),
	// if it's not in Best Friends group then we send a presence unavailable. This avoids polling
	// but makes very easy to discover who is pseudo invisible: just send a presence probe to offline
	// buddies, if they're pseudo invisible they'll answer with the real presence. Quite easy to hack!
	// It only make sense to send presence update to offline buddies because online buddies have already
	// updated the status
	if (!PURPLE_BUDDY_IS_ONLINE(purple_find_buddy(account, bname[0])))
	{ 
		if (purple_find_buddy_in_group(account, bname[0], purple_find_group(BEST_FRIENDS_GROUP)))
			send_presence_pseudoinvisible(gc, bname[0]);
		else
			send_presence_unavailable(gc, bname[0]);
	}
	
	g_free(bname[0]);
	g_free(bname[1]);

	return FALSE;
}


static void 
account_status_changed_cb(PurpleAccount *account, PurpleStatus *old, PurpleStatus *new, gpointer data)
{
	purple_debug_info(PLUGIN_STATIC_NAME, "account_status_changed\n");
    
    //run only for jabber accounts...
    if (is_jabber_account(account))
    {
    	//if effectly new status is 'invisible'...
		if (purple_status_type_get_primitive(purple_status_get_type(new)) == PURPLE_STATUS_INVISIBLE)
		{
			// ...then update account blist
			update_account_blist(account);

			// disable inactivity to avoid idle system to send presence changing status
			if (purple_prefs_get_bool(PREF_DISABLE_INACTIVITY))
				disable_inactivity();
			else
			{
				// warn user about effects of not disabling inactivity
				if (g_strcmp0(purple_prefs_get_string("/purple/away/idle_reporting"), "never") ||
					purple_prefs_get_bool("/purple/away/away_when_idle"))
					purple_notify_warning(
						this_plugin,
						PLUGIN_NAME,
						"WARNING!!!",
						STRING_INACTIVITY_WARNING);
			}
			
			// jabber receiving presence
			purple_signal_connect(purple_find_prpl("prpl-jabber"),
				"jabber-receiving-presence",
				this_plugin,
				PURPLE_CALLBACK(jabber_receiveing_presence_cb),
				NULL);

			timeout = purple_timeout_add_seconds(purple_prefs_get_int(PREF_UPDATE_TIMEOUT), update_blist_cb, NULL);
			
			purple_savedstatus_set_type(purple_savedstatus_get_default(), PURPLE_STATUS_INVISIBLE);
		}
		else if (purple_status_type_get_primitive(purple_status_get_type(old)) == PURPLE_STATUS_INVISIBLE)
		{
			// disable jabber-receiving-presence signal
			purple_signal_disconnect(purple_find_prpl("prpl-jabber"),
				"jabber-receiving-presence",
				this_plugin,
				PURPLE_CALLBACK(jabber_receiveing_presence_cb));
			
			// ...else reenable inactivity
			if (purple_prefs_get_bool(PREF_DISABLE_INACTIVITY))
				reenable_inactivity();
			
			purple_timeout_remove(timeout);
			timeout = 0;
		}
	}
}


static void
signed_on_cb(PurpleConnection *gc, gpointer data)
{
	PurpleAccount *account = NULL;
	
	purple_debug_info(PLUGIN_STATIC_NAME, "signed_on_cb\n");
	
    account = purple_connection_get_account(gc);
    
    if (is_jabber_account(account))
    {		
		// we try to add invisible status here because the user could have made
		// a new account when the plugin was already loaded
		add_status_invisible(account);
		
		if (is_status_invisible())
		{
			// this makes purple call the account_status_changed_cb that does
			// the rest of the initial work	
			purple_prpl_got_account_status(account, "pseudoinvisible", NULL);

			if (purple_account_is_status_active(account, "pseudoinvisible"))
				purple_debug_info(PLUGIN_STATIC_NAME, "signed_on_cb Pseudo Invisibile\n");
			else
				purple_debug_info(PLUGIN_STATIC_NAME, "signed_on_cb Other\n");
		}
	}
}


static gboolean
plugin_load (PurplePlugin *plugin)
{
	GList *accounts = NULL;
	PurpleAccount *account = NULL;
	this_plugin = plugin;
	gboolean warn = FALSE;

	purple_debug_info(PLUGIN_STATIC_NAME, "plugin_load\n");
	
	warn = FALSE;
	
	//add invisible status to all jabber accounts
	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next)
	{
		account = (PurpleAccount *) accounts->data;
		
		if (is_jabber_account(account))
		{
			add_status_invisible(account);
			
			if (purple_account_is_connected(account) && !warn)
			{
				purple_notify_warning(
					this_plugin,
					PLUGIN_NAME,
					"WARNING!!!",
					STRING_PLUGIN_LOAD_WARNING);
				warn = TRUE;
			}
		}
	}

	// if there's no Best Friends group create it
	if (purple_find_group(BEST_FRIENDS_GROUP) == NULL) 
		purple_blist_add_group(purple_group_new(BEST_FRIENDS_GROUP), NULL);

	purple_debug_info(PLUGIN_STATIC_NAME, "status type is %s\n",
		purple_primitive_get_name_from_type(
			purple_savedstatus_get_type(
				purple_savedstatus_get_default())));

	// connect callbacks to signals

	// signed on	
	purple_signal_connect(purple_connections_get_handle(),
        "signed-on",
        plugin,
        PURPLE_CALLBACK(signed_on_cb),
        NULL);

	// account status changed
	purple_signal_connect(purple_accounts_get_handle(),
		"account-status-changed",
		plugin,
		PURPLE_CALLBACK(account_status_changed_cb),
		NULL);

	return TRUE;
}


static gboolean
plugin_unload (PurplePlugin *plugin)
{
	GList *accounts = NULL;
	PurpleAccount *account = NULL;
	gboolean warn = FALSE;
	
	purple_debug_info(PLUGIN_STATIC_NAME, "plugin_unload\n");

	// restoring inactivity prefs and removing old values
	if (purple_prefs_get_bool(PREF_DISABLE_INACTIVITY))
		reenable_inactivity();

	purple_debug_info(PLUGIN_STATIC_NAME, "plugin_load\n");
	
	//notify client restart
	warn = FALSE;
	
	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next)
	{
		account = (PurpleAccount *) accounts->data;
		
		if (is_jabber_account(accounts->data))
		{
			if (purple_account_is_connected(account) && !warn)
			{
				purple_notify_warning(
					this_plugin,
					PLUGIN_NAME,
					"WARNING!!!",
					STRING_PLUGIN_UNLOAD_WARNING);
				warn = TRUE;
			}
		}
	}
	
	return TRUE;
}


static void
action_update_blist_cb(PurplePluginAction *action)
{
	GList *accounts = NULL;
	
	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next)
		if (is_jabber_account(accounts->data))
			update_account_blist(accounts->data);
}


static GList *
plugin_actions(PurplePlugin *plugin, gpointer context)
{
    GList *l = NULL;
    PurplePluginAction *action = NULL;
    
    action = purple_plugin_action_new("Update blist", action_update_blist_cb);
    
    l = g_list_append(l, action);
    
    return l;
}


static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_name_and_label(
				PREF_DISABLE_INACTIVITY,
				"Disable Idle Reporting and Auto Away (highly recommended)");
	purple_plugin_pref_frame_add(frame, pref);
	
	pref = purple_plugin_pref_new_with_name_and_label(PREF_UPDATE_TIMEOUT, "Resend presences interval (in seconds)");
	purple_plugin_pref_set_bounds(pref, PREF_TIMEOUT_MIN, PREF_TIMEOUT_MAX);
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}


static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};


/* For specific notes on the meanings of each of these members, consult the C Plugin Howto
 * on the website. */
static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	PLUGIN_ID,
	PLUGIN_NAME,
	PLUGIN_VERSION,
	PLUGIN_SUMMARY,
	PLUGIN_DESCRIPTION,
	PLUGIN_AUTHOR,
	PLUGIN_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	&prefs_info,
	plugin_actions,
	NULL,
	NULL,
	NULL,
	NULL
};


static void
init_plugin (PurplePlugin * plugin)
{
	// add plugin pref
	purple_prefs_add_none(PREF_PREFIX);

	// check inactivity pref
	if (!purple_prefs_exists(PREF_DISABLE_INACTIVITY))
		purple_prefs_add_bool(PREF_DISABLE_INACTIVITY, FALSE);

	// check update timeout pref
	if (!purple_prefs_exists(PREF_UPDATE_TIMEOUT))
		purple_prefs_add_int(PREF_UPDATE_TIMEOUT, PREF_UPDATE_TIMEOUT_DEFAULT);
}


PURPLE_INIT_PLUGIN (PLUGIN_ID, init_plugin, info)
