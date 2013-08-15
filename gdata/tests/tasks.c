#include "gdata.h"
#include <glib.h>
#include <glib-object.h>
#include "common.h"

int main (int argc, char *argv[]) {
	GDataOAuth1Authorizer *authorizer = NULL;
	GDataService *service = NULL;
	GDataFeed *feed = NULL;
	GError *error = NULL;
	
	gdata_test_init (argc, argv);

	gchar *authentication_uri, *token, *token_secret;
	gchar verifier[100];
	authorizer = gdata_oauth1_authorizer_new ("Application name", GDATA_TYPE_TASKS_SERVICE);
	
	/* Get an authentication URI */
	authentication_uri = gdata_oauth1_authorizer_request_authentication_uri (authorizer, &token, &token_secret, NULL, NULL);
	g_assert (authentication_uri != NULL);
	
	printf ("URI %s\n", authentication_uri);
	
	g_free (authentication_uri);
	
	/* Read verifier token pasted by user */
	scanf ("%s", verifier);
	
	/* Authorise the token */
	g_assert (gdata_oauth1_authorizer_request_authorization (authorizer, token, token_secret, verifier, NULL, NULL) == TRUE);

	service = GDATA_SERVICE (gdata_tasks_service_new (GDATA_AUTHORIZER (authorizer)));
	feed = gdata_tasks_service_query_all_tasklists (GDATA_TASKS_SERVICE (service), NULL, NULL, NULL, NULL, &error);

	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);
	
	g_object_unref (service);
	g_object_unref (feed);

	return 0;

}
