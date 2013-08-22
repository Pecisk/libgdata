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
	
	client = goa_client_new_sync(NULL, &error);
	g_assert_no_error(error);
	object = goa_client_lookup_by_id(client, "account_1377173897" /* Get this from seahorse */);
	
	authorizer = gdata_goa_authorizer_new(object);
	service = GDATA_SERVICE (gdata_tasks_service_new (GDATA_AUTHORIZER (authorizer)));
	feed = gdata_tasks_service_query_all_tasklists (GDATA_TASKS_SERVICE (service), NULL, NULL, NULL, NULL, &error);

	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);
	
	g_object_unref (service);
	g_object_unref (feed);

	return 0;

}
