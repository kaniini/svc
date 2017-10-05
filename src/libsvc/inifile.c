/* INI file parser - parses INI files to nvlists */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <nv.h>

#include "libsvc/inifile.h"


static bool
value_is_numeric(const char *data)
{
	for (const char *p = data; *p; p++)
		if (!isdigit(*p))
			return false;

	return true;
}


nvlist_t *
inifile_parse(const char *path)
{
	FILE *f;
	nvlist_t *out, *section = NULL;
	char buffer[4096], *tmp, *section_name;

	f = fopen(path, "rb");
	if (f == NULL)
		return NULL;

	out = nvlist_create(0);

	while (fgets(buffer, sizeof buffer, f))
	{
		if (buffer[0] == '[' && (tmp = strchr(buffer, ']')) != NULL)
		{
			*tmp = 0;

			if (section != NULL && section_name != NULL)
			{
				nvlist_add_nvlist(out, section_name, section);
				nvlist_destroy(section);
				free(section_name);
			}

			section = nvlist_create(NV_FLAG_NO_UNIQUE);
			section_name = strdup(&buffer[1]);
		}
		else if (buffer[0] != '#' && section != NULL && (tmp = strchr(buffer, '=')) != NULL)
		{
			const char *key, *value;
			char *end;

			*tmp = 0;

			key = buffer;
			value = tmp + 1;

			end = strchr(tmp + 1, '\n');
			if (end != NULL)
				*end = 0;

			end = strchr(tmp + 1, '\r');
			if (end != NULL)
				*end = 0;

			if (!value_is_numeric(value))
				nvlist_add_string(section, key, value);
			else
			{
				uint64_t number;

				number = strtol(value, NULL, 10);
				nvlist_add_number(section, key, number);
			}
		}
	}

	if (section != NULL && section_name != NULL)
	{
		nvlist_add_nvlist(out, section_name, section);
		nvlist_destroy(section);
		free(section_name);
	}

	fclose(f);
	return out;
}
