#include "gdata.h"
#include <glib.h>
#include <glib-object.h>
#include "common.h"

int main (int argc, char *argv[]) {
	GDataAuthorizer *authorizer = NULL;
	GDataService *service = NULL;
	GDataFeed *feed = NULL;
	GError *error = NULL;
		
	gdata_test_init (argc, argv);
	authorizer = GDATA_AUTHORIZER (gdata_client_login_authorizer_new (CLIENT_ID, GDATA_TYPE_TASKS_SERVICE));
	gdata_client_login_authorizer_authenticate (GDATA_CLIENT_LOGIN_AUTHORIZER (authorizer), USERNAME, PASSWORD, NULL, NULL);

	service = GDATA_SERVICE (gdata_tasks_service_new (authorizer));
		
	feed = gdata_tasks_service_query_all_tasklists (GDATA_TASKS_SERVICE (service), NULL, NULL, NULL, NULL, &error);

	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);
	
	g_object_unref (service);
	g_object_unref (feed);

	return 0;

}
