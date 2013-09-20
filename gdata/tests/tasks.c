#include "gdata.h"
#include <glib.h>
#include <glib-object.h>
#include "common.h"
#include "gdata-goa-authorizer.h"

int main (int argc, char *argv[]) {
	//GDataOAuth1Authorizer *authorizer = NULL;
	GDataService *service = NULL;
	GDataFeed *feed = NULL;
	GError *error = NULL;
	GoaClient *client;
	GoaObject *object;
	GDataGoaAuthorizer *authorizer;
	
	gdata_test_init (argc, argv);
	
	client = goa_client_new_sync (NULL, &error);
	g_assert_no_error (error);
	object = goa_client_lookup_by_id (client, "account_1377173897" /* Get this from seahorse */);
	
	authorizer = gdata_goa_authorizer_new (object);
	service = GDATA_SERVICE (gdata_tasks_service_new (GDATA_AUTHORIZER (authorizer)));
	feed = gdata_tasks_service_query_all_tasklists (GDATA_TASKS_SERVICE (service), NULL, NULL, NULL, NULL, &error);
	GList *entries = gdata_feed_get_entries (feed);
	
	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);
	
	GList *entries_back = entries;
	
	//while (entries != NULL) {
		//GDataTasksTasklist *tasklist = GDATA_TASKS_TASKLIST (entries->data);
		//const gchar *title = gdata_entry_get_title (GDATA_ENTRY (tasklist));
		//const gchar *id = gdata_entry_get_id (GDATA_ENTRY (tasklist));
		//guint64 updated = gdata_entry_get_updated (GDATA_ENTRY (tasklist));
		
		//printf("Tasklist %s\n%s %"G_GUINT64_FORMAT"\n", title, id, updated);
		
		///* printing out tasks */
		//GDataFeed *feed2 = NULL;
		//feed2 = gdata_tasks_service_query_tasks (GDATA_TASKS_SERVICE (service), tasklist, NULL, NULL, NULL, NULL, &error);
		//GList *entries2 = gdata_feed_get_entries (feed2);
		//guint size = g_list_length (entries2);
		//printf("Size of tasklist: %i\n", size);
		///* reading tasks in loop */
		//while (entries2 != NULL) {
			//GDataTasksTask *task = GDATA_TASKS_TASK (entries2->data);
			//const gchar *title = gdata_entry_get_title (GDATA_ENTRY (task));
			//const gchar *notes = gdata_tasks_task_get_notes (GDATA_TASKS_TASK (task));
			//printf("%s\n%s\n--------------------\n", title, notes);
			//entries2 = entries2->next;
		//}
		//g_object_unref (feed2);
		//g_list_free (entries2);
		//entries = entries->next;
	//}

	GDataTasksTask *new_task = gdata_tasks_task_new (NULL);
	gdata_entry_set_title (GDATA_ENTRY (new_task), "Test task");
	gdata_tasks_task_set_notes (new_task, "Note to read");
	
	GDataTasksTask *new_task_returned = gdata_tasks_service_insert_task (GDATA_TASKS_SERVICE (service), new_task, GDATA_TASKS_TASKLIST (entries_back->data), NULL, NULL);
	gdata_entry_set_title (GDATA_ENTRY (new_task_returned), "Test task for real");
	GDataTasksTask *new_task_updated = gdata_tasks_service_update_task (GDATA_TASKS_SERVICE (service), new_task_returned, NULL, error);
	gdata_tasks_service_delete_task (GDATA_TASKS_SERVICE (service), new_task_updated, NULL, NULL);
	g_object_unref (new_task_returned);
	g_object_unref (service);
	g_object_unref (feed);

	return 0;

}
