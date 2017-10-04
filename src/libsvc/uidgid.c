/* uid/gid helpers */

#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "libsvc/uidgid.h"


/*
 * Resolving UIDs and GIDs:
 *
 * It is imperative that we check usernames before falling back to valid ids.  We also need to
 * validate the id string before accepting it as an id to avoid situations such as `0day' being
 * resolved as uid=0.
 *
 * See also <https://github.com/systemd/systemd/issues/6309> for an example of what happens when
 * proper validation is not done.
 */
static bool
id_validate(const char *data)
{
	for (const char *p = data; *p; p++)
		if (!isdigit(*p))
			return false;

	return true;
}


uid_t
uid_resolve(const char *username)
{
	struct passwd pwd;
	struct passwd *result;
	char buf[16384];

	getpwnam_r(username, &pwd, buf, sizeof buf, &result);
	if (result != NULL)
		return result->pw_uid;

	if (id_validate(username))
		return atoi(username);

	return -1;
}


gid_t
gid_resolve(const char *groupname)
{
	struct group grp;
	struct group *result;
	char buf[16384];

	getgrnam_r(groupname, &grp, buf, sizeof buf, &result);
	if (result != NULL)
		return result->gr_gid;

	if (id_validate(groupname))
		return atoi(groupname);

	return -1;
}
